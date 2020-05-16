// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "agent_server.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <spdlog/spdlog.h>

AgentConn::AgentConn(AgentServer &server)
    : m_server(server)
    , m_callback(nullptr)
    , m_reading(false)
    , m_readeof(false)
    , m_writing(false)
    , m_writeof(false)
    , m_closed(false)
    , m_uvclosed(false)
{
}

AgentConn::~AgentConn()
{
    assert(m_callback == nullptr);
}

AgentConn* AgentConn::Create(AgentServer &server)
{
    return new AgentConn(server);
}

void AgentConn::Accept()
{
    auto logger = spdlog::get(Agent_LOGGER);

    uv_tcp_init(m_server.loop(), &m_socket);
    uv_tcp_nodelay(&m_socket, 1);
    m_socket.data = this;

    int r = uv_accept(m_server.listener(), (uv_stream_t*)&m_socket);
    if (r != 0)
    {
        logger->error("agent client accept failed, code={0:d}", r);
        m_closed = true;
        close();
        return;
    }
    else
    {
        int delay = 1; // 1s
        uv_tcp_keepalive(&m_socket, 1, delay);
        int rcv_size = 512 * 1024; // 512k
        uv_recv_buffer_size((uv_handle_t*)&m_socket, &rcv_size);
        int snd_size = 512 * 1024; // 512k
        uv_send_buffer_size((uv_handle_t*)&m_socket, &snd_size);
    }

    client_handshaked();
}

void AgentConn::Proxy(Callback* cb)
{
    m_callback = cb;
}

void AgentConn::Close()
{
    m_closed = true;
    if (m_uvclosed)
    {
        delete this;
        return;
    }
    close();
}

void AgentConn::StartRead()
{
    if (reading() || m_readeof)
        return;

    int r = uv_read_start((uv_stream_t*)&m_socket, on_uv_alloc, on_uv_read);
    if (r != 0)
    {
        m_callback->OnAgentConnErr("tcp_read", r);
        return;
    }
    m_reading = true;
}

void AgentConn::StopRead()
{
    if (!reading() || m_readeof)
        return;

    uv_read_stop((uv_stream_t*)&m_socket);
    m_reading = false;
}

void AgentConn::Write(const uv_buf_t bufs[], int nbufs, size_t len)
{
    assert(!writing());
    if (writing() || len == 0 || m_writeof)
        return;

    m_write.data = (void*)len;
    int r = uv_write(&m_write, (uv_stream_t*)&m_socket, bufs, nbufs, on_uv_write);
    if (r != 0)
    {
        m_callback->OnAgentConnErr("tcp_write", r);
        return;
    }
    m_writing = true;
}

void AgentConn::Shutdown()
{
    if (m_writeof)
        return;

    uv_shutdown(&m_shutdown, (uv_stream_t*)&m_socket, on_uv_shutdown);
}

void AgentConn::close()
{
    if (uv_is_closing((uv_handle_t*)&m_socket))
        return;
    if (m_callback != nullptr)
        m_callback->OnAgentConnClosed();
    uv_close((uv_handle_t*)&m_socket, on_uv_close);
}

static void get_sockaddr_name(sockaddr_storage &ss, char* ip, size_t len, int &port)
{
    if (ss.ss_family == AF_INET)
    {
        sockaddr_in* in = (sockaddr_in*)&ss;
        uv_ip4_name(in, ip, len);
        port = ntohs(in->sin_port);
    }
    else if (ss.ss_family == AF_INET6)
    {
        sockaddr_in6* in6 = (sockaddr_in6*)&ss;
        uv_ip6_name(in6, ip, len);
        port = ntohs(in6->sin6_port);
    }
    else
    {
        assert("unknown ss_family" == 0);
    }
}

void AgentConn::client_handshaked()
{
    auto logger = spdlog::get(Agent_LOGGER);

    char srcIp[64] = {0}; int srcPort = 0;
    {
        sockaddr_storage ss = {0};
        int len = sizeof(ss);
        uv_tcp_getpeername(&m_socket, (sockaddr*)&ss, &len);
        get_sockaddr_name(ss, srcIp, sizeof(srcIp), srcPort);
    }

    logger->info("agent client handshaked, client={0}:{1:d}", srcIp, srcPort);
    m_server.callback()->OnAgentConnAccept(this, srcIp, srcPort);
}

