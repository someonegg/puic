// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_PER_SESSION_WRITER_H_
#define PUIC_PER_SESSION_WRITER_H_

#include <stddef.h>

#include "base/macros.h"
#include "net/quic/core/quic_connection.h"
#include "net/quic/core/quic_packet_writer.h"

namespace net {

class PuicPerSessionWriter : public QuicPacketWriter {
 public:
  PuicPerSessionWriter(QuicPacketWriter* lower_writer /* not owned */);
  ~PuicPerSessionWriter() override;

  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options) override;
  bool IsWriteBlockedDataBuffered() const override;
  bool IsWriteBlocked() const override;
  void SetWritable() override;
  QuicByteCount GetMaxPacketSize(
      const QuicSocketAddress& peer_address) const override;

 private:
  bool blocked_;
  QuicPacketWriter* lower_writer_;

  DISALLOW_COPY_AND_ASSIGN(PuicPerSessionWriter);
};

}  // namespace net

#endif  // PUIC_PER_SESSION_WRITER_H_
