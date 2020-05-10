// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_SERVER_H__
#define __PUIC_SERVER_H__

#include <memory>
#include <stack>

#include "puic_server_def.h"
#include "puic_server_err.h"

#include "puic/puic_dispatcher_interface.h"
#include "puic/puic_session_interface.h"
#include "puic/puic_stream_interface.h"
#include "puic_uv/puic_uv_utils.h"
#include "net/quic/core/quic_alarm_factory.h"
#include "net/quic/core/quic_simple_buffer_allocator.h"
#include "net/quic/core/quic_connection.h"

namespace net {

class PuicListener;

class PuicServerConn
    : public PuicSessionInterface::Delegate
    , public PuicStreamInterface::Delegate
{
    PuicListener &m_listener;
    PuicSessionInterface* m_session;
    bool m_handshaked;
    void* m_data;

public:
    PuicServerConn(
        PuicListener &listener,
        PuicSessionInterface* session /* not owned */
    );

    PuicSessionInterface* session() const;

    void* Data() const;

    void SetData(void* data);

public:
    // PuicSessionInterface::Delegate overrides.
    void OnHandshaked(
        PuicSessionInterface* session,
        const sockaddr* peer_address
        ) override;

    void OnIncomingStream(
        PuicSessionInterface* session,
        PuicStreamInterface* stream
        ) override;

    void OnWriteBlocked(
        PuicSessionInterface* session
        ) override;

    void OnClosed(
        PuicSessionInterface* session,
        int error_code,
        bool from_remote,
        const std::string& error_details
        ) override;

    // PuicStreamInterface::Delegate overrides.
    void OnCanRead(PuicStreamInterface* stream) override;

    void OnCanWrite(PuicStreamInterface* stream) override;

    void OnClosed(
        PuicStreamInterface* stream,
        int stream_error,
        bool from_remote
        ) override;
};

// libuv : UV_UDP_MTU_DGRAM : UDP_DGRAM_MAXSIZE
const size_t MessageBuflen = 1500;

// libuv : MSGNUM_PER_UDP_RECVMMSG
const size_t MaxRecvMessages = 32;

// buffer used = MessageBuflen * MaxFlyingMessages
const size_t MaxFlyingMessages = 1024;

class PuicListener
    : public QuicAlarmFactory
    , public QuicConnectionHelperInterface
    , public QuicPacketWriter
    , public PuicDispatcherInterface::Delegate
{
    uv_loop_t* m_loop;
    sockaddr_storage m_serverAddr;

    PuicUVClock m_clock;
    SimpleBufferAllocator m_bufferAllocator;

    uv_udp_t* m_socket;
    char m_recvBuf[MessageBuflen * MaxRecvMessages];
    std::stack<uv_udp_send_t*> m_availSendReqs;
    bool m_writeBlocked;

    PUICSERVER_ConnsCallbaks m_callbacks;
    void* m_userData;
    std::unique_ptr<PuicDispatcherInterface> m_puicDispatcher;

public:
    PuicListener(uv_loop_t* loop, const sockaddr_storage &serverAddr);
    ~PuicListener() override;

    int Start(const PUICSERVER_ConnsCallbaks &callbacks, void* userData);

    void Stop();

public:
    // for PuicServerConn
    const PUICSERVER_ConnsCallbaks& callbacks() const;
    void* userData() const;

public:
    // QuicAlarmFactory overrides.
    QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) override;

    QuicArenaScopedPtr<QuicAlarm> CreateAlarm(
        QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
        QuicConnectionArena* arena
        ) override;

    // QuicConnectionHelperInterface overrides.
    const QuicClock* GetClock() const override;

    QuicRandom* GetRandomGenerator() override;

    QuicBufferAllocator* GetStreamSendBufferAllocator() override;

public:
    // QuicPacketWriter overrides.
    WriteResult WritePacket(
        const char* buffer,
        size_t buf_len,
        const QuicIpAddress& self_address,
        const QuicSocketAddress& peer_address,
        PerPacketOptions* options
        ) override;

    bool IsWriteBlockedDataBuffered() const override;

    bool IsWriteBlocked() const override;

    void SetWritable() override;

    QuicByteCount GetMaxPacketSize(
        const QuicSocketAddress& peer_address
        ) const override;

public:
    // PuicDispatcherInterface::Delegate overrides.
    void OnNewSession(
        PuicDispatcherInterface* dispatcher,
        PuicSessionInterface* session,
        const sockaddr* client_address
        ) override;

private:
    static void on_uv_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void on_uv_recv(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
        const sockaddr* addr, unsigned flags
        );
    static void on_uv_send(uv_udp_send_t* req, int status);
};

} // namespace net

#endif // __PUIC_SERVER_H__