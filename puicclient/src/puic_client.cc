// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic_client.h"

#include <cassert>
#include <cstdlib>
#include <string>

namespace net {

PuicClientConn::PuicClientConn(
    PuicDialer &dialer,
    PuicSessionInterface* session /*owned*/
    )
    : m_dialer(dialer)
    , m_session(session)
    , m_sessionId(session->SessionId())
    , m_data(nullptr)
{
    m_session->SetDelegate(this);
}

PuicSessionInterface* PuicClientConn::session() const
{
    return m_session.get();
}

PuicSessionId PuicClientConn::sessionId() const
{
    return m_sessionId;
}

void* PuicClientConn::Data() const
{
    return m_data;
}

void PuicClientConn::SetData(void* data)
{
    m_data = data;
}

void PuicClientConn::OnHandshaked(
    PuicSessionInterface* session,
    const sockaddr* peer_address
    )
{
    assert(session == m_session.get());
    session->PresetStream()->SetDelegate(this);
    m_dialer.callbacks().OnConnected(m_dialer.userData(), this);
}

void PuicClientConn::OnIncomingStream(
    PuicSessionInterface* session,
    PuicStreamInterface* stream
    )
{
    assert(session == m_session.get());
    // unwanted.
    session->CancelStream(stream->StreamId());
}

void PuicClientConn::OnWriteBlocked(
    PuicSessionInterface* session
    )
{
    assert(session == m_session.get());
    m_dialer.OnConnWriteBlocked(this);
}

void PuicClientConn::OnClosed(
    PuicSessionInterface* session,
    int error_code,
    bool from_remote,
    const std::string& error_details
    )
{
    assert(session == m_session.get());
    m_dialer.callbacks().OnDisconnected(m_dialer.userData(), this,
        error_code, from_remote, error_details.c_str());
    m_dialer.OnConnClosed(this);
}

void PuicClientConn::OnCanRead(PuicStreamInterface* stream)
{
    assert(stream->Session() == m_session.get());
    m_dialer.callbacks().OnCanRead(m_dialer.userData(), this);
}

void PuicClientConn::OnCanWrite(PuicStreamInterface* stream)
{
    assert(stream->Session() == m_session.get());
    m_dialer.callbacks().OnCanWrite(m_dialer.userData(), this,
        stream->WriteBufferedAmount());
}

void PuicClientConn::OnClosed(
    PuicStreamInterface* stream,
    int stream_error,
    bool from_remote
    )
{
    // ignore.
}

namespace {

class DeleteConnsAlarm : public QuicAlarm::Delegate
{
public:
    explicit DeleteConnsAlarm(PuicDialer* dialer)
        : m_dialer(dialer) {}

