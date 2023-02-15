// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2023 Rafael Lopez <rafael.lopez@canonical.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef CEPH_MSG_EVENTEPOLL_H
#define CEPH_MSG_EVENTEPOLL_H

#include <unistd.h>
//#include <sys/epoll.h>
#include "mingw/mingw-w64-headers/include/windows.h"
#include "mingw/mingw-w64-headers/include/winbase.h"
#include "mingw/mingw-w64-headers/include/winsock2.h"

#include "Event.h"

class IocpDriver : public EventDriver {
//  int epfd;
//  struct epoll_event *events;
//  CephContext *cct;
	HANDLE iocp;
	//SOCK_NOTIFY_REGISTRATION *events;
	OVERLAPPED_ENTRY *notifications;
	CephContext *cct;
	int nevent;	
	

 public:
  explicit IocpDriver(CephContext *c): iocp(NULL), notifications(NULL), cct(c), nevent(0) {}
  ~IocpDriver() override {
    if (iocp != NULL)
      CloseHandle(iocp);

    if (notifications)
      free(notifications);
  }

  int init(EventCenter *c, int nevent) override;
  int add_event(int fd, int cur_mask, int add_mask) override;
  int del_event(int fd, int cur_mask, int del_mask) override;
  int resize_events(int newsize) override;
  int event_wait(std::vector<FiredFileEvent> &fired_events,
		 struct timeval *tp) override;
};

#endif
