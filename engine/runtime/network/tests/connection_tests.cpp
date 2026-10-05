#include <gtest/gtest.h>

#include <tempest/network/connection.hpp>
#include <tempest/network/simulator.hpp>
#include <tempest/string_view.hpp>

namespace tempest::network::tests
{
    // =========================================================================
    // SECTION: Ping-Pong Roundtrip Time (RTT) Tests
    // =========================================================================

    /// @brief Verifies that bidirectional ping-pong through a simulator with 40ms one-way latency converges to ~80ms smoothed RTT.
    TEST(connection_tests, connection_ping_pong_rtt)
    {
        // 1. Setup: Endpoints, connections, and simulator with 40ms one-way latency
        const auto client_endpoint = endpoint(127, 0, 0, 1, 41001);
        const auto server_endpoint = endpoint(127, 0, 0, 1, 41002);
        constexpr auto session_id = 0xDEAD'BEEF'0123'4567ULL;

        auto client_conn = connection{session_id, server_endpoint};
        auto server_conn = connection{session_id, client_endpoint};
        client_conn.set_state(connection_state::connected);
        server_conn.set_state(connection_state::connected);

        auto sim_config = network_simulator_config{
            .latency_ms = 40.0f, // 40ms each way => 80ms RTT
            .jitter_ms = 0.0f,
            .packet_loss_rate = 0.0f,
            .packet_duplicate_rate = 0.0f,
            .random_seed = 1111,
        };
        auto simulator = network_simulator{sim_config};

        const auto start_time = chrono::steady_clock::time_point{};

        // 2. Act: Run simulation loop for 1,500ms in 10ms steps.
        // Client sends ping every 100ms. Server immediately replies with pong upon delivery.
        for (auto time_offset_ms = 0U; time_offset_ms <= 1500U; time_offset_ms += 10U)
        {
            const auto current_time = start_time + chrono::milliseconds(time_offset_ms);

            // Client sends periodic ping
            if (time_offset_ms % 100U == 0U)
            {
                const auto ping_data = array<byte, 4>{byte{'P'}, byte{'I'}, byte{'N'}, byte{'G'}};
                const auto ping_packet = client_conn.build_packet(packet_type::ping,
                                                                  span<const byte>(ping_data.data(), ping_data.size()),
                                                                  current_time);
                simulator.send_packet(client_endpoint, server_endpoint,
                                      span<const byte>(ping_packet.data(), ping_packet.size()), current_time);
            }

            // Update simulator to deliver in-flight packets
            simulator.update(current_time, [&]([[maybe_unused]] const endpoint& sender, const endpoint& destination,
                                               span<const byte> delivered_data) {
                if (destination == server_endpoint)
                {
                    const auto parsed = server_conn.process_packet(delivered_data, current_time);
                    if (parsed.has_value() && parsed->header.type == static_cast<uint8_t>(packet_type::ping))
                    {
                        // Server sends pong response
                        const auto pong_data = array<byte, 4>{byte{'P'}, byte{'O'}, byte{'N'}, byte{'G'}};
                        const auto pong_packet = server_conn.build_packet(
                            packet_type::pong, span<const byte>(pong_data.data(), pong_data.size()), current_time);
                        simulator.send_packet(server_endpoint, client_endpoint,
                                              span<const byte>(pong_packet.data(), pong_packet.size()), current_time);
                    }
                }
                else if (destination == client_endpoint)
                {
                    [[maybe_unused]] const auto parsed = client_conn.process_packet(delivered_data, current_time);
                }
            });
        }

        // 3. Assert: Client's smoothed RTT must converge to approximately 80ms (40ms outbound + 40ms return)
        EXPECT_NEAR(client_conn.smoothed_rtt_ms(), 80.0f, 1.5f);
        EXPECT_EQ(client_conn.state(), connection_state::connected);
        EXPECT_EQ(server_conn.state(), connection_state::connected);
    }

    // =========================================================================
    // SECTION: Loss Recovery and Connection Stability Tests
    // =========================================================================

