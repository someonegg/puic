// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic/puic_per_session_writer.h"

#include <cassert>

namespace net {

PuicPerSessionWriter::PuicPerSessionWriter(
    QuicPacketWriter* lower_writer)
    : blocked_(false), lower_writer_(lower_writer) {}

PuicPerSessionWriter::~PuicPerSessionWriter() = default;

WriteResult PuicPerSessionWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const QuicIpAddress& self_address,
    const QuicSocketAddress& peer_address,
    PerPacketOptions* options) {
  WriteResult result = lower_writer_->WritePacket(
    buffer, buf_len, self_address, peer_address, options);
  if (result.status == WRITE_STATUS_BLOCKED) {
    blocked_ = true;
    assert(lower_writer_->IsWriteBlocked());
  }
  return result;
}

bool PuicPerSessionWriter::IsWriteBlockedDataBuffered() const {
  return lower_writer_->IsWriteBlockedDataBuffered();
}

bool PuicPerSessionWriter::IsWriteBlocked() const {
  return blocked_;
}

void PuicPerSessionWriter::SetWritable() {
  blocked_ = false;
}

QuicByteCount PuicPerSessionWriter::GetMaxPacketSize(
    const QuicSocketAddress& peer_address) const {
  return lower_writer_->GetMaxPacketSize(peer_address);
}

}  // namespace net
