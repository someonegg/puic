// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic/puic_dispatcher.h"

#include "puic/puic_per_session_writer.h"
#include "net/quic/platform/api/quic_ptr_util.h"

namespace net {

namespace {

// An alarm that informs the PuicDispatcher to process buffered sessions.
class BufferedSessionsAlarm : public QuicAlarm::Delegate {
 public:
  explicit BufferedSessionsAlarm(PuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {}

  void OnAlarm() override { dispatcher_->ProcessBufferedSessions(); }

 private:
  // Not owned.
  PuicDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(BufferedSessionsAlarm);
};

}  // namespace

PuicDispatcher::PuicDispatcher(
    const QuicSocketAddress& server_address,
    const QuicConfig& config,
    const QuicVersionManager& version_manager,
    QuicCryptoServerConfig* crypto_config,
    QuicCryptoServerStream::Helper* session_helper,
    QuicConnectionHelperInterface* helper,
    QuicAlarmFactory* alarm_factory,
    QuicPacketWriter* writer,
    size_t max_new_sessions_per_sec)
    : QuicDispatcher(config, version_manager,
                     crypto_config, session_helper,
                     helper, alarm_factory, writer),
      server_address_(server_address),
      clock_(QuicDispatcher::helper()->GetClock()),
      max_new_sessions_per_sec_(max_new_sessions_per_sec),
      buffered_sessions_alarm_(
          alarm_factory->CreateAlarm(new BufferedSessionsAlarm(this))) {}

void PuicDispatcher::Close() {
  Shutdown();
  buffered_sessions_alarm_->Cancel();
}

void PuicDispatcher::OnTransportCanWrite() {
  OnCanWrite();
}

void PuicDispatcher::OnTransportReceived(const sockaddr* client_address,
                                         const char* data,
                                         size_t data_len) {
  QuicReceivedPacket packet(data, data_len, clock_->Now());
  ProcessUdpPacket(server_address_, QuicSocketAddress(client_address), packet);
}

void PuicDispatcher::SetDelegate(PuicDispatcherInterface::Delegate* delegate) {
  if (delegate_) {
    LOG(WARNING) << "The delegate for the dispatcher has already been set.";
  }
  delegate_ = delegate;
  DCHECK(delegate_);
}

void PuicDispatcher::ProcessBufferedSessions() {
  size_t max_per_100ms = max_new_sessions_per_sec_ / 10 + 1;
  ProcessBufferedChlos(max_per_100ms);
  buffered_sessions_alarm_->Update(clock_->ApproximateNow() +
                                      QuicTime::Delta::FromMilliseconds(100),
                                   QuicTime::Delta::Zero());
}

PuicServerSession* PuicDispatcher::CreateQuicSession(
    QuicConnectionId connection_id,
    const QuicSocketAddress& client_address,
    QuicStringPiece /*alpn*/) {
  // The PuicServerSession takes ownership of |connection| below.
  QuicConnection* connection = new QuicConnection(
      connection_id, client_address, helper(), alarm_factory(),
      CreatePerSessionWriter(), /* owns_writer= */ true,
      Perspective::IS_SERVER, GetSupportedVersions());
  connection->SetSelfAddress(server_address_);

  PuicServerSession* session = new PuicServerSession(
      connection, this, config(), crypto_config(), session_helper(),
      compressed_certs_cache());
  session->Initialize();

  DCHECK(delegate_);
  sockaddr_storage client_addr = client_address.generic_address();
  delegate_->OnNewSession(this, session,
      reinterpret_cast<const sockaddr*>(&client_addr));

  session->StartCryptoHandshake();

  return session;
}

QuicPacketWriter* PuicDispatcher::CreatePerSessionWriter() {
  return new PuicPerSessionWriter(writer());
}

namespace {

// Used by QuicCryptoServerConfig to provide dummy proof credentials.
class DummyProofSource : public ProofSource {
 public:
  DummyProofSource() {}
  ~DummyProofSource() override {}

  // ProofSource override.
  void GetProof(const QuicSocketAddress& server_addr,
                const std::string& hostname,
                const std::string& server_config,
                QuicTransportVersion transport_version,
                QuicStringPiece chlo_hash,
                std::unique_ptr<Callback> callback) override {
    QuicReferenceCountedPointer<ProofSource::Chain> chain;
    QuicCryptoProof proof;
    std::vector<std::string> certs;
    certs.push_back("Dummy cert");
    chain = new ProofSource::Chain(certs);
    proof.signature = "Dummy signature";
    proof.leaf_cert_scts = "Dummy timestamp";
    callback->Run(true, chain, proof, nullptr /* details */);
  }

