// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_CLIENT_DEF_H__
#define __PUIC_CLIENT_DEF_H__

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
extern "C" {
#else
#include <stddef.h>
#include <stdint.h>
#endif

typedef void* puicclient_dialer;

typedef void* puicclient_conn;

typedef struct _PUICCLIENT_ConnsCallbaks
{
    void (*OnConnected) (void* userData, puicclient_conn conn);

    void (*OnCanRead) (void* userData, puicclient_conn conn);

    void (*OnCanWrite) (void* userData, puicclient_conn conn, size_t remain);

    // Do not access conn after OnDisconnected is called.
    void (*OnDisconnected) (void* userData, puicclient_conn conn, int connErr, bool fromRemote, const char* details);

} PUICCLIENT_ConnsCallbaks;

#ifdef __cplusplus
}
#endif

#endif // __PUIC_CLIENT_DEF_H__