    void OnAlarm() override { m_dialer->DeleteConns(true); }

private:
    PuicDialer* m_dialer;
};

}

PuicDialer::PuicDialer(uv_loop_t* loop, const sockaddr_storage &bindAddr)
    : m_loop(loop)
    , m_bindAddr(bindAddr)
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

PuicDialer::~PuicDialer()
{
    assert(m_cryptoConfig == nullptr);
    while (!m_availSendReqs.empty())
    {
        uv_udp_send_t* req = m_availSendReqs.top();
        m_availSendReqs.pop();
        free(req);
    }
}

int PuicDialer::Start(const PUICCLIENT_ConnsCallbaks &callbacks, void* userData)
{
    m_callbacks = callbacks;
    m_userData = userData;
    return start();
}

int PuicDialer::start()
{
    if (m_cryptoConfig != nullptr)
        return PUICLIERR_STATE;

    m_socket = (uv_udp_t*)malloc(sizeof(uv_udp_t));
    uv_udp_init(m_loop, m_socket);
    m_socket->data = this;

    if (uv_udp_bind(m_socket, (sockaddr*)&m_bindAddr, UV_UDP_REUSEADDR) != 0 ||
        uv_udp_recv_start(m_socket, on_uv_alloc, on_uv_recv) != 0)
    {
        uv_close((uv_handle_t*)m_socket, (uv_close_cb)free);
        m_socket = nullptr;
        return PUICLIERR_UDP;
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

    m_deleteConnsAlarm.reset(CreateAlarm(new DeleteConnsAlarm(this)));
    m_cryptoConfig.reset(CreateQuicCryptoClientConfig());
    // Assume CPU has hardware acceleration for AES-GCM.
    // m_cryptoConfig->PreferAesGcm();

    return PUICLIERR_SUCCESS;
}

void PuicDialer::Stop()
{
    stop();
    memset(&m_callbacks, 0, sizeof(m_callbacks));
    m_userData = nullptr;
}

void PuicDialer::stop()
{
    if (m_cryptoConfig == nullptr)
        return;

    uv_close((uv_handle_t*)m_socket, (uv_close_cb)free);
    m_socket = nullptr;
    m_writeBlocked = true;

    m_deleteConnsAlarm->Cancel();

    while (!m_conns.empty())
    {
        PuicClientConn* conn = m_conns.begin()->second.get();
        conn->session()->Close();
    }
    DeleteConns(false);

    m_cryptoConfig.reset(nullptr);
}

void PuicDialer::DeleteConns(bool mayRestart)
{
    m_closedConns.clear();

    if (mayRestart && m_conns.empty())
    {
        stop();
    }
}

PuicClientConn* PuicDialer::CreateConn(
    const std::string &serverHost,
    const sockaddr_storage &serverAddr
    )
{
    if (m_cryptoConfig == nullptr && start() != PUICLIERR_SUCCESS)
        return nullptr;

    PuicClientSessionConfig create_config;
    PuicSessionInterface* session = CreatePuicClientSession(
        create_config, serverHost,
        (sockaddr*)&m_bindAddr, (sockaddr*)&serverAddr,
        this, this, this, m_cryptoConfig.get());

    PuicClientConn* conn = new PuicClientConn(*this, session);
    m_conns.insert(std::make_pair(conn->sessionId(), QuicWrapUnique(conn)));

    return conn;
}

const PUICCLIENT_ConnsCallbaks& PuicDialer::callbacks() const
{
    return m_callbacks;
}

void* PuicDialer::userData() const
{
    return m_userData;
}

void PuicDialer::OnConnClosed(PuicClientConn* conn)
{
    PuicSessionId id = conn->sessionId();
    ConnMap::iterator it = m_conns.find(id);
    if (it == m_conns.end()) {
        assert("conn not found" == nullptr);
        return;
    }

    if (m_closedConns.empty())
    {
        m_deleteConnsAlarm->Update(m_clock.ApproximateNow(),
            QuicTime::Delta::Zero());
    }

    m_closedConns.push_back(std::move(it->second));
    m_conns.erase(it);
    m_blockedConns.erase(conn);
}

void PuicDialer::OnConnWriteBlocked(PuicClientConn* conn)
{
    m_blockedConns.insert(std::make_pair(conn, true));
}

QuicAlarm* PuicDialer::CreateAlarm(QuicAlarm::Delegate* delegate)
{
    return new PuicUVAlarm(m_loop, GetClock(),
        QuicArenaScopedPtr<QuicAlarm::Delegate>(delegate));
}

QuicArenaScopedPtr<QuicAlarm> PuicDialer::CreateAlarm(
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

const QuicClock* PuicDialer::GetClock() const
{
    return &m_clock;
}

QuicRandom* PuicDialer::GetRandomGenerator()
{
    return QuicRandom::GetInstance();
}

QuicBufferAllocator* PuicDialer::GetStreamSendBufferAllocator()
{
    return &m_bufferAllocator;
}

WriteResult PuicDialer::WritePacket(
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

bool PuicDialer::IsWriteBlockedDataBuffered() const
{
    return false;
}

bool PuicDialer::IsWriteBlocked() const
{
    return m_writeBlocked;
}

void PuicDialer::SetWritable()
{
    // ignore.
}

QuicByteCount PuicDialer::GetMaxPacketSize(
    const QuicSocketAddress& peer_address
    ) const
{
    assert(kMaxPacketSize < MessageBuflen);
    return kMaxPacketSize;
}

void PuicDialer::on_uv_alloc(
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

    PuicDialer* pThis = (PuicDialer*)(handle->data);
    *buf = uv_buf_init(pThis->m_recvBuf, MessageBuflen * MaxRecvMessages);
}

void PuicDialer::on_uv_recv(
    uv_udp_t* handle,
    ssize_t nread,
    const uv_buf_t* buf,
    const sockaddr* addr,
    unsigned flags
    )
{
    if (uv_is_closing((uv_handle_t*)handle))
        return;

    PuicDialer* pThis = (PuicDialer*)(handle->data);

    if (addr == nullptr || nread == 0)
        return;

    if (nread < 0)
        return; // read error.

    pThis->dispatch(addr, buf->base, nread);
}

void PuicDialer::dispatch(const sockaddr* addr, const char* data, size_t len)
{
    PuicSessionId id = ReadPuicSessionId(data, len);
    if (id == 0)
        return;

    ConnMap::iterator it = m_conns.find(id);
    if (it == m_conns.end())
        return;

    PuicClientConn* conn = it->second.get();
    conn->session()->OnTransportReceived(addr, data, len);
}

void PuicDialer::on_uv_send(uv_udp_send_t* req, int status)
{
    uv_udp_t* handle = req->handle;
    if (uv_is_closing((uv_handle_t*)handle))
    {
        free(req);
        return;
    }

    PuicDialer* pThis = (PuicDialer*)(handle->data);

    pThis->m_availSendReqs.push(req);

    if (pThis->m_writeBlocked && pThis->m_availSendReqs.size() == MaxFlyingMessages)
    {
        pThis->m_writeBlocked = false;
        pThis->wakeup();
    }
}

void PuicDialer::wakeup()
{
    assert(!IsWriteBlocked());

    while (!m_blockedConns.empty() && !IsWriteBlocked())
    {
        PuicClientConn* conn = m_blockedConns.begin()->first;
        m_blockedConns.erase(m_blockedConns.begin());
        conn->session()->OnTransportCanWrite();
    }
}

}  // namespace net
