// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic/puic_server_session.h"

#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_ptr_util.h"

namespace net {

PuicServerSession::PuicServerSession(
    QuicConnection* connection,
    QuicSession::Visitor* owner,
    const QuicConfig& config,
    const QuicCryptoServerConfig* crypto_config,
    QuicCryptoServerStream::Helper* session_helper,
    QuicCompressedCertsCache* compressed_certs_cache)
    : PuicSessionBase(connection, owner, config),
      crypto_config_(crypto_config),
      session_helper_(session_helper),
      compressed_certs_cache_(compressed_certs_cache) {}

PuicServerSession::~PuicServerSession() {}

void PuicServerSession::Initialize() {
  crypto_stream_ = CreateQuicCryptoStream();
  PuicSessionBase::Initialize();
}

const QuicCryptoServerStreamBase* PuicServerSession::GetCryptoStream() const {
  return crypto_stream_.get();
}

QuicCryptoServerStreamBase* PuicServerSession::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

void PuicServerSession::StartCryptoHandshake() {
}

std::unique_ptr<QuicCryptoServerStreamBase>
PuicServerSession::CreateQuicCryptoStream() {
  return QuicMakeUnique<QuicCryptoServerStream>(
      crypto_config_, compressed_certs_cache_,
      FLAGS_quic_reloadable_flag_enable_quic_stateless_reject_support,
      this, session_helper_);
}

}  // namespace net
