// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_CLIENT_SESSION_H_
#define PUIC_CLIENT_SESSION_H_

#include "puic/puic_session_base.h"

#include "net/quic/core/quic_crypto_client_stream.h"

namespace net {

class PuicClientSession
    : public PuicSessionBase,
      public QuicCryptoClientStream::ProofHandler {
 public:
  PuicClientSession(QuicConnection* connection,
                    QuicSession::Visitor* owner,
                    const QuicConfig& config,
                    const QuicServerId& server_id,
                    QuicCryptoClientConfig* crypto_config);
  ~PuicClientSession() override;

  void Initialize() override;

  QuicCryptoClientStreamBase* GetMutableCryptoStream() override;

  const QuicCryptoClientStreamBase* GetCryptoStream() const override;

  void StartCryptoHandshake() override;

  void OnProofValid(const QuicCryptoClientConfig::CachedState& cached) override;

  void OnProofVerifyDetailsAvailable(
      const ProofVerifyDetails& verify_details) override;

 protected:
  QuicServerId server_id() const { return server_id_; }

  QuicCryptoClientConfig* crypto_config() const { return crypto_config_; }

  // Create the crypto stream.
  virtual std::unique_ptr<QuicCryptoClientStreamBase> CreateQuicCryptoStream();

 private:
  QuicServerId server_id_;

  QuicCryptoClientConfig* crypto_config_;
  std::unique_ptr<QuicCryptoClientStreamBase> crypto_stream_;

  DISALLOW_COPY_AND_ASSIGN(PuicClientSession);
};

}  // namespace net

#endif  // PUIC_CLIENT_SESSION_H_
