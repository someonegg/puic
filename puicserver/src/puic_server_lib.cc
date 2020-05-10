// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic_server_lib.h"

#include "puic_server.h"

extern "C" int PUICSERVER_CreateListener(
    uv_loop_t* loop,
    const char* serverIp, int serverPort,
    puicserver_listener* pListener
    )
{
    sockaddr_storage serverAddr;
    if (uv_ip4_addr(serverIp, serverPort, (sockaddr_in*)&serverAddr) != 0 &&
        uv_ip6_addr(serverIp, serverPort, (sockaddr_in6*)&serverAddr) != 0)
    {
        return PUISRVERR_PARAMS;
    }

    *pListener = new net::PuicListener(loop, serverAddr);

    return PUISRVERR_SUCCESS;
}

extern "C" int PUICSERVER_ListenerStart(
    puicserver_listener listener,
    const PUICSERVER_ConnsCallbaks* callbacks,
    void* userData
    )
{
    auto lis = (net::PuicListener*)listener;
    return lis->Start(*callbacks, userData);
}

extern "C" void PUICSERVER_ListenerClose(puicserver_listener listener)
{
    auto lis = (net::PuicListener*)listener;
    lis->Stop();
    delete lis;
}

extern "C" void* PUICSERVER_ConnData(puicserver_conn conn)
{
    return ((net::PuicServerConn*)conn)->Data();
}

extern "C" void PUICSERVER_SetConnData(
    puicserver_conn conn,
    void* data
    )
{
    ((net::PuicServerConn*)conn)->SetData(data);
}

extern "C" int PUICSERVER_ConnReadableRegions(
    puicserver_conn conn,
    const char* regs[], size_t lens[], int* count
    )
{
    auto session = ((net::PuicServerConn*)conn)->session();
    auto stream = session->PresetStream();

    stream->GetReadableRegions(regs, lens, count);

    if (*count == 0 && stream->IsEOF())
        return PUISRVERR_EOF;

    return PUISRVERR_SUCCESS;
}

extern "C" int PUICSERVER_ConnMarkConsumed(
    puicserver_conn conn,
    size_t bytes
    )
{
    auto session = ((net::PuicServerConn*)conn)->session();
    auto stream = session->PresetStream();

    stream->MarkConsumed(bytes);

    return PUISRVERR_SUCCESS;
}

extern "C" int PUICSERVER_ConnWrite(
    puicserver_conn conn,
    const char* data, size_t len, bool fin,
    size_t* buffered
    )
{
    auto session = ((net::PuicServerConn*)conn)->session();
    auto stream = session->PresetStream();

    if (stream->FinSent())
        return PUISRVERR_FIN;

    stream->Write(data, len, fin);

    *buffered = stream->WriteBufferedAmount();

    return PUISRVERR_SUCCESS;
}

extern "C" void PUICSERVER_ConnDisconnect(puicserver_conn conn)
{
    auto session = ((net::PuicServerConn*)conn)->session();
    session->Close();
}