  QuicReferenceCountedPointer<Chain> GetCertChain(
      const QuicSocketAddress& server_address,
      const std::string& hostname) override {
    return QuicReferenceCountedPointer<Chain>();
  }

  void ComputeTlsSignature(
      const QuicSocketAddress& server_address,
      const std::string& hostname,
      uint16_t signature_algorithm,
      QuicStringPiece in,
      std::unique_ptr<SignatureCallback> callback) override {
    callback->Run(true, "Dummy signature");
  }
};

// A helper class is used by the QuicCryptoServerStream.
class PuicCryptoServerStreamHelper : public QuicCryptoServerStream::Helper {
  QuicRandom* random_;
 public:
  PuicCryptoServerStreamHelper(QuicRandom* random) : random_(random) {}

  QuicConnectionId GenerateConnectionIdForReject(
      QuicConnectionId connection_id) const override {
    return random_->RandUint64();
  }

  bool CanAcceptClientHello(const CryptoHandshakeMessage& message,
                            const QuicSocketAddress& self_address,
                            std::string* error_details) const override {
    return true;
  }
};

class PuicDispatcherW : public PuicDispatcher {
public:
  PuicDispatcherW(
      const QuicSocketAddress& server_address,
      const QuicConfig& config,
      const QuicVersionManager& version_manager,
      std::unique_ptr<QuicCryptoServerConfig> crypto_config,
      std::unique_ptr<QuicCryptoServerStream::Helper> session_helper,
      QuicConnectionHelperInterface* helper,
      QuicAlarmFactory* alarm_factory,
      QuicPacketWriter* writer,
      size_t max_new_sessions_per_sec)
      : PuicDispatcher(server_address, config, version_manager,
                       crypto_config.get(), session_helper.get(),
                       helper, alarm_factory, writer,
                       max_new_sessions_per_sec),
        crypto_config_(std::move(crypto_config)),
        session_helper_(std::move(session_helper)) {}

private:
  std::unique_ptr<QuicCryptoServerConfig> crypto_config_;
  std::unique_ptr<QuicCryptoServerStream::Helper> session_helper_;
};

}  // namespace

PuicDispatcherInterface*
CreatePuicDispatcher(const PuicDispatcherConfig& create_config,
                     const sockaddr* server_address,
                     QuicConnectionHelperInterface* helper,
                     QuicAlarmFactory* alarm_factory,
                     QuicPacketWriter* writer) {
  QuicSocketAddress server_addr(server_address);
  QuicVersionManager versions(AllSupportedVersions());

  // Length of HKDF input keying material, equal to its number of bytes.
  // https://tools.ietf.org/html/rfc5869#section-2.2.
  const size_t kInputKeyingMaterialLength = 32;
  char source_address_token_secret[kInputKeyingMaterialLength];
  helper->GetRandomGenerator()->RandBytes(source_address_token_secret,
                                          kInputKeyingMaterialLength);

  QuicConfig config;
  {
    config.set_max_time_before_crypto_handshake(
        QuicTime::Delta::FromSeconds(create_config.handshake_timeout_secs));

    config.SetIdleNetworkTimeout(
        QuicTime::Delta::FromSeconds(create_config.max_idle_timeout_secs),
        QuicTime::Delta::FromSeconds(create_config.default_idle_timeout_secs));

    config.SetInitialStreamFlowControlWindowToSend(
      create_config.initial_stream_window);
    config.SetInitialSessionFlowControlWindowToSend(
      create_config.initial_session_window);
  }

  auto crypto_config = QuicMakeUnique<QuicCryptoServerConfig>(
      source_address_token_secret,
      helper->GetRandomGenerator(),
      QuicMakeUnique<DummyProofSource>());
  {
    // Provide server with serialized config string to prove ownership.
    QuicCryptoServerConfig::ConfigOptions options;
    // The |message| is used to handle the return value of AddDefaultConfig
    // which is raw pointer of the CryptoHandshakeMessage.
    std::unique_ptr<CryptoHandshakeMessage> scfg(
        crypto_config->AddDefaultConfig(
            helper->GetRandomGenerator(), helper->GetClock(), options));
  }

  auto crypto_stream_helper = QuicMakeUnique<PuicCryptoServerStreamHelper>(
      helper->GetRandomGenerator());

  PuicDispatcher* dispatcher = new PuicDispatcherW(
      server_addr, config, versions,
      std::move(crypto_config), std::move(crypto_stream_helper),
      helper, alarm_factory, writer,
      create_config.max_new_sessions_per_sec);

  // Start the process of buffered sessions.
  dispatcher->ProcessBufferedSessions();

  return dispatcher;
}

}  // namespace net