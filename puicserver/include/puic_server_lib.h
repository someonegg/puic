// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_SERVER_LIB_H__
#define __PUIC_SERVER_LIB_H__

#include <uv.h>
#include "puic_server_def.h"
#include "puic_server_err.h"

#if defined(PUICSERVER_SHARED_LIBRARY)

#if defined(_WIN32)

#if defined(PUICSERVER_IMPLEMENTATION)
#define PUICSERVER_EXPORT __declspec(dllexport)
#else
#define PUICSERVER_EXPORT __declspec(dllimport)
#endif

#else  /* defined(_WIN32) */

#if defined(PUICSERVER_IMPLEMENTATION)
#define PUICSERVER_EXPORT __attribute__((visibility("default")))
#else
#define PUICSERVER_EXPORT
#endif

#endif  /* defined(_WIN32) */

#else  /* defined(PUICSERVER_SHARED_LIBRARY) */

#define PUICSERVER_EXPORT

#endif  /* defined(PUICSERVER_SHARED_LIBRARY) */

#ifdef __cplusplus
extern "C" {
#endif

PUICSERVER_EXPORT int PUICSERVER_CreateListener(
    uv_loop_t* loop,
    const char* serverIp, int serverPort,
    puicserver_listener* pListener
    );

PUICSERVER_EXPORT int PUICSERVER_ListenerStart(
    puicserver_listener listener,
    const PUICSERVER_ConnsCallbaks* callbacks,
    void* userData
    );

// Will close all associated conns too.
PUICSERVER_EXPORT void PUICSERVER_ListenerClose(
    puicserver_listener listener
    );

PUICSERVER_EXPORT void* PUICSERVER_ConnData(
    puicserver_conn conn
    );

PUICSERVER_EXPORT void PUICSERVER_SetConnData(
    puicserver_conn conn,
    void* data
    );

PUICSERVER_EXPORT int PUICSERVER_ConnReadableRegions(
    puicserver_conn conn,
    const char* regs[], size_t lens[], int* count
    );

PUICSERVER_EXPORT int PUICSERVER_ConnMarkConsumed(
    puicserver_conn conn,
    size_t bytes
    );

PUICSERVER_EXPORT int PUICSERVER_ConnWrite(
    puicserver_conn conn,
    const char* data, size_t len, bool fin,
    size_t* buffered
    );

PUICSERVER_EXPORT void PUICSERVER_ConnDisconnect(
    puicserver_conn conn
    );

#ifdef __cplusplus
}
#endif

#endif // __PUIC_SERVER_LIB_H__
