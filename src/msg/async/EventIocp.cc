// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2014 UnitedStack <haomai@unitedstack.com>
 *
 * Author: Haomai Wang <haomaiwang@gmail.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#include "common/errno.h"
//#include <fcntl.h>
//#include "EventEpoll.h"
//#include "mingw/mingw-w64-headers/include/winsock2.h"

#include "EventIocp.h"
#define dout_subsys ceph_subsys_ms

#undef dout_prefix
#define dout_prefix *_dout << "IocpDriver."

int IocpDriver::init(EventCenter *c, int nevent)
{
	notifications = (OVERLAPPED_ENTRY*)calloc(nevent, sizeof(OVERLAPPED_ENTRY));	
  	if (!notifications) {
  	  lderr(cct) << __func__ << " unable to malloc memory. " << dendl;
  	  return -ENOMEM;
  	}
	iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    	if (iocp == NULL) {
		int e = GetLastError();
		lderr(cct) << __func__ << " unable to create IO Completion"
			<< " port: " << e << dendl;
		return -e;
	}
	this->nevent = nevent;
  	return 0;
}

int IocpDriver::add_event(int fd, int cur_mask, int add_mask)
{
	SOCK_NOTIFY_REGISTRATION snr = {};
	snr.operation = SOCK_NOTIFY_OP_ENABLE;
	snr.triggerFlags = SOCK_NOTIFY_TRIGGER_EDGE;
	add_mask |= cur_mask;
	if (add_mask & EVENT_READABLE) {
		snr.eventFilter |= SOCK_NOTIFY_REGISTER_EVENT_IN;
	}
	if (add_mask & EVENT_WRITABLE) {
		snr.eventFilter |= SOCK_NOTIFY_REGISTER_EVENT_OUT;
	}
	snr.socket = fd;
	snr.completionKey = (PVOID)fd;

	int err;
	err = ProcessSocketNotifications(iocp, 1, &snr, 0, 0, NULL, NULL);
	if (err != ERROR_SUCCESS) {
    		lderr(cct) << __func__ << " ProcessSocketNotifications: add fd=" << fd << " failed. "
    		           << cpp_strerror(err) << dendl;
		return -err;
	}
	err = snr.registrationResult;
	if (err != ERROR_SUCCESS) {
    		lderr(cct) << __func__ << " ProcessSocketNotifications: add fd=" << fd << " failed. "
    		           << cpp_strerror(err) << dendl;
		return -err;
	}

  	return 0;
}

int IocpDriver::del_event(int fd, int cur_mask, int delmask)
{
	SOCK_NOTIFY_REGISTRATION snr = {};
	snr.eventFilter = SOCK_NOTIFY_REGISTER_EVENT_NONE;
	snr.socket = fd;
	snr.completionKey = (PVOID)fd;

	int mask = cur_mask & (~delmask);
	if (mask != EVENT_NONE) {
		snr.operation = SOCK_NOTIFY_OP_ENABLE;
		snr.triggerFlags = SOCK_NOTIFY_TRIGGER_EDGE;
		if (mask & EVENT_READABLE) {
			snr.eventFilter |= SOCK_NOTIFY_REGISTER_EVENT_IN;
		}
		if (mask & EVENT_WRITABLE) {
			snr.eventFilter |= SOCK_NOTIFY_REGISTER_EVENT_OUT;
		}
	} else {
		snr.operation = SOCK_NOTIFY_OP_REMOVE;
	}

	int err;
	err = ProcessSocketNotifications(iocp, 1, &snr, 0, 0, NULL, NULL);
	if (err != ERROR_SUCCESS) {
    		lderr(cct) << __func__ << " ProcessSocketNotifications: del fd=" << fd << " failed. "
    		           << cpp_strerror(err) << dendl;
		return -err;
	}
	err = snr.registrationResult;
	if (err != ERROR_SUCCESS) {
    		lderr(cct) << __func__ << " ProcessSocketNotifications: del fd=" << fd << " failed. "
    		           << cpp_strerror(err) << dendl;
		return -err;
	}
  	return 0;
}

int IocpDriver::resize_events(int newsize)
{
  return 0;
}

int IocpDriver::event_wait(std::vector<FiredFileEvent> &fired_events, struct timeval *tvp)
{
  //int retval, numevents = 0;
  int numevents = 0;
	bool ret;
	ret = GetQueuedCompletionStatusEx(iocp, notifications, nevent,
		&numevents, tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1,
		false);
	if (ret == true) {
		fired_events.resize(numevents);
		for (int event_id = 0; event_id < numevents; event_id++) {
			int mask = 0;
			OVERLAPPED_ENTRY *o = &notifications[event_id];
			int events = SocketNotificationRetrieveEvents(o);
  			if (events & SOCK_NOTIFY_EVENT_IN) mask |= EVENT_READABLE;
  			if (events & SOCK_NOTIFY_EVENT_OUT) mask |= EVENT_WRITABLE;
  			if (events & SOCK_NOTIFY_EVENT_ERR) mask |= EVENT_READABLE|EVENT_WRITABLE;
  			if (events & SOCK_NOTIFY_EVENT_HANGUP) mask |= EVENT_READABLE|EVENT_WRITABLE;
  			fired_events[event_id].fd = o->lpCompletionKey;
  			fired_events[event_id].mask = mask;
		}
	}
  return numevents;
}
