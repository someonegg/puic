// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_STREAM_H_
#define PUIC_STREAM_H_

#include "puic/puic_stream_interface.h"

#include "net/quic/core/quic_session.h"
#include "net/quic/core/quic_stream.h"

namespace net {

class PuicStream : public QuicStream,
                   public PuicStreamInterface {
 public:
  PuicStream(QuicStreamId id, QuicSession* session);

  ~PuicStream() override;

  void OnDataAvailable() override;

  void OnClose() override;

  void OnCanWrite() override;

  PuicStreamId StreamId() const override;

  PuicSessionInterface* Session() override;

  bool IsEOF() override;

  size_t Read(char* data, size_t size) override;

  void GetReadableRegions(const char* regs[],
                          size_t lens[],
                          int *count) override;
  void MarkConsumed(size_t num_bytes) override;

  bool FinSent() override;

  void Write(const char* data, size_t size, bool fin) override;

  size_t WriteBufferedAmount() override;

  void SetDelegate(PuicStreamInterface::Delegate* delegate) override;

 private:
  bool closed_ = false;
  PuicStreamInterface::Delegate* delegate_ = nullptr;
};

}  // namespace net

#endif  // PUIC_STREAM_H_
