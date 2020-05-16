// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_DISPATCHER_INTERFACE_H_
#define PUIC_DISPATCHER_INTERFACE_H_

#include <cstddef>

#include "puic/puic_session_interface.h"

namespace net {

// A server side dispatcher which dispatches a given client's data to their
// session.
class PuicDispatcherInterface {
 public:
  virtual ~PuicDispatcherInterface() {}

  virtual void Close() = 0;

  virtual void OnTransportCanWrite() = 0;

  virtual void OnTransportReceived(const sockaddr* client_address,
                                   const char* data,
                                   size_t data_len) = 0;

  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnNewSession(PuicDispatcherInterface* dispatcher,
                              PuicSessionInterface* session,
                              const sockaddr* client_address) = 0;
  };

  // The |delegate| is not owned by PuicDispatcher.
  virtual void SetDelegate(Delegate* delegate) = 0;
};

struct PuicDispatcherConfig {
  size_t handshake_timeout_secs = 5;

  size_t max_idle_timeout_secs = 120;

  size_t default_idle_timeout_secs = 50;

  size_t initial_stream_window = 512 * 1024;

  size_t initial_session_window = 768 * 1024;

  size_t max_new_sessions_per_sec = 100;
};

class QuicConnectionHelperInterface;
class QuicAlarmFactory;
class QuicPacketWriter;

PuicDispatcherInterface*
CreatePuicDispatcher(const PuicDispatcherConfig& create_config,
                     const sockaddr* server_address,
                     QuicConnectionHelperInterface* helper,
                     QuicAlarmFactory* alarm_factory,
                     QuicPacketWriter* writer);

}  // namespace net

#endif  // PUIC_DISPATCHER_INTERFACE_H_
