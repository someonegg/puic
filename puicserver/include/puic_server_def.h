// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_SERVER_DEF_H__
#define __PUIC_SERVER_DEF_H__

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
extern "C" {
#else
#include <stddef.h>
#include <stdint.h>
#endif

typedef void* puicserver_listener;

typedef void* puicserver_conn;

typedef struct _PUICSERVER_ConnsCallbaks
{
	void (*OnPreAccept) (void* userData, const char* clientIp, int clientPort);

    void (*OnAccept) (void* userData, puicserver_conn conn, const char* clientIp, int clientPort);

    void (*OnCanRead) (void* userData, puicserver_conn conn);

    void (*OnCanWrite) (void* userData, puicserver_conn conn, size_t remain);

    // Do not access conn after OnDisconnected is called.
    void (*OnDisconnected) (void* userData, puicserver_conn conn, int connErr, bool fromRemote, const char* details);

} PUICSERVER_ConnsCallbaks;

#ifdef __cplusplus
}
#endif

#endif // __PUIC_SERVER_DEF_H__
