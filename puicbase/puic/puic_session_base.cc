// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic/puic_session_base.h"

#include "net/quic/platform/api/quic_ptr_util.h"
#include "net/quic/core/quic_data_reader.h"

using std::string;

namespace net {

PuicSessionBase::PuicSessionBase(QuicConnection* connection,
                                 QuicSession::Visitor* owner,
                                 const QuicConfig& config)
    : QuicSession(connection, owner, config),
      clock_(connection->helper()->GetClock()),
      preset_stream_(nullptr) {}

PuicSessionBase::~PuicSessionBase() {
  delete connection();
}

void PuicSessionBase::Initialize() {
  QuicSession::Initialize();
}

void PuicSessionBase::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  QuicSession::OnCryptoHandshakeEvent(event);
  if (event == HANDSHAKE_CONFIRMED) {
    DCHECK(IsEncryptionEstablished());
    DCHECK(IsCryptoHandshakeConfirmed());

    if (perspective() == Perspective::IS_SERVER) {
      set_largest_peer_created_stream_id(kPresetStreamId);
      preset_stream_ = CreateIncomingDynamicStream(kPresetStreamId);
    } else {
      preset_stream_ = CreateOutgoingDynamicStream(kHighestPriority);
    }

    DCHECK(delegate_);
    sockaddr_storage peer_addr = connection()->peer_address().generic_address();
    delegate_->OnHandshaked(this,
      reinterpret_cast<const sockaddr*>(&peer_addr));
  }
}

void PuicSessionBase::CloseStream(QuicStreamId stream_id) {
  if (IsClosedStream(stream_id)) {
    // When CloseStream has been called recursively (via
    // QuicStream::OnClose), the stream is already closed so return.
    return;
  }
  write_blocked_streams()->UnregisterStream(stream_id);
  QuicSession::CloseStream(stream_id);
}

void PuicSessionBase::OnConfigNegotiated() {
  QuicSession::OnConfigNegotiated();
}

void PuicSessionBase::OnWriteBlocked() {
  QuicSession::OnWriteBlocked();
  DCHECK(delegate_);
  delegate_->OnWriteBlocked(this);
}

void PuicSessionBase::OnConnectionClosed(QuicErrorCode error,
                                         const string& error_details,
                                         ConnectionCloseSource source) {
  QuicSession::OnConnectionClosed(error, error_details, source);
  DCHECK(delegate_);
  delegate_->OnClosed(
      this, error, source == ConnectionCloseSource::FROM_PEER, error_details);
}

PuicSessionId PuicSessionBase::SessionId() const {
  return connection()->connection_id();
}

PuicStreamInterface* PuicSessionBase::PresetStream() {
  return preset_stream_;
}

void PuicSessionBase::Close() {
  connection()->CloseConnection(
      QUIC_PEER_GOING_AWAY, "disconnecting",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

PuicStreamInterface* PuicSessionBase::CreateStream(const StreamParameters& param) {
  if (GetNumOpenOutgoingStreams() >= max_open_outgoing_streams()) {
    return nullptr;
  }
  PuicStream* stream = CreateOutgoingDynamicStream(param.priority);
  if (stream == nullptr) {
    return nullptr;
  }
  return stream;
}

void PuicSessionBase::CancelStream(PuicStreamId stream_id) {
  ResetStream(stream_id, QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
}

bool PuicSessionBase::IsOpenStream(PuicStreamId stream_id) {
  return QuicSession::IsOpenStream(stream_id);
}

void PuicSessionBase::OnTransportCanWrite() {
  connection()->OnBlockedWriterCanWrite();
}

void PuicSessionBase::OnTransportReceived(const char* data, size_t data_len) {
  QuicReceivedPacket packet(data, data_len, clock_->Now());
  ProcessUdpPacket(connection()->self_address(), connection()->peer_address(),
                   packet);
}

void PuicSessionBase::OnTransportReceived(const sockaddr* peer_address,
                                          const char* data,
                                          size_t data_len) {
  QuicReceivedPacket packet(data, data_len, clock_->Now());
  ProcessUdpPacket(connection()->self_address(), QuicSocketAddress(peer_address),
                   packet);
}

void PuicSessionBase::SetDelegate(
    PuicSessionInterface::Delegate* delegate) {
  if (delegate_) {
    LOG(WARNING) << "The delegate for the session has already been set.";
  }
  delegate_ = delegate;
  DCHECK(delegate_);
}

PuicStream* PuicSessionBase::CreateIncomingDynamicStream(QuicStreamId id) {
  if (id == kPresetStreamId)
    return ActivateDataStream(CreateDataStream(id, kHighestPriority));
  else
    return ActivateDataStream(CreateDataStream(id, kDefaultPriority));
}

PuicStream* PuicSessionBase::CreateOutgoingDynamicStream(
    SpdyPriority priority) {
  return ActivateDataStream(
      CreateDataStream(GetNextOutgoingStreamId(), priority));
}

std::unique_ptr<PuicStream> PuicSessionBase::CreateDataStream(
    QuicStreamId id,
    SpdyPriority priority) {
  const QuicCryptoStream* crypto_stream = GetCryptoStream();
  if (crypto_stream == nullptr || !crypto_stream->encryption_established()) {
    // Encryption not active so no stream created
    return nullptr;
  }
  auto stream = QuicMakeUnique<PuicStream>(id, this);
  if (stream) {
    // Register the stream to the QuicWriteBlockedList. |priority| is clamped
    // between 0 and 7, with 0 being the highest priority and 7 the lowest
    // priority.
    write_blocked_streams()->RegisterStream(stream->id(), priority);

    if (IsIncomingStream(id) && id != kPresetStreamId) {
      DCHECK(delegate_);
      // Incoming streams need to be registered with the delegate_.
      delegate_->OnIncomingStream(this, stream.get());
    }
  }
  return stream;
}

PuicStream* PuicSessionBase::ActivateDataStream(
    std::unique_ptr<PuicStream> stream) {
  // Transfer ownership of the data stream to the session via ActivateStream().
  PuicStream* raw = stream.release();
  if (raw) {
    // Make QuicSession take ownership of the stream.
    ActivateStream(std::unique_ptr<QuicStream>(raw));
  }
  return raw;
}

void PuicSessionBase::ResetStream(QuicStreamId stream_id,
                                  QuicRstStreamErrorCode error) {
  if (!IsOpenStream(stream_id)) {
    return;
  }
  QuicStream* stream = QuicSession::GetOrCreateStream(stream_id);
  if (stream) {
    stream->Reset(error);
  }
}

PuicSessionId ReadPuicSessionId(const char* packet, size_t packet_len) {
  QuicDataReader reader(packet, packet_len, NETWORK_BYTE_ORDER);

  uint8_t public_flags;
  if (!reader.ReadBytes(&public_flags, 1)) {
    return 0;
  }

  if ((public_flags & PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID) ==
       PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID) {
    QuicConnectionId connection_id = 0;
    if (reader.ReadConnectionId(&connection_id)) {
      return connection_id;
    }
  }

  return 0;
}

}  // namespace net
