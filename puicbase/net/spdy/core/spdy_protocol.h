// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <PUIC-PATCH>

// This file contains some protocol structures for use with SPDY 3 and HTTP 2
// The SPDY 3 spec can be found at:
// http://dev.chromium.org/spdy/spdy-protocol/spdy-protocol-draft3

#ifndef NET_SPDY_CORE_SPDY_PROTOCOL_H_
#define NET_SPDY_CORE_SPDY_PROTOCOL_H_

#include <stddef.h>
#include <stdint.h>

#include <iosfwd>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <utility>

#include "net/spdy/platform/api/spdy_export.h"

namespace net {

// A stream id is a 31 bit entity.
typedef uint32_t SpdyStreamId;

// Specifies the stream ID used to denote the current session (for
// flow control).
const SpdyStreamId kSessionFlowControlStreamId = 0;

// Max stream id.
const SpdyStreamId kMaxStreamId = 0x7fffffff;

// A SPDY priority is a number between 0 and 7 (inclusive).
typedef uint8_t SpdyPriority;

// Lowest and Highest here refer to SPDY priorities as described in
// https://www.chromium.org/spdy/spdy-protocol/spdy-protocol-draft3-1#TOC-2.3.3-Stream-priority
const SpdyPriority kV3HighestPriority = 0;
const SpdyPriority kV3LowestPriority = 7;

// Returns SPDY 3.x priority value clamped to the valid range of [0, 7].
SPDY_EXPORT_PRIVATE SpdyPriority ClampSpdy3Priority(SpdyPriority priority);

// HTTP/2 stream weights are integers in range [1, 256], as specified in RFC
// 7540 section 5.3.2. Default stream weight is defined in section 5.3.5.
const int kHttp2MinStreamWeight = 1;
const int kHttp2MaxStreamWeight = 256;
const int kHttp2DefaultStreamWeight = 16;

// Returns HTTP/2 weight clamped to the valid range of [1, 256].
SPDY_EXPORT_PRIVATE int ClampHttp2Weight(int weight);

// Maps SPDY 3.x priority value in range [0, 7] to HTTP/2 weight value in range
// [1, 256], where priority 0 (i.e. highest precedence) corresponds to maximum
// weight 256 and priority 7 (lowest precedence) corresponds to minimum weight
// 1.
SPDY_EXPORT_PRIVATE int Spdy3PriorityToHttp2Weight(SpdyPriority priority);

// Maps HTTP/2 weight value in range [1, 256] to SPDY 3.x priority value in
// range [0, 7], where minimum weight 1 corresponds to priority 7 (lowest
// precedence) and maximum weight 256 corresponds to priority 0 (highest
// precedence).
SPDY_EXPORT_PRIVATE SpdyPriority Http2WeightToSpdy3Priority(int weight);

// Reserved ID for root stream of HTTP/2 stream dependency tree, as specified
// in RFC 7540 section 5.3.1.
const unsigned int kHttp2RootStreamId = 0;

// Variant type (i.e. tagged union) that is either a SPDY 3.x priority value,
// or else an HTTP/2 stream dependency tuple {parent stream ID, weight,
// exclusive bit}. Templated to allow for use by QUIC code; SPDY and HTTP/2
// code should use the concrete type instantiation SpdyStreamPrecedence.
template <typename StreamIdType>
class StreamPrecedence {
 public:
  // Constructs instance that is a SPDY 3.x priority. Clamps priority value to
  // the valid range [0, 7].
  explicit StreamPrecedence(SpdyPriority priority)
      : is_spdy3_priority_(true),
        spdy3_priority_(ClampSpdy3Priority(priority)) {}

  // Constructs instance that is an HTTP/2 stream weight, parent stream ID, and
  // exclusive bit. Clamps stream weight to the valid range [1, 256].
  StreamPrecedence(StreamIdType parent_id, int weight, bool is_exclusive)
      : is_spdy3_priority_(false),
        http2_stream_dependency_{parent_id, ClampHttp2Weight(weight),
                                 is_exclusive} {}

  // Intentionally copyable, to support pass by value.
  StreamPrecedence(const StreamPrecedence& other) = default;
  StreamPrecedence& operator=(const StreamPrecedence& other) = default;

  // Returns true if this instance is a SPDY 3.x priority, or false if this
  // instance is an HTTP/2 stream dependency.
  bool is_spdy3_priority() const { return is_spdy3_priority_; }

  // Returns SPDY 3.x priority value. If |is_spdy3_priority()| is true, this is
  // the value provided at construction, clamped to the legal priority
  // range. Otherwise, it is the HTTP/2 stream weight mapped to a SPDY 3.x
  // priority value, where minimum weight 1 corresponds to priority 7 (lowest
  // precedence) and maximum weight 256 corresponds to priority 0 (highest
  // precedence).
  SpdyPriority spdy3_priority() const {
    return is_spdy3_priority_
               ? spdy3_priority_
               : Http2WeightToSpdy3Priority(http2_stream_dependency_.weight);
  }

  // Returns HTTP/2 parent stream ID. If |is_spdy3_priority()| is false, this is
  // the value provided at construction, otherwise it is |kHttp2RootStreamId|.
  StreamIdType parent_id() const {
    return is_spdy3_priority_ ? kHttp2RootStreamId
                              : http2_stream_dependency_.parent_id;
  }

  // Returns HTTP/2 stream weight. If |is_spdy3_priority()| is false, this is
  // the value provided at construction, clamped to the legal weight
  // range. Otherwise, it is the SPDY 3.x priority value mapped to an HTTP/2
  // stream weight, where priority 0 (i.e. highest precedence) corresponds to
  // maximum weight 256 and priority 7 (lowest precedence) corresponds to
  // minimum weight 1.
  int weight() const {
    return is_spdy3_priority_ ? Spdy3PriorityToHttp2Weight(spdy3_priority_)
                              : http2_stream_dependency_.weight;
  }

  // Returns HTTP/2 parent stream exclusivity. If |is_spdy3_priority()| is
  // false, this is the value provided at construction, otherwise it is false.
  bool is_exclusive() const {
    return !is_spdy3_priority_ && http2_stream_dependency_.is_exclusive;
  }

  // Facilitates test assertions.
  bool operator==(const StreamPrecedence& other) const {
    if (is_spdy3_priority()) {
      return other.is_spdy3_priority() &&
             (spdy3_priority() == other.spdy3_priority());
    } else {
      return !other.is_spdy3_priority() && (parent_id() == other.parent_id()) &&
             (weight() == other.weight()) &&
             (is_exclusive() == other.is_exclusive());
    }
  }

  bool operator!=(const StreamPrecedence& other) const {
    return !(*this == other);
  }

 private:
  struct Http2StreamDependency {
    StreamIdType parent_id;
    int weight;
    bool is_exclusive;
  };

  bool is_spdy3_priority_;
  union {
    SpdyPriority spdy3_priority_;
    Http2StreamDependency http2_stream_dependency_;
  };
};

typedef StreamPrecedence<SpdyStreamId> SpdyStreamPrecedence;

}  // namespace net

#endif  // NET_SPDY_CORE_SPDY_PROTOCOL_H_