void AgentConn::on_uv_close(uv_handle_t* handle)
{
    AgentConn* pThis = (AgentConn*)handle->data;
    pThis->m_callback = nullptr;
    pThis->m_reading = false;
    pThis->m_readeof = true;
    pThis->m_writing = false;
    pThis->m_writeof = true;
    pThis->m_uvclosed = true;
    if (pThis->m_closed)
        delete pThis;
}

void AgentConn::on_uv_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    if (uv_is_closing((uv_handle_t*)handle))
    {
        *buf = uv_buf_init(nullptr, 0);
        return;
    }

    AgentConn* pThis = (AgentConn*)handle->data;
    *buf = uv_buf_init(pThis->m_readBuf, TCPCONN_READBUF_SIZE);
}

void AgentConn::on_uv_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf)
{
    if (uv_is_closing((uv_handle_t*)handle))
        return;

    AgentConn* pThis = (AgentConn*)handle->data;

    if (nread > 0)
    {
        pThis->m_callback->OnAgentConnRcvd(buf->base, nread);
        return;
    }

    if (nread == UV_EOF)
    {
        pThis->m_callback->OnAgentConnEOF();
        pThis->m_reading = false;
        pThis->m_readeof = true;
        if (pThis->m_writeof)
            pThis->close();
        return;
    }

    if (nread < 0)
    {
        pThis->m_callback->OnAgentConnErr("tcp_read", nread);
        return;
    }
}

void AgentConn::on_uv_write(uv_write_t* req, int status)
{
    uv_stream_t* handle = req->handle;
    if(uv_is_closing((uv_handle_t*)handle))
        return;

    AgentConn* pThis = (AgentConn*)handle->data;

    if (status != 0)
    {
        pThis->m_callback->OnAgentConnErr("tcp_write", status);
        return;
    }

    assert(pThis->writing());
    pThis->m_writing = false;

    size_t len = (size_t)req->data;
    pThis->m_callback->OnAgentConnSent(len);
}

void AgentConn::on_uv_shutdown(uv_shutdown_t* req, int status)
{
    uv_stream_t* handle = req->handle;
    if(uv_is_closing((uv_handle_t*)handle))
        return;

    AgentConn* pThis = (AgentConn*)handle->data;

    if (status != 0)
    {
        pThis->m_callback->OnAgentConnErr("tcp_shutdown", status);
        return;
    }

    pThis->m_writing = false;
    pThis->m_writeof = true;
    if (pThis->m_readeof)
        pThis->close();
}

AgentServer::AgentServer(uv_loop_t* loop, const sockaddr_storage &lisSS)
    : m_loop(loop)
    , m_listenSS(lisSS)
    , m_listener(nullptr)
    , m_callback(nullptr)
{
}

AgentServer::~AgentServer()
{
    assert(m_callback == nullptr);
}

bool AgentServer::Start(Callback* cb)
{
    auto logger = spdlog::get(Agent_LOGGER);

    m_listener = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(m_loop, m_listener);
    m_listener->data = this;

    int r = uv_tcp_bind(m_listener, (const struct sockaddr*)&m_listenSS, 0);
    if (r != 0)
    {
        logger->error("agent server bind failed, code={0:d}", r);
        close();
        return false;
    }

    r = uv_listen((uv_stream_t*)m_listener, TCPCONN_BACKLOG, on_uv_connection);
    if (r != 0)
    {
        logger->error("agent server listen failed, code={0:d}", r);
        close();
        return false;
    }

    m_callback = cb;
    return true;
}

void AgentServer::Close()
{
    assert(m_callback != nullptr);
    close();
}

void AgentServer::close()
{
    uv_close((uv_handle_t*)m_listener, (uv_close_cb)free);
    m_callback = nullptr;
}

void AgentServer::on_uv_connection(uv_stream_t* handle, int status)
{
    if (uv_is_closing((uv_handle_t*)handle))
        return;

    AgentServer* pThis = (AgentServer*)handle->data;

    AgentConn* conn = AgentConn::Create(*pThis);
    conn->Accept();
}

char Agent_LOGGER[64];
