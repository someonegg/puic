
if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(C_CXX_FLAGS "-Wall")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ${C_CXX_FLAGS}")
elseif (MSVC)
	add_definitions(/wd4244 /wd4267 /wd4800 /wd4996)
endif ()

if (WIN32)
	add_definitions(-DNOMINMAX)
	add_definitions(-DUNICODE)
	add_definitions(-D_UNICODE)
endif ()

include_directories(. ./third_party/modp_b64 .. ../third_party/protobuf/3.3.0/src)

if (NOT PUIC_NO_EMBEDDED_BORINGSSL)
	include_directories(../third_party/boringssl/include)
endif ()

set(
	BASE_SOURCES

	base/base64.cc
	base/callback_internal.cc
	base/hash.cc
	base/location.cc
	base/logging.cc
	base/vlog.cc
	base/memory/aligned_memory.cc
	base/rand_util.cc
	base/strings/string_number_conversions.cc
	base/strings/string_piece.cc
	base/strings/string_split.cc
	base/strings/string_util.cc
	base/strings/string_util_constants.cc
	base/strings/stringprintf.cc
	base/strings/utf_string_conversions.cc
	base/strings/utf_string_conversion_utils.cc
	base/trace_event/memory_usage_estimator.cc

	base/third_party/dmg_fp/dtoa.cc
	base/third_party/dmg_fp/g_fmt.cc
	base/third_party/icu/icu_utf.cc
	base/third_party/superfasthash/superfasthash.c
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(
		BASE_SYSTEM_SOURCES

		base/strings/string16.cc
		base/rand_util_posix.cc
		base/posix/safe_strerror.cc
	)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Android")
	set(
		BASE_SYSTEM_SOURCES

		base/strings/string16.cc
		base/rand_util_posix.cc
		base/posix/safe_strerror.cc
	)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(
		BASE_SYSTEM_SOURCES

		base/strings/string16.cc
		base/rand_util_posix.cc
		base/posix/safe_strerror.cc
	)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(
		BASE_SYSTEM_SOURCES

		base/rand_util_win.cc
	)
endif ()

set(
	CRYPTO_SOURCES

	crypto/hkdf.cc
	crypto/hmac.cc
	crypto/openssl_util.cc
	crypto/random.cc
	crypto/secure_util.cc
	crypto/symmetric_key.cc
)

set(
	URL_SOURCES

	url/url_canon_internal.cc
	url/url_canon_ip.cc
	url/url_canon_stdstring.cc

	url/third_party/mozilla/url_parse.cc
)

set(
	NET_SOURCES

	net/base/address_family.cc
	net/base/hex_utils.cc
	net/base/host_port_pair.cc
	net/base/int128.cc
	net/base/io_buffer.cc
	net/base/ip_address.cc
	net/base/ip_endpoint.cc
	net/base/net_errors.cc
	net/base/parse_number.cc
	net/base/sockaddr_storage.cc
)

set(
	QUIC_SOURCES

	net/quic/core/congestion_control/bandwidth_sampler.cc
	net/quic/core/congestion_control/bbr_sender.cc
	net/quic/core/congestion_control/cubic_bytes.cc
	net/quic/core/congestion_control/general_loss_algorithm.cc
	net/quic/core/congestion_control/hybrid_slow_start.cc
	net/quic/core/congestion_control/pacing_sender.cc
	net/quic/core/congestion_control/prr_sender.cc
	net/quic/core/congestion_control/rtt_stats.cc
	net/quic/core/congestion_control/send_algorithm_interface.cc
	net/quic/core/congestion_control/tcp_cubic_sender_base.cc
	net/quic/core/congestion_control/tcp_cubic_sender_bytes.cc
	net/quic/core/crypto/aead_base_decrypter.cc
	net/quic/core/crypto/aead_base_encrypter.cc
	net/quic/core/crypto/aes_128_gcm_12_decrypter.cc
	net/quic/core/crypto/aes_128_gcm_12_encrypter.cc
	net/quic/core/crypto/aes_128_gcm_decrypter.cc
	net/quic/core/crypto/aes_128_gcm_encrypter.cc
	net/quic/core/crypto/aes_256_gcm_decrypter.cc
	net/quic/core/crypto/aes_256_gcm_encrypter.cc
	net/quic/core/crypto/chacha20_poly1305_decrypter.cc
	net/quic/core/crypto/chacha20_poly1305_encrypter.cc
	net/quic/core/crypto/chacha20_poly1305_tls_decrypter.cc
	net/quic/core/crypto/chacha20_poly1305_tls_encrypter.cc
	net/quic/core/crypto/channel_id.cc
	net/quic/core/crypto/cert_compressor.cc
	net/quic/core/crypto/common_cert_set.cc
	net/quic/core/crypto/crypto_framer.cc
	net/quic/core/crypto/crypto_handshake.cc
	net/quic/core/crypto/crypto_handshake_message.cc
	net/quic/core/crypto/crypto_secret_boxer.cc
	net/quic/core/crypto/crypto_server_config_protobuf.cc
	net/quic/core/crypto/crypto_utils.cc
	net/quic/core/crypto/curve25519_key_exchange.cc
	net/quic/core/crypto/null_decrypter.cc
	net/quic/core/crypto/null_encrypter.cc
	net/quic/core/crypto/p256_key_exchange.cc
	net/quic/core/crypto/proof_source.cc
	net/quic/core/crypto/quic_compressed_certs_cache.cc
	net/quic/core/crypto/quic_crypto_client_config.cc
	net/quic/core/crypto/quic_crypto_proof.cc
	net/quic/core/crypto/quic_crypto_server_config.cc
	net/quic/core/crypto/quic_decrypter.cc
	net/quic/core/crypto/quic_encrypter.cc
	net/quic/core/crypto/quic_random.cc
	net/quic/core/crypto/scoped_evp_aead_ctx.cc
	net/quic/core/frames/quic_ack_frame.cc
	net/quic/core/frames/quic_blocked_frame.cc
	net/quic/core/frames/quic_connection_close_frame.cc
	net/quic/core/frames/quic_frame.cc
	net/quic/core/frames/quic_goaway_frame.cc
	net/quic/core/frames/quic_padding_frame.cc
	net/quic/core/frames/quic_ping_frame.cc
	net/quic/core/frames/quic_rst_stream_frame.cc
	net/quic/core/frames/quic_stop_waiting_frame.cc
	net/quic/core/frames/quic_stream_frame.cc
	net/quic/core/frames/quic_window_update_frame.cc

	net/quic/core/quic_ack_listener_interface.cc
	net/quic/core/quic_alarm.cc
	net/quic/core/quic_bandwidth.cc
	net/quic/core/quic_buffer_allocator.cc
	net/quic/core/quic_buffered_packet_store.cc
	net/quic/core/quic_config.cc
	net/quic/core/quic_connection.cc
	net/quic/core/quic_connection_stats.cc
	net/quic/core/quic_constants.cc
	net/quic/core/quic_crypto_client_handshaker.cc
	net/quic/core/quic_crypto_server_handshaker.cc
	net/quic/core/quic_crypto_handshaker.cc
	net/quic/core/quic_crypto_client_stream.cc
	net/quic/core/quic_crypto_server_stream.cc
	net/quic/core/quic_crypto_stream.cc
	net/quic/core/quic_data_reader.cc
	net/quic/core/quic_data_writer.cc
	net/quic/core/quic_error_codes.cc
	net/quic/core/quic_flow_controller.cc
	net/quic/core/quic_framer.cc
	net/quic/core/quic_packet_creator.cc
	net/quic/core/quic_packet_generator.cc
	net/quic/core/quic_packets.cc
	net/quic/core/quic_received_packet_manager.cc
	net/quic/core/quic_sent_packet_manager.cc
	net/quic/core/quic_server_id.cc
	net/quic/core/quic_session.cc
	net/quic/core/quic_simple_buffer_allocator.cc
	net/quic/core/quic_socket_address_coder.cc
	net/quic/core/quic_stream.cc
	net/quic/core/quic_stream_sequencer.cc
	net/quic/core/quic_stream_sequencer_buffer.cc
	net/quic/core/quic_sustained_bandwidth_recorder.cc
	net/quic/core/quic_tag.cc
	net/quic/core/quic_time.cc
	net/quic/core/quic_transmission_info.cc
	net/quic/core/quic_types.cc
	net/quic/core/quic_unacked_packet_map.cc
	net/quic/core/quic_utils.cc
	net/quic/core/quic_version_manager.cc
	net/quic/core/quic_versions.cc
	net/quic/core/quic_write_blocked_list.cc
	net/quic/core/quic_stream_send_buffer.cc

	net/quic/platform/api/quic_clock.cc
	net/quic/platform/api/quic_ip_address.cc
	net/quic/platform/api/quic_socket_address.cc
	net/quic/platform/impl/quic_flags_impl.cc
	net/quic/platform/impl/quic_ip_address_impl.cc
	net/quic/platform/impl/quic_mem_slice_impl.cc
	net/quic/platform/impl/quic_mem_slice_span_impl.cc
	net/quic/platform/impl/quic_socket_address_impl.cc

	net/quic/core/proto/cached_network_parameters.pb.cc
	net/quic/core/proto/source_address_token.pb.cc

	net/spdy/core/spdy_protocol.cc

	net/tools/quic/chlo_extractor.cc
	net/tools/quic/quic_dispatcher.cc
	net/tools/quic/quic_per_connection_packet_writer.cc
	net/tools/quic/quic_time_wait_list_manager.cc
	net/tools/quic/stateless_rejector.cc
)

set(
	PUIC_SOURCES

	puic/puic_per_session_writer.cc
	puic/puic_stream.cc
	puic/puic_session_base.cc
	puic/puic_client_session.cc
	puic/puic_server_session.cc
	puic/puic_dispatcher.cc
)

add_library(
	puicbase
	STATIC

	${BASE_SOURCES}
	${BASE_SYSTEM_SOURCES}
	${CRYPTO_SOURCES}
	${URL_SOURCES}
	${NET_SOURCES}
	${QUIC_SOURCES}
	${PUIC_SOURCES}

	third_party/modp_b64/modp_b64.cc
	third_party/zlib/gzwrite.c
	third_party/zlib/crc32.c
	third_party/zlib/zutil.c
	third_party/zlib/deflate.c
	third_party/zlib/simd_stub.c
	third_party/zlib/infback.c
	third_party/zlib/inffast.c
	third_party/zlib/inflate.c
	third_party/zlib/inftrees.c
	third_party/zlib/compress.c
	third_party/zlib/gzclose.c
	third_party/zlib/adler32.c
	third_party/zlib/gzlib.c
	third_party/zlib/trees.c
	third_party/zlib/gzread.c
	third_party/zlib/uncompr.c
)

target_link_libraries(puicbase protobuflite crypto ssl)
