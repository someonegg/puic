// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic/puic_client_session.h"

#include "puic/puic_per_session_writer.h"
#include "net/quic/core/quic_version_manager.h"
#include "net/quic/platform/api/quic_ptr_util.h"

using std::string;

namespace net {

PuicClientSession::PuicClientSession(QuicConnection* connection,
                                     QuicSession::Visitor* owner,
                                     const QuicConfig& config,
                                     const QuicServerId& server_id,
                                     QuicCryptoClientConfig* crypto_config)
    : PuicSessionBase(connection, owner, config),
      server_id_(server_id),
      crypto_config_(crypto_config) {}

PuicClientSession::~PuicClientSession() {}

const QuicCryptoClientStreamBase* PuicClientSession::GetCryptoStream() const {
  return crypto_stream_.get();
}

QuicCryptoClientStreamBase* PuicClientSession::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

void PuicClientSession::Initialize() {
  crypto_stream_ = CreateQuicCryptoStream();
  PuicSessionBase::Initialize();
}

void PuicClientSession::StartCryptoHandshake() {
  crypto_stream_->CryptoConnect();
}

void PuicClientSession::OnProofValid(
    const QuicCryptoClientConfig::CachedState& cached) {
}

void PuicClientSession::OnProofVerifyDetailsAvailable(
    const ProofVerifyDetails& verify_details) {
}

std::unique_ptr<QuicCryptoClientStreamBase>
PuicClientSession::CreateQuicCryptoStream() {
  return QuicMakeUnique<QuicCryptoClientStream>(
      server_id_, this, new ProofVerifyContext(),
      crypto_config_, this);
}

namespace {

// Used by QuicCryptoClientConfig to ignore the peer's credentials
// and establish an insecure QUIC connection.
class InsecureProofVerifier : public ProofVerifier {
 public:
  InsecureProofVerifier() {}
  ~InsecureProofVerifier() override {}

  // ProofVerifier override.
  QuicAsyncStatus VerifyProof(
      const string& hostname,
      const uint16_t port,
      const string& server_config,
      QuicTransportVersion quic_version,
      QuicStringPiece chlo_hash,
      const std::vector<string>& certs,
      const string& cert_sct,
      const string& signature,
      const ProofVerifyContext* context,
      string* error_details,
      std::unique_ptr<ProofVerifyDetails>* verify_details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    return QUIC_SUCCESS;
  }

  QuicAsyncStatus VerifyCertChain(
      const string& hostname,
      const std::vector<string>& certs,
      const ProofVerifyContext* context,
      string* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    return QUIC_SUCCESS;
  }
};

}  // namespace

QuicCryptoClientConfig* CreateQuicCryptoClientConfig() {
  return new QuicCryptoClientConfig(
    QuicMakeUnique<InsecureProofVerifier>());
}

PuicSessionInterface*
CreatePuicClientSession(const PuicClientSessionConfig& create_config,
                        const std::string& server_host,
                        const sockaddr* client_address,
                        const sockaddr* server_address,
                        QuicConnectionHelperInterface* helper,
                        QuicAlarmFactory* alarm_factory,
                        QuicPacketWriter* writer,
                        QuicCryptoClientConfig* crypto_config) {
  QuicSocketAddress client_addr(client_address);
  QuicSocketAddress server_addr(server_address);
  QuicServerId server_id(server_host.empty() ?
                          server_addr.host().ToString() : server_host,
                         server_addr.port(),
                         server_host.empty());
  QuicVersionManager versions(AllSupportedVersions());

  QuicConfig config;
  {
    QuicTagVector copt;
    copt.push_back(kTBBR);
    //copt.push_back(k5RTO);
    config.SetConnectionOptionsToSend(copt);
    config.SetClientConnectionOptions(copt);

    config.set_max_time_before_crypto_handshake(
        QuicTime::Delta::FromSeconds(create_config.handshake_timeout_secs));

    config.SetIdleNetworkTimeout(
        QuicTime::Delta::FromSeconds(create_config.idle_timeout_secs),
        QuicTime::Delta::FromSeconds(create_config.idle_timeout_secs));

    config.SetInitialStreamFlowControlWindowToSend(
      create_config.stream_window);
    config.SetInitialSessionFlowControlWindowToSend(
      create_config.session_window);
  }

  QuicConnectionId connection_id = helper->GetRandomGenerator()->RandUint64();
  QuicConnection* connection = new QuicConnection(
      connection_id, server_addr, helper, alarm_factory,
      new PuicPerSessionWriter(writer), /* owns_writer= */ true,
      Perspective::IS_CLIENT, versions.GetSupportedVersions());
  connection->SetSelfAddress(client_addr);

  PuicClientSession* session = new PuicClientSession(
      connection, nullptr, config, server_id, crypto_config);
  session->Initialize();

  return session;
}

}  // namespace net
