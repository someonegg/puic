// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <PUIC-PATCH>

#include "net/spdy/core/spdy_protocol.h"

#include "net/spdy/core/spdy_bug_tracker.h"

namespace net {

SpdyPriority ClampSpdy3Priority(SpdyPriority priority) {
  if (priority < kV3HighestPriority) {
    SPDY_BUG << "Invalid priority: " << static_cast<int>(priority);
    return kV3HighestPriority;
  }
  if (priority > kV3LowestPriority) {
    SPDY_BUG << "Invalid priority: " << static_cast<int>(priority);
    return kV3LowestPriority;
  }
  return priority;
}

int ClampHttp2Weight(int weight) {
  if (weight < kHttp2MinStreamWeight) {
    SPDY_BUG << "Invalid weight: " << weight;
    return kHttp2MinStreamWeight;
  }
  if (weight > kHttp2MaxStreamWeight) {
    SPDY_BUG << "Invalid weight: " << weight;
    return kHttp2MaxStreamWeight;
  }
  return weight;
}

int Spdy3PriorityToHttp2Weight(SpdyPriority priority) {
  priority = ClampSpdy3Priority(priority);
  const float kSteps = 255.9f / 7.f;
  return static_cast<int>(kSteps * (7.f - priority)) + 1;
}

SpdyPriority Http2WeightToSpdy3Priority(int weight) {
  weight = ClampHttp2Weight(weight);
  const float kSteps = 255.9f / 7.f;
  return static_cast<SpdyPriority>(7.f - (weight - 1) / kSteps);
}

}  // namespace net
