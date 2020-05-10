// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_SESSION_BASE_H_
#define PUIC_SESSION_BASE_H_

#include <memory>

#include "puic/puic_session_interface.h"
#include "puic/puic_stream.h"

#include "net/quic/core/quic_session.h"

namespace net {

class PuicStream;

class PuicSessionBase : public QuicSession,
                        public PuicSessionInterface {
 public:
  // Takes ownership of |connection|.
  PuicSessionBase(QuicConnection* connection,
                  QuicSession::Visitor* owner,
                  const QuicConfig& config);
  ~PuicSessionBase() override;

  void Initialize() override;

  void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) override;

  void CloseStream(QuicStreamId stream_id) override;

  void OnConfigNegotiated() override;

  void OnWriteBlocked() override;

  void OnConnectionClosed(QuicErrorCode error,
                          const std::string& error_details,
                          ConnectionCloseSource source) override;

  PuicSessionId SessionId() const override;

  PuicStreamInterface* PresetStream() override;

  void Close() override;

  // Returns nullptr if failed (max streams have already been opened, for example).
  PuicStreamInterface* CreateStream(const StreamParameters& param) override;

  void CancelStream(PuicStreamId stream_id) override;

  bool IsOpenStream(PuicStreamId stream_id) override;

  void OnTransportCanWrite() override;

  void OnTransportReceived(const char* data, size_t data_len) override;

  void OnTransportReceived(const sockaddr* peer_address,
                           const char* data,
                           size_t data_len) override;

  void SetDelegate(PuicSessionInterface::Delegate* delegate) override;

 protected:
  PuicStream* CreateIncomingDynamicStream(QuicStreamId id) override;

  PuicStream* CreateOutgoingDynamicStream(SpdyPriority priority) override;

  std::unique_ptr<PuicStream> CreateDataStream(QuicStreamId id,
                                               SpdyPriority priority);

  PuicStream* ActivateDataStream(std::unique_ptr<PuicStream> stream);

  void ResetStream(QuicStreamId stream_id, QuicRstStreamErrorCode error);

  const QuicClock* clock() const { return clock_; }

  PuicSessionInterface::Delegate* delegate() { return delegate_; }

 private:
  // For recording packet receipt time
  const QuicClock* clock_;

  PuicStream* preset_stream_;

  PuicSessionInterface::Delegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PuicSessionBase);
};

}  // namespace net

#endif  // PUIC_SESSION_BASE_H_