    /// @brief Verifies that bidirectional packet exchange under 5% packet loss retains connection stability and tracks loss.
    TEST(connection_tests, connection_loss_recovery)
    {
        // 1. Setup: Endpoints, connections, and simulator configured with 5% packet loss
        const auto client_endpoint = endpoint(127, 0, 0, 1, 42001);
        const auto server_endpoint = endpoint(127, 0, 0, 1, 42002);
        constexpr auto session_id = 0x1122'3344'5566'7788ULL;

        auto client_conn = connection{session_id, server_endpoint};
        auto server_conn = connection{session_id, client_endpoint};
        client_conn.set_state(connection_state::connected);
        server_conn.set_state(connection_state::connected);

        auto sim_config = network_simulator_config{
            .latency_ms = 10.0f,
            .jitter_ms = 0.0f,
            .packet_loss_rate = 0.05f, // 5% packet loss
            .packet_duplicate_rate = 0.0f,
            .random_seed = 54321,
        };
        auto simulator = network_simulator{sim_config};

        const auto start_time = chrono::steady_clock::time_point{};

        // 2. Act: Send 100 packets in each direction
        for (auto time_offset_ms = 0U; time_offset_ms <= 2000U; time_offset_ms += 10U)
        {
            const auto current_time = start_time + chrono::milliseconds(time_offset_ms);

            // Client sends snapshot packet every 20ms
            if (time_offset_ms % 20U == 0U && time_offset_ms <= 1000U)
            {
                const auto payload = array<byte, 2>{byte{1}, byte{2}};
                const auto packet = client_conn.build_packet(
                    packet_type::snapshot, span<const byte>(payload.data(), payload.size()), current_time);
                simulator.send_packet(client_endpoint, server_endpoint,
                                      span<const byte>(packet.data(), packet.size()), current_time);
            }

            // Server sends input packet every 20ms
            if (time_offset_ms % 20U == 10U && time_offset_ms <= 1000U)
            {
                const auto payload = array<byte, 2>{byte{3}, byte{4}};
                const auto packet = server_conn.build_packet(
                    packet_type::input, span<const byte>(payload.data(), payload.size()), current_time);
                simulator.send_packet(server_endpoint, client_endpoint,
                                      span<const byte>(packet.data(), packet.size()), current_time);
            }

            simulator.update(current_time, [&](const endpoint&, const endpoint& destination,
                                               span<const byte> delivered_data) {
                if (destination == server_endpoint)
                {
                    [[maybe_unused]] const auto parsed = server_conn.process_packet(delivered_data, current_time);
                }
                else if (destination == client_endpoint)
                {
                    [[maybe_unused]] const auto parsed = client_conn.process_packet(delivered_data, current_time);
                }
            });
        }

        // 3. Assert: Connection remains connected and loss rate reflects dropped packets (~5%)
        EXPECT_EQ(client_conn.state(), connection_state::connected);
        EXPECT_EQ(server_conn.state(), connection_state::connected);
        EXPECT_GT(client_conn.packet_loss_rate(), 0.0f);
        EXPECT_LT(client_conn.packet_loss_rate(), 0.15f);
    }

    // =========================================================================
    // SECTION: Security Filter Pipeline Tests
    // =========================================================================

    /// @brief Dummy XOR cipher implementing ipacket_security for testing payload encryption hooks.
    class xor_security_filter : public ipacket_security
    {
      public:
        explicit xor_security_filter(uint8_t key) noexcept : _key{key} {}

        auto encrypt_payload([[maybe_unused]] uint64_t session_id, [[maybe_unused]] uint32_t sequence_id,
                             span<const byte> plaintext, vector<byte>& out_ciphertext) -> bool override
        {
            out_ciphertext.resize(plaintext.size());
            for (auto index = 0U; index < plaintext.size(); ++index)
            {
                out_ciphertext[index] = static_cast<byte>(static_cast<uint8_t>(plaintext[index]) ^ _key);
            }
            return true;
        }

        auto decrypt_payload([[maybe_unused]] uint64_t session_id, [[maybe_unused]] uint32_t sequence_id,
                             span<const byte> ciphertext, vector<byte>& out_plaintext) -> bool override
        {
            out_plaintext.resize(ciphertext.size());
            for (auto index = 0U; index < ciphertext.size(); ++index)
            {
                out_plaintext[index] = static_cast<byte>(static_cast<uint8_t>(ciphertext[index]) ^ _key);
            }
            return true;
        }

      private:
        uint8_t _key{0};
    };

