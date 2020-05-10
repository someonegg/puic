// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_CLIENT_ERR_H__
#define __PUIC_CLIENT_ERR_H__

#ifdef __cplusplus
#include <cstdio>
extern "C" {
#else
#include <stdio.h>
#endif

// api error.
#define PUICLIERR_SUCCESS  0
#define PUICLIERR_EOF      EOF

#define PUICLIERR_UNKNOWN  -51001
#define PUICLIERR_PARAMS   -51002
#define PUICLIERR_FIN      -51003

#define PUICLIERR_STATE    -51101
#define PUICLIERR_UDP      -51102

// connection error.
#define PUICCONN_NO_ERROR  0 // QuicErrorCode

// stream error.
#define PUICSTRM_NO_ERROR  0 // QuicRstStreamErrorCode

#ifdef __cplusplus
}
#endif

#endif // __PUIC_CLIENT_ERR_H__
