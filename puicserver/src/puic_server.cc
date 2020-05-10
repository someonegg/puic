// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic_server.h"

#include <cassert>
#include <cstdlib>
#include <string>

namespace net {

PuicServerConn::PuicServerConn(
    PuicListener &listener,
    PuicSessionInterface* session
    )
    : m_listener(listener)
    , m_session(session)
    , m_handshaked(false)
    , m_data(nullptr)
{
    m_session->SetDelegate(this);
}

PuicSessionInterface* PuicServerConn::session() const
{
    return m_session;
}

void* PuicServerConn::Data() const
{
    return m_data;
}

void PuicServerConn::SetData(void* data)
{
    m_data = data;
}

void PuicServerConn::OnHandshaked(
    PuicSessionInterface* session,
    const sockaddr* peer_address
    )
{
    assert(session == m_session);
    session->PresetStream()->SetDelegate(this);

    char name[64] = {0};
    int port = 0;
    if (peer_address->sa_family == AF_INET)
    {
        const sockaddr_in* in = (const sockaddr_in*)peer_address;
        uv_ip4_name(in, name, sizeof(name));
        port = ntohs(in->sin_port);
    }
    else if (peer_address->sa_family == AF_INET6)
    {
        const sockaddr_in6* in6 = (const sockaddr_in6*)peer_address;
        uv_ip6_name(in6, name, sizeof(name));
        port = ntohs(in6->sin6_port);
    }

    m_handshaked = true;
    m_listener.callbacks().OnAccept(m_listener.userData(), this, name, port);
}

void PuicServerConn::OnIncomingStream(
    PuicSessionInterface* session,
    PuicStreamInterface* stream
    )
{
    assert(session == m_session);
    // unwanted.
    session->CancelStream(stream->StreamId());
}

void PuicServerConn::OnWriteBlocked(
    PuicSessionInterface* session
    )
{
    // ignore.
}

void PuicServerConn::OnClosed(
    PuicSessionInterface* session,
    int error_code,
    bool from_remote,
    const std::string& error_details
    )
{
    assert(session == m_session);
    if (!m_handshaked)
        return;

    m_listener.callbacks().OnDisconnected(m_listener.userData(), this,
        error_code, from_remote, error_details.c_str());
    delete this;
}

void PuicServerConn::OnCanRead(PuicStreamInterface* stream)
{
    assert(stream->Session() == m_session);
    if (!m_handshaked)
        return;

    m_listener.callbacks().OnCanRead(m_listener.userData(), this);
}

void PuicServerConn::OnCanWrite(PuicStreamInterface* stream)
{
    assert(stream->Session() == m_session);
    if (!m_handshaked)
        return;

    m_listener.callbacks().OnCanWrite(m_listener.userData(), this,
        stream->WriteBufferedAmount());
}

void PuicServerConn::OnClosed(
    PuicStreamInterface* stream,
    int stream_error,
    bool from_remote
    )
{
    // ignore.
}

PuicListener::PuicListener(uv_loop_t* loop, const sockaddr_storage &serverAddr)
    : m_loop(loop)
    , m_serverAddr(serverAddr)
    , m_clock(m_loop)
    , m_socket(nullptr)
    , m_writeBlocked(true)
    , m_userData(nullptr)
{
    for (size_t i = 0; i < MaxFlyingMessages; ++i)
    {
        char* mem = (char*)malloc(sizeof(uv_udp_send_t) + MessageBuflen);
        uv_udp_send_t* req = (uv_udp_send_t*)mem;
        req->data = mem + sizeof(uv_udp_send_t);
        m_availSendReqs.push(req);
    }
    memset(&m_callbacks, 0, sizeof(m_callbacks));
}

PuicListener::~PuicListener()
{
    assert(m_puicDispatcher == nullptr);
    while (!m_availSendReqs.empty())
    {
        uv_udp_send_t* req = m_availSendReqs.top();
        m_availSendReqs.pop();
        free(req);
    }
}

int PuicListener::Start(const PUICSERVER_ConnsCallbaks &callbacks, void* userData)
{
    if (m_puicDispatcher != nullptr)
        return PUISRVERR_STATE;

    m_socket = (uv_udp_t*)malloc(sizeof(uv_udp_t));
    uv_udp_init(m_loop, m_socket);
    m_socket->data = this;

    if (uv_udp_bind(m_socket, (sockaddr*)&m_serverAddr, UV_UDP_REUSEADDR) != 0 ||
        uv_udp_recv_start(m_socket, on_uv_alloc, on_uv_recv) != 0)
    {
        uv_close((uv_handle_t*)m_socket, (uv_close_cb)free);
        m_socket = nullptr;
        return PUISRVERR_UDP;
    }
    m_writeBlocked = false;

    // adjust recv buffer
    {
        const int DEFAULT = 8 * 1024 * 1024;
        int size = 0;
        uv_recv_buffer_size((uv_handle_t*)m_socket, &size);
        if (size < DEFAULT)
        {
            size = DEFAULT;
            uv_recv_buffer_size((uv_handle_t*)m_socket, &size);
        }
    }
    // adjust send buffer
    {
        const int DEFAULT = 8 * 1024 * 1024;
        int size = 0;
        uv_send_buffer_size((uv_handle_t*)m_socket, &size);
        if (size < DEFAULT)
        {
            size = DEFAULT;
            uv_send_buffer_size((uv_handle_t*)m_socket, &size);
        }
    }

    m_callbacks = callbacks;
    m_userData = userData;

    PuicDispatcherConfig create_config;

    m_puicDispatcher.reset(CreatePuicDispatcher(
        create_config, (sockaddr*)&m_serverAddr, this, this, this));

    m_puicDispatcher->SetDelegate(this);

    return PUISRVERR_SUCCESS;
}

void PuicListener::Stop()
{
    if (m_puicDispatcher == nullptr)
        return;

    uv_close((uv_handle_t*)m_socket, (uv_close_cb)free);
    m_socket = nullptr;
    m_writeBlocked = true;

    m_puicDispatcher->Close();
    m_puicDispatcher.reset(nullptr);

    memset(&m_callbacks, 0, sizeof(m_callbacks));
    m_userData = nullptr;
}

const PUICSERVER_ConnsCallbaks& PuicListener::callbacks() const
{
    return m_callbacks;
}

void* PuicListener::userData() const
{
    return m_userData;
}

QuicAlarm* PuicListener::CreateAlarm(QuicAlarm::Delegate* delegate)
{
    return new PuicUVAlarm(m_loop, GetClock(),
        QuicArenaScopedPtr<QuicAlarm::Delegate>(delegate));
}

QuicArenaScopedPtr<QuicAlarm> PuicListener::CreateAlarm(
    QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
    QuicConnectionArena* arena
    )
{
    if (arena != nullptr)
    {
        return arena->New<PuicUVAlarm>(
            m_loop, GetClock(), std::move(delegate));
    }
    else
    {
        return QuicArenaScopedPtr<QuicAlarm>(new PuicUVAlarm(
            m_loop, GetClock(), std::move(delegate)));
    }
}

const QuicClock* PuicListener::GetClock() const
{
    return &m_clock;
}

QuicRandom* PuicListener::GetRandomGenerator()
{
    return QuicRandom::GetInstance();
}

QuicBufferAllocator* PuicListener::GetStreamSendBufferAllocator()
{
    return &m_bufferAllocator;
}

WriteResult PuicListener::WritePacket(
    const char* buffer,
    size_t buf_len,
    const QuicIpAddress& self_address,
    const QuicSocketAddress& peer_address,
    PerPacketOptions* options
    )
{
    assert(buf_len < MessageBuflen);

    if (m_writeBlocked)
        return WriteResult(WRITE_STATUS_BLOCKED, EWOULDBLOCK);

    sockaddr_storage peerAddr = peer_address.generic_address();

    if (m_availSendReqs.empty())
    {
        m_writeBlocked = true;
        return WriteResult(WRITE_STATUS_BLOCKED, EWOULDBLOCK);
    }

    uv_udp_send_t* req = m_availSendReqs.top();
    m_availSendReqs.pop();

    memcpy(req->data, buffer, buf_len);

    uv_buf_t buf = uv_buf_init((char*)req->data, (unsigned)buf_len);
    int r = uv_udp_send(req, m_socket, &buf, 1, (sockaddr*)&peerAddr, on_uv_send);
    if (r < 0)
        return WriteResult(WRITE_STATUS_ERROR, r);

    return WriteResult(WRITE_STATUS_OK, buf_len);
}

bool PuicListener::IsWriteBlockedDataBuffered() const
{
    return false;
}

bool PuicListener::IsWriteBlocked() const
{
    return m_writeBlocked;
}

void PuicListener::SetWritable()
{
    // ignore.
}

QuicByteCount PuicListener::GetMaxPacketSize(
    const QuicSocketAddress& peer_address
    ) const
{
    assert(kMaxPacketSize < MessageBuflen);
    return kMaxPacketSize;
}

void PuicListener::OnNewSession(
    PuicDispatcherInterface* dispatcher,
    PuicSessionInterface* session,
    const sockaddr* client_address
    )
{
    assert(dispatcher == m_puicDispatcher.get());
    new PuicServerConn(*this, session);

    if (m_callbacks.OnPreAccept != nullptr)
    {
        char name[64] = {0};
        int port = 0;
        if (client_address->sa_family == AF_INET)
        {
            const sockaddr_in* in = (const sockaddr_in*)client_address;
            uv_ip4_name(in, name, sizeof(name));
            port = ntohs(in->sin_port);
        }
        else if (client_address->sa_family == AF_INET6)
        {
            const sockaddr_in6* in6 = (const sockaddr_in6*)client_address;
            uv_ip6_name(in6, name, sizeof(name));
            port = ntohs(in6->sin6_port);
        }
        m_callbacks.OnPreAccept(m_userData, name, port);
    }
}

void PuicListener::on_uv_alloc(
    uv_handle_t* handle,
    size_t suggested_size,
    uv_buf_t* buf
    )
{
    if (uv_is_closing((uv_handle_t*)handle))
    {
        *buf = uv_buf_init(nullptr, 0);
        return;
    }

    PuicListener* pThis = (PuicListener*)(handle->data);
    *buf = uv_buf_init(pThis->m_recvBuf, MessageBuflen * MaxRecvMessages);
}

void PuicListener::on_uv_recv(
    uv_udp_t* handle,
    ssize_t nread,
    const uv_buf_t* buf,
    const sockaddr* addr,
    unsigned flags
    )
{
    if (uv_is_closing((uv_handle_t*)handle))
        return;

    PuicListener* pThis = (PuicListener*)(handle->data);

    if (addr == nullptr || nread == 0)
        return;

    if (nread < 0)
        return; // read error.

    pThis->m_puicDispatcher->OnTransportReceived(addr, buf->base, nread);
}

void PuicListener::on_uv_send(uv_udp_send_t* req, int status)
{
    uv_udp_t* handle = req->handle;
    if (uv_is_closing((uv_handle_t*)handle))
    {
        free(req);
        return;
    }

    PuicListener* pThis = (PuicListener*)(handle->data);

    pThis->m_availSendReqs.push(req);

    if (pThis->m_writeBlocked && pThis->m_availSendReqs.size() == MaxFlyingMessages)
    {
        pThis->m_writeBlocked = false;
        pThis->m_puicDispatcher->OnTransportCanWrite();
    }
}

}  // namespace net
