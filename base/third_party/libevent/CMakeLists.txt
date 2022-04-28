# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

add_library(basium_libevent STATIC
  buffer.c
  evbuffer.c
  evdns.c
  evdns.h
  event-config.h
  event-internal.h
  event.c
  event.h
  event_tagging.c
  evhttp.h
  evrpc-internal.h
  evrpc.c
  evrpc.h
  evsignal.h
  evutil.c
  evutil.h
  http-internal.h
  http.c
  log.c
  log.h
  min_heap.h
  poll.c
  select.c
  signal.c
  strlcpy-internal.h
  strlcpy.c)

target_compile_definitions(basium_libevent PRIVATE HAVE_CONFIG_H)

if(APPLE)
  target_sources(basium_libevent PRIVATE
    kqueue.c
    mac/config.h
    mac/event-config.h)
  target_include_directories(basium_libevent PUBLIC mac)
elseif(LINUX OR CHROMEOS)
  target_sources(basium_libevent PRIVATE
    epoll.c
    linux/config.h
    linux/event-config.h)
  target_include_directories(basium_libevent PUBLIC linux)
elseif(ANDROID)
  target_sources(basium_libevent PRIVATE
    android/config.h
    android/event-config.h
    epoll.c)
  target_include_directories(basium_libevent PUBLIC android)
endif()
