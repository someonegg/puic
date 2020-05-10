// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_SESSION_INTERFACE_H_
#define PUIC_SESSION_INTERFACE_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "puic/puic_stream_interface.h"

struct sockaddr;

namespace net {

typedef uint64_t PuicSessionId;

PuicSessionId ReadPuicSessionId(const char* packet, size_t packet_len);

// Send and receive separate streams of reliable, in-order data.
class PuicSessionInterface {
 public:
  virtual ~PuicSessionInterface() {}

  virtual PuicSessionId SessionId() const = 0;

  virtual void StartCryptoHandshake() = 0;

  // The preset stream is owned by PuicSession, not the caller.
  virtual PuicStreamInterface* PresetStream() = 0;

  virtual void Close() = 0;

  struct StreamParameters {
    StreamParameters() : priority(kDefaultPriority) {}
    PuicPriority priority;
  };

  // The return stream is owned by PuicSession, not the caller.
  // Returns nullptr if failed.
  virtual PuicStreamInterface* CreateStream(
      const StreamParameters& params) = 0;

  virtual void CancelStream(PuicStreamId stream_id) = 0;

  // This method verifies if a stream is still open and stream pointer can be
  // used. When true is returned, the interface pointer is good for making a
  // call immediately on the same thread, but may be rendered invalid by ANY
  // other QUIC activity.
  virtual bool IsOpenStream(PuicStreamId stream_id) = 0;

  virtual void OnTransportCanWrite() = 0;

  virtual void OnTransportReceived(const char* data, size_t data_len) = 0;

  virtual void OnTransportReceived(const sockaddr* peer_address,
                                   const char* data,
                                   size_t data_len) = 0;

  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnHandshaked(PuicSessionInterface* session,
                              const sockaddr* peer_address) = 0;

    virtual void OnIncomingStream(PuicSessionInterface* session,
                                  PuicStreamInterface* stream) = 0;

    virtual void OnWriteBlocked(PuicSessionInterface* session) = 0;

    // The session should not be accessed after OnClosed is called.
    virtual void OnClosed(PuicSessionInterface* session,
                          int error_code,
                          bool from_remote,
                          const std::string& error_details) = 0;
  };

  // The |delegate| is not owned by PuicSession.
  virtual void SetDelegate(Delegate* delegate) = 0;
};

struct PuicClientSessionConfig {
  size_t handshake_timeout_secs = 5;

  size_t idle_timeout_secs = 80;

  size_t stream_window = 1024 * 1024;

  size_t session_window = 1536 * 1024;
};

class QuicConnectionHelperInterface;
class QuicAlarmFactory;
class QuicPacketWriter;
class QuicCryptoClientConfig;

QuicCryptoClientConfig* CreateQuicCryptoClientConfig();

// The |helper| |alarm_factory| |writer| |crypto_config| are not owned by PuicSession.
PuicSessionInterface*
CreatePuicClientSession(const PuicClientSessionConfig& create_config,
                        const std::string& server_host,
                        const sockaddr* client_address,
                        const sockaddr* server_address,
                        QuicConnectionHelperInterface* helper,
                        QuicAlarmFactory* alarm_factory,
                        QuicPacketWriter* writer,
                        QuicCryptoClientConfig* crypto_config);

}  // namespace net

#endif  // PUIC_SESSION_INTERFACE_H_
