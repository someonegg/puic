// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_CLIENT_H__
#define __PUIC_CLIENT_H__

#include <memory>
#include <vector>
#include <stack>
#include <string>

#include "puic_client_def.h"
#include "puic_client_err.h"

#include "puic/puic_session_interface.h"
#include "puic/puic_stream_interface.h"
#include "puic_uv/puic_uv_utils.h"
#include "net/quic/platform/api/quic_ptr_util.h"
#include "net/quic/platform/api/quic_containers.h"
#include "net/quic/core/quic_alarm_factory.h"
#include "net/quic/core/quic_simple_buffer_allocator.h"
#include "net/quic/core/quic_connection.h"
#include "net/quic/core/crypto/quic_crypto_client_config.h"

namespace net {

class PuicDialer;

class PuicClientConn
    : public PuicSessionInterface::Delegate
    , public PuicStreamInterface::Delegate
{
    PuicDialer &m_dialer;
    std::unique_ptr<PuicSessionInterface> m_session;
    PuicSessionId m_sessionId;
    void* m_data;

public:
    PuicClientConn(
        PuicDialer &dialer,
        PuicSessionInterface* session /* owned */
        );

    PuicSessionInterface* session() const;
    PuicSessionId sessionId() const;

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

class PuicDialer
    : public QuicAlarmFactory
    , public QuicConnectionHelperInterface
    , public QuicPacketWriter
{
    uv_loop_t* m_loop;
    sockaddr_storage m_bindAddr;

    PuicUVClock m_clock;
    SimpleBufferAllocator m_bufferAllocator;

    uv_udp_t* m_socket;
    char m_recvBuf[MessageBuflen * MaxRecvMessages];
    std::stack<uv_udp_send_t*> m_availSendReqs;
    bool m_writeBlocked;

    PUICCLIENT_ConnsCallbaks m_callbacks;
    void* m_userData;

    std::unique_ptr<QuicCryptoClientConfig> m_cryptoConfig;

    using ConnMap = QuicUnorderedMap<PuicSessionId, std::unique_ptr<PuicClientConn>>;
    ConnMap m_conns;

    using ConnArray = std::vector<std::unique_ptr<PuicClientConn>>;
    ConnArray m_closedConns;
    std::unique_ptr<QuicAlarm> m_deleteConnsAlarm;

    using WriteBlockedList = QuicLinkedHashMap<PuicClientConn*, bool>;
    WriteBlockedList m_blockedConns;

public:
    PuicDialer(uv_loop_t* loop, const sockaddr_storage &bindAddr);
    ~PuicDialer() override;

    int Start(const PUICCLIENT_ConnsCallbaks &callbacks, void* userData);

    void Stop();

    void DeleteConns(bool mayRestart);

    PuicClientConn* CreateConn(
        const std::string &serverHost,
        const sockaddr_storage &serverAddr
        );

public:
    // for PuicClientConn
    const PUICCLIENT_ConnsCallbaks& callbacks() const;
    void* userData() const;

    void OnConnClosed(PuicClientConn* conn);
    void OnConnWriteBlocked(PuicClientConn* conn);

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

protected:
    int start();

    void stop();

private:
    static void on_uv_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

    static void on_uv_recv(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
        const sockaddr* addr, unsigned flags
        );
    void dispatch(const sockaddr* addr, const char* data, size_t len);

    static void on_uv_send(uv_udp_send_t* req, int status);
    void wakeup();
};

} // namespace net

#endif // __PUIC_CLIENT_H__