// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_CLIENT_LIB_H__
#define __PUIC_CLIENT_LIB_H__

#include <uv.h>
#include "puic_client_def.h"
#include "puic_client_err.h"

#if defined(PUICCLIENT_SHARED_LIBRARY)

#if defined(_WIN32)

#if defined(PUICCLIENT_IMPLEMENTATION)
#define PUICCLIENT_EXPORT __declspec(dllexport)
#else
#define PUICCLIENT_EXPORT __declspec(dllimport)
#endif

#else  /* defined(_WIN32) */

#if defined(PUICCLIENT_IMPLEMENTATION)
#define PUICCLIENT_EXPORT __attribute__((visibility("default")))
#else
#define PUICCLIENT_EXPORT
#endif

#endif  /* defined(_WIN32) */

#else  /* defined(PUICCLIENT_SHARED_LIBRARY) */

#define PUICCLIENT_EXPORT

#endif  /* defined(PUICCLIENT_SHARED_LIBRARY) */

#ifdef __cplusplus
extern "C" {
#endif

PUICCLIENT_EXPORT int PUICCLIENT_CreateDialer(
    uv_loop_t* loop,
    const char* bindIp, int bindPort,
    puicclient_dialer* pDialer
    );

PUICCLIENT_EXPORT int PUICCLIENT_DialerStart(
    puicclient_dialer dialer,
    const PUICCLIENT_ConnsCallbaks* callbacks,
    void* userData
    );

// Will close all associated conns too.
PUICCLIENT_EXPORT void PUICCLIENT_DialerClose(
    puicclient_dialer dialer
    );

PUICCLIENT_EXPORT int PUICCLIENT_CreateConn(
    puicclient_dialer dialer,
    const char* serverHost,
    const char* serverIp, int serverPort,
    puicclient_conn* pConn
    );

PUICCLIENT_EXPORT void* PUICCLIENT_ConnData(
    puicclient_conn conn
    );

PUICCLIENT_EXPORT void PUICCLIENT_SetConnData(
    puicclient_conn conn,
    void* data
    );

PUICCLIENT_EXPORT void PUICCLIENT_ConnConnect(
    puicclient_conn conn
    );

PUICCLIENT_EXPORT int PUICCLIENT_ConnReadableRegions(
    puicclient_conn conn,
    const char* regs[], size_t lens[], int* count
    );

PUICCLIENT_EXPORT int PUICCLIENT_ConnMarkConsumed(
    puicclient_conn conn,
    size_t bytes
    );

PUICCLIENT_EXPORT int PUICCLIENT_ConnWrite(
    puicclient_conn conn,
    const char* data, size_t len, bool fin,
    size_t* buffered
    );

PUICCLIENT_EXPORT void PUICCLIENT_ConnDisconnect(
    puicclient_conn conn
    );

#ifdef __cplusplus
}
#endif

#endif // __PUIC_CLIENT_LIB_H__
