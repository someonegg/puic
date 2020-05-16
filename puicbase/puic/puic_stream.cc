// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic/puic_stream.h"

#include "puic/puic_session_base.h"
#include "net/quic/platform/api/quic_string_piece.h"

namespace net {

PuicStream::PuicStream(QuicStreamId id, QuicSession* session)
    : QuicStream(id, session) {}

PuicStream::~PuicStream() {}

void PuicStream::OnDataAvailable() {
  if (!closed_) {
    delegate_->OnCanRead(this);
  }
}

void PuicStream::OnClose() {
  closed_ = true;
  QuicStream::OnClose();
  DCHECK(delegate_);
  delegate_->OnClosed(this, stream_error(), !rst_sent());
}

void PuicStream::OnCanWrite() {
  QuicStream::OnCanWrite();
  DCHECK(delegate_);
  if (!closed_ && !write_side_closed() && !fin_buffered()) {
    delegate_->OnCanWrite(this);
  }
}

PuicStreamId PuicStream::StreamId() const {
  return id();
}

PuicSessionInterface* PuicStream::Session() {
  return static_cast<PuicSessionBase*>(session());
}

bool PuicStream::IsEOF() {
  bool eof = sequencer()->IsClosed();
  if (eof)
    StopReading();
  return eof;
}

size_t PuicStream::Read(char* data, size_t size)
{
  struct iovec iov;
  iov.iov_base = data;
  iov.iov_len = size;
  return static_cast<size_t>(sequencer()->Readv(&iov, 1));
}

void PuicStream::GetReadableRegions(const char* regs[],
                                    size_t lens[],
                                    int* count) {
  int getn = *count;
  if (getn > kMaxReadableRegionsCount)
    getn = kMaxReadableRegionsCount;

  struct iovec iovs[kMaxReadableRegionsCount];
  getn = sequencer()->GetReadableRegions(iovs, getn);

  for (int i = 0; i < getn; ++i) {
    regs[i] = reinterpret_cast<const char*>(iovs[i].iov_base);
    lens[i] = iovs[i].iov_len;
  }

  *count = getn;
}

void PuicStream::MarkConsumed(size_t num_bytes) {
  sequencer()->MarkConsumed(num_bytes);
}

bool PuicStream::FinSent() {
  return QuicStream::fin_sent();
}

void PuicStream::Write(const char* data, size_t size, bool fin) {
  WriteOrBufferData(QuicStringPiece(data, size), fin, nullptr);
}

size_t PuicStream::WriteBufferedAmount() {
  return size_t(BufferedDataBytes());
}

void PuicStream::SetDelegate(PuicStreamInterface::Delegate* delegate) {
  if (delegate_) {
    LOG(WARNING) << "The delegate for Stream " << id()
                 << " has already been set.";
  }
  delegate_ = delegate;
  DCHECK(delegate_);
}

}  // namespace net
