// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_STREAM_INTERFACE_H_
#define PUIC_STREAM_INTERFACE_H_

#include <cstddef>
#include <cstdint>

struct iovec;

namespace net {

typedef uint32_t PuicStreamId;

// A PUIC priority is a number between 0 and 7 (inclusive).
typedef uint8_t PuicPriority;

const PuicPriority kDefaultPriority = 3;
const PuicPriority kHighestPriority = 0;

// The preset stream's ID.
// The preset stream's priority is highest (0).
const PuicStreamId kPresetStreamId = 3;

const int kMaxReadableRegionsCount = 16;

class PuicSessionInterface;

// Sends and receives data with a particular PUIC stream ID, reliably
// and in-order.
class PuicStreamInterface {
 public:
  virtual ~PuicStreamInterface() {}

  virtual PuicStreamId StreamId() const = 0;

  virtual PuicSessionInterface* Session() = 0;

  virtual bool IsEOF() = 0;

  virtual size_t Read(char* data, size_t size) = 0;

  // do zero-copy read, max count is kMaxReadableRegionsCount.
  virtual void GetReadableRegions(const char* regs[],
                                  size_t lens[],
                                  int* count) = 0;
  virtual void MarkConsumed(size_t num_bytes) = 0;

  virtual bool FinSent() = 0;

  virtual void Write(const char* data, size_t size, bool fin) = 0;

  virtual size_t WriteBufferedAmount() = 0;

  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnCanRead(PuicStreamInterface* stream) = 0;

    virtual void OnCanWrite(PuicStreamInterface* stream) = 0;

    // The stream should not be accessed after OnClosed is called.
    virtual void OnClosed(PuicStreamInterface* stream,
                          int stream_error,
                          bool from_remote) = 0;
  };

  // The |delegate| is not owned by PuicStream.
  virtual void SetDelegate(Delegate* delegate) = 0;

  bool IsPresetStream() { return StreamId() == kPresetStreamId; }
};

}  // namespace net

#endif  // PUIC_STREAM_INTERFACE_H_
