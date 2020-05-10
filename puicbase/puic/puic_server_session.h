// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_SERVER_SESSION_H_
#define PUIC_SERVER_SESSION_H_

#include "puic/puic_session_base.h"

#include "net/quic/core/quic_crypto_server_stream.h"

namespace net {

class PuicServerSession : public PuicSessionBase {
 public:
  PuicServerSession(QuicConnection* connection,
                    QuicSession::Visitor* owner,
                    const QuicConfig& config,
                    const QuicCryptoServerConfig* crypto_config,
                    QuicCryptoServerStream::Helper* session_helper,
                    QuicCompressedCertsCache* compressed_certs_cache);
  ~PuicServerSession() override;

  void Initialize() override;

  QuicCryptoServerStreamBase* GetMutableCryptoStream() override;

  const QuicCryptoServerStreamBase* GetCryptoStream() const override;

  void StartCryptoHandshake() override;

protected:
  const QuicCryptoServerConfig* crypto_config() const { return crypto_config_; }

  QuicCryptoServerStream::Helper* session_helper() const { return session_helper_; }

  QuicCompressedCertsCache* compressed_certs_cache() const { return compressed_certs_cache_; }

  // Create the crypto stream.
  virtual std::unique_ptr<QuicCryptoServerStreamBase> CreateQuicCryptoStream();

 private:
  const QuicCryptoServerConfig* crypto_config_;
  QuicCryptoServerStream::Helper* session_helper_;
  QuicCompressedCertsCache* compressed_certs_cache_;

  std::unique_ptr<QuicCryptoServerStreamBase> crypto_stream_;

  DISALLOW_COPY_AND_ASSIGN(PuicServerSession);
};

}  // namespace net

#endif  // PUIC_SERVER_SESSION_H_
