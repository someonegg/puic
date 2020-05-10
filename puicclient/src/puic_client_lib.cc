// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic_client_lib.h"

#include <string>
#include "puic_client.h"

extern "C" int PUICCLIENT_CreateDialer(
    uv_loop_t* loop,
    const char* bindIp, int bindPort,
    puicclient_dialer* pDialer
    )
{
    sockaddr_storage bindAddr;
    if (uv_ip4_addr(bindIp, bindPort, (sockaddr_in*)&bindAddr) != 0 &&
        uv_ip6_addr(bindIp, bindPort, (sockaddr_in6*)&bindAddr) != 0)
    {
        return PUICLIERR_PARAMS;
    }

    *pDialer = new net::PuicDialer(loop, bindAddr);

    return PUICLIERR_SUCCESS;
}

extern "C" int PUICCLIENT_DialerStart(
    puicclient_dialer dialer,
    const PUICCLIENT_ConnsCallbaks* callbacks,
    void* userData
    )
{
    auto dia = (net::PuicDialer*)dialer;
    return dia->Start(*callbacks, userData);
}

extern "C" void PUICCLIENT_DialerClose(puicclient_dialer dialer)
{
    auto dia = (net::PuicDialer*)dialer;
    dia->Stop();
    delete dia;
}

extern "C" int PUICCLIENT_CreateConn(
    puicclient_dialer dialer,
    const char* serverHost,
    const char* serverIp, int serverPort,
    puicclient_conn* pConn
    )
 {
    auto dia = (net::PuicDialer*)dialer;

    sockaddr_storage serverAddr;
    if (uv_ip4_addr(serverIp, serverPort, (sockaddr_in*)&serverAddr) != 0 &&
        uv_ip6_addr(serverIp, serverPort, (sockaddr_in6*)&serverAddr) != 0)
    {
        return PUICLIERR_PARAMS;
    }

    auto conn = dia->CreateConn(std::string(serverHost), serverAddr);
    if (conn == nullptr)
    {
        return PUICLIERR_UNKNOWN;
    }

    *pConn = conn;
    return PUICLIERR_SUCCESS;
 }

extern "C" void* PUICCLIENT_ConnData(puicclient_conn conn)
{
    return ((net::PuicClientConn*)conn)->Data();
}

extern "C" void PUICCLIENT_SetConnData(
    puicclient_conn conn,
    void* data
    )
{
    ((net::PuicClientConn*)conn)->SetData(data);
}

extern "C" void PUICCLIENT_ConnConnect(
    puicclient_conn conn
    )
{
    auto session = ((net::PuicClientConn*)conn)->session();
    session->StartCryptoHandshake();
}

extern "C" int PUICCLIENT_ConnReadableRegions(
    puicclient_conn conn,
    const char* regs[], size_t lens[], int* count
    )
{
    auto session = ((net::PuicClientConn*)conn)->session();
    auto stream = session->PresetStream();

    stream->GetReadableRegions(regs, lens, count);

    if (*count == 0 && stream->IsEOF())
        return PUICLIERR_EOF;

    return PUICLIERR_SUCCESS;
}

extern "C" int PUICCLIENT_ConnMarkConsumed(
    puicclient_conn conn,
    size_t bytes
    )
{
    auto session = ((net::PuicClientConn*)conn)->session();
    auto stream = session->PresetStream();

    stream->MarkConsumed(bytes);

    return PUICLIERR_SUCCESS;
}

extern "C" int PUICCLIENT_ConnWrite(
    puicclient_conn conn,
    const char* data, size_t len, bool fin,
    size_t* buffered
    )
{
    auto session = ((net::PuicClientConn*)conn)->session();
    auto stream = session->PresetStream();

    if (stream->FinSent())
        return PUICLIERR_FIN;

    stream->Write(data, len, fin);

    *buffered = stream->WriteBufferedAmount();

    return PUICLIERR_SUCCESS;
}

extern "C" void PUICCLIENT_ConnDisconnect(puicclient_conn conn)
{
    auto session = ((net::PuicClientConn*)conn)->session();
    session->Close();
}
