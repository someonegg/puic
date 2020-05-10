// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_DISPATCHER_H_
#define PUIC_DISPATCHER_H_

#include <memory>

#include "puic/puic_dispatcher_interface.h"
#include "puic/puic_server_session.h"

#include "net/tools/quic/quic_dispatcher.h"

namespace net {

class PuicDispatcher : public QuicDispatcher,
                       public PuicDispatcherInterface {
 public:
  PuicDispatcher(
      const QuicSocketAddress& server_address,
      const QuicConfig& config,
      const QuicVersionManager& version_manager,
      QuicCryptoServerConfig* crypto_config,
      QuicCryptoServerStream::Helper* session_helper,
      QuicConnectionHelperInterface* helper,
      QuicAlarmFactory* alarm_factory,
      QuicPacketWriter* writer,
      size_t max_new_sessions_per_sec);

  void Close() override;

  void OnTransportCanWrite() override;

  void OnTransportReceived(const sockaddr* client_address,
                           const char* data,
                           size_t data_len) override;

  void SetDelegate(PuicDispatcherInterface::Delegate* delegate) override;

  void ProcessBufferedSessions();

 protected:
  const QuicSocketAddress& server_address() const { return server_address_; }

  PuicServerSession* CreateQuicSession(
      QuicConnectionId connection_id,
      const QuicSocketAddress& client_address,
      QuicStringPiece alpn) override;

  virtual QuicPacketWriter* CreatePerSessionWriter();

 private:
  QuicSocketAddress server_address_;

  // For recording packet receipt time
  const QuicClock* clock_;

  size_t max_new_sessions_per_sec_;

  std::unique_ptr<QuicAlarm> buffered_sessions_alarm_;

  PuicDispatcherInterface::Delegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PuicDispatcher);
};

}  // namespace net

#endif  // PUIC_DISPATCHER_H_
