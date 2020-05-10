// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_SERVER_ERR_H__
#define __PUIC_SERVER_ERR_H__

#ifdef __cplusplus
#include <cstdio>
extern "C" {
#else
#include <stdio.h>
#endif

// api error.
#define PUISRVERR_SUCCESS  0
#define PUISRVERR_EOF      EOF

#define PUISRVERR_UNKNOWN  -50001
#define PUISRVERR_PARAMS   -50002
#define PUISRVERR_FIN      -50003

#define PUISRVERR_STATE    -50101
#define PUISRVERR_UDP      -50102

// connection error.
#define PUICCONN_NO_ERROR  0 // QuicErrorCode

// stream error.
#define PUICSTRM_NO_ERROR  0 // QuicRstStreamErrorCode

#ifdef __cplusplus
}
#endif

#endif // __PUIC_SERVER_ERR_H__