    /// @brief Verifies that custom ipacket_security transforms wire payloads and correctly restores plaintext upon reception.
    TEST(connection_tests, connection_security_filter)
    {
        // 1. Setup: Security filter with XOR key 0x5A
        auto filter = xor_security_filter{0x5A};
        const auto remote_ep = endpoint(127, 0, 0, 1, 43001);
        constexpr auto session_id = 0xCAFE'BABE'0000'0001ULL;

        auto sender_conn = connection{session_id, remote_ep, &filter};
        auto receiver_conn = connection{session_id, remote_ep, &filter};

        const auto raw_message = string_view{"Tempest Secure UDP Payload"};
        const auto plaintext_bytes = span<const byte>(reinterpret_cast<const byte*>(raw_message.data()), raw_message.size());
        const auto timestamp = chrono::steady_clock::time_point{};

        // 2. Act: Sender builds an encrypted packet
        const auto packet_bytes = sender_conn.build_packet(packet_type::input, plaintext_bytes, timestamp);

        // Verify wire format: Header (27 bytes) is plaintext, but payload bytes must be encrypted (XORed)
        ASSERT_EQ(packet_bytes.size(), sizeof(packet_header) + raw_message.size());
        const auto wire_payload = span<const byte>(packet_bytes.data() + sizeof(packet_header), raw_message.size());

        // Wire payload must NOT match plaintext
        auto matches_plaintext = true;
        for (auto index = 0U; index < raw_message.size(); ++index)
        {
            if (wire_payload[index] != plaintext_bytes[index])
            {
                matches_plaintext = false;
                break;
            }
        }
        EXPECT_FALSE(matches_plaintext);

        // Receiver processes the encrypted packet
        const auto incoming = receiver_conn.process_packet(
            span<const byte>(packet_bytes.data(), packet_bytes.size()), timestamp);

        // 3. Assert: Packet was decrypted and payload matches original plaintext
        ASSERT_TRUE(incoming.has_value());
        EXPECT_EQ(incoming->header.session_id, session_id);
        EXPECT_EQ(incoming->header.type, static_cast<uint8_t>(packet_type::input));
        EXPECT_EQ(incoming->payload.size(), raw_message.size());

        const auto decrypted_view = string_view{
            reinterpret_cast<const char*>(incoming->payload.data()), incoming->payload.size()};
        EXPECT_EQ(decrypted_view, raw_message);
    }

    /// @brief Rejecting security filter simulating MAC verification or decryption failure.
    class rejecting_security_filter : public ipacket_security
    {
      public:
        auto encrypt_payload([[maybe_unused]] uint64_t session_id, [[maybe_unused]] uint32_t sequence_id,
                             span<const byte> plaintext, vector<byte>& out_ciphertext) -> bool override
        {
            out_ciphertext.assign(plaintext.begin(), plaintext.end());
            return true;
        }

        auto decrypt_payload([[maybe_unused]] uint64_t session_id, [[maybe_unused]] uint32_t sequence_id,
                             [[maybe_unused]] span<const byte> ciphertext,
                             [[maybe_unused]] vector<byte>& out_plaintext) -> bool override
        {
            // Deliberately fail decryption to simulate message tampering or invalid MAC
            return false;
        }
    };

    /// @brief Verifies that an unauthenticated packet (failed decryption/MAC) does NOT advance remote_sequence,
    /// does NOT update ack_bitfield, and does NOT acknowledge any sent packets.
    TEST(connection_tests, security_rejects_unauthenticated_header_mutation)
    {
        // 1. Setup: Connection with rejecting security filter and an in-flight outgoing packet
        auto rejecting_filter = rejecting_security_filter{};
        const auto remote_ep = endpoint(127, 0, 0, 1, 44001);
        constexpr auto session_id = 0xFEED'FACE'CAFE'BEEFULL;

        auto test_conn = connection{session_id, remote_ep, &rejecting_filter};
        test_conn.set_state(connection_state::connected);

        const auto start_time = chrono::steady_clock::time_point{};

        // Send an initial packet to register sequence 1 in the ACK history
        const auto outbound_payload = array<byte, 4>{byte{1}, byte{2}, byte{3}, byte{4}};
        const auto sent_packet = test_conn.build_packet(
            packet_type::input, span<const byte>(outbound_payload.data(), outbound_payload.size()), start_time);
        EXPECT_FALSE(sent_packet.empty());
        EXPECT_EQ(test_conn.tracker().next_sequence(), 2U);
        EXPECT_EQ(test_conn.smoothed_rtt_ms(), 0.0f);

        // Record initial ACK tracker state before hostile packet arrival
        const auto initial_remote_sequence = test_conn.tracker().remote_sequence();
        const auto initial_ack_bitfield = test_conn.tracker().ack_bitfield();

        // Craft a forged packet on the wire with valid header magic and session ID,
        // advancing remote sequence to 10 and claiming to acknowledge sequence 1
        auto forged_header = packet_header{};
        forged_header.protocol_magic = packet_header::default_protocol_magic;
        forged_header.protocol_version = packet_header::default_protocol_version;
        forged_header.session_id = session_id;
        forged_header.sequence_id = 10U;
        forged_header.ack_sequence_id = 1U;
        forged_header.ack_bitfield = 0U;
        forged_header.type = static_cast<uint8_t>(packet_type::snapshot);

        const auto forged_payload = array<byte, 4>{byte{0xDE}, byte{0xAD}, byte{0xBE}, byte{0xEF}};
        auto forged_wire_packet = vector<byte>(sizeof(packet_header) + forged_payload.size());
        tempest::memcpy(forged_wire_packet.data(), &forged_header, sizeof(packet_header));
        tempest::memcpy(forged_wire_packet.data() + sizeof(packet_header), forged_payload.data(), forged_payload.size());

        // 2. Act: Process the unauthenticated packet at start_time + 50ms
        const auto receive_time = start_time + chrono::milliseconds(50);
        const auto result = test_conn.process_packet(
            span<const byte>(forged_wire_packet.data(), forged_wire_packet.size()), receive_time);

        // 3. Assert: Packet is rejected, remote sequence did NOT advance, bitfield untouched, and sent packet NOT acked
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(test_conn.tracker().remote_sequence(), initial_remote_sequence);
        EXPECT_EQ(test_conn.tracker().ack_bitfield(), initial_ack_bitfield);
        EXPECT_EQ(test_conn.smoothed_rtt_ms(), 0.0f);
    }

    // =========================================================================
    // SECTION: Zero-Allocation Transmit Path Tests
    // =========================================================================

    /// @brief Verifies that build_packet into a fixed buffer span writes exact header and payload bytes,
    /// returns the exact packet length, advances sequence tracking, and rejects undersized output buffers
    /// without consuming sequence numbers.
    TEST(connection_tests, connection_build_packet_zero_allocation)
    {
        // 1. Setup: Connection with remote endpoint and session ID
        const auto remote_endpoint = endpoint(127, 0, 0, 1, 45001);
        constexpr auto test_session_id = 0xA1B2'C3D4'E5F6'0718ULL;
        auto test_connection = connection{test_session_id, remote_endpoint};
        test_connection.set_state(connection_state::connected);

        const auto timestamp = chrono::steady_clock::time_point{};
        const auto payload_data = array<byte, 8>{
            byte{0x10}, byte{0x20}, byte{0x30}, byte{0x40},
            byte{0x50}, byte{0x60}, byte{0x70}, byte{0x80},
        };
        const auto payload_span = span<const byte>(payload_data.data(), payload_data.size());

        // 2. Act & Assert: Undersized output buffer must fail and reject write
        auto undersized_buffer = array<byte, sizeof(packet_header) + 4>{};
        const auto undersized_written = test_connection.build_packet(
            packet_type::input, payload_span,
            span<byte>(undersized_buffer.data(), undersized_buffer.size()), timestamp);

        EXPECT_EQ(undersized_written, 0U);
        EXPECT_EQ(test_connection.tracker().next_sequence(), 1U);

        // 3. Act: Zero-allocation framing into fixed stack buffer
        auto packet_buffer = array<byte, 128>{};
        const auto expected_packet_size = sizeof(packet_header) + payload_data.size();
        const auto written_bytes = test_connection.build_packet(
            packet_type::input, payload_span,
            span<byte>(packet_buffer.data(), packet_buffer.size()), timestamp);

        // 4. Assert: Exact byte count returned, sequence allocated, header and payload match
        EXPECT_EQ(written_bytes, expected_packet_size);
        EXPECT_EQ(test_connection.tracker().next_sequence(), 2U);

        auto wire_header = packet_header{};
        tempest::memcpy(&wire_header, packet_buffer.data(), sizeof(packet_header));
        EXPECT_EQ(wire_header.protocol_magic, packet_header::default_protocol_magic);
        EXPECT_EQ(wire_header.protocol_version, packet_header::default_protocol_version);
        EXPECT_EQ(wire_header.session_id, test_session_id);
        EXPECT_EQ(wire_header.sequence_id, 1U);
        EXPECT_EQ(wire_header.type, static_cast<uint8_t>(packet_type::input));

        for (auto byte_index = 0U; byte_index < payload_data.size(); ++byte_index)
        {
            EXPECT_EQ(packet_buffer[sizeof(packet_header) + byte_index], payload_data[byte_index]);
        }

        // Receiving connection successfully parses the generated wire packet
        auto receiver_connection = connection{test_session_id, remote_endpoint};
        const auto parsed_packet = receiver_connection.process_packet(
            span<const byte>(packet_buffer.data(), written_bytes), timestamp);

        ASSERT_TRUE(parsed_packet.has_value());
        EXPECT_EQ(parsed_packet->header.sequence_id, 1U);
        EXPECT_EQ(parsed_packet->header.type, static_cast<uint8_t>(packet_type::input));
        EXPECT_EQ(parsed_packet->payload.size(), payload_data.size());
        for (auto byte_index = 0U; byte_index < payload_data.size(); ++byte_index)
        {
            EXPECT_EQ(parsed_packet->payload[byte_index], payload_data[byte_index]);
        }
    }
} // namespace tempest::network::tests
