#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/network/bit_stream.hpp>
#include <tempest/network/protocol.hpp>
#include <tempest/span.hpp>

namespace tempest::network::tests
{
    // =========================================================================
    // SECTION: Handshake Connection Request Wire Codec
    // =========================================================================

    /// @brief Verifies that connect_request payloads serialize and deserialize accurately
    /// across wire bitstreams with exact field preservation.
    TEST(protocol_tests, connect_request_round_trip)
    {
        // 1. Setup
        auto writer = bit_writer{};

        const auto original = connect_request_payload{
            .client_version = 1,
            .client_nonce = 0xDEADBEEFCAFE1234ULL,
        };

        // 2. Act
        write_connect_request(writer, original);
        writer.flush();

        auto reader = bit_reader{writer.data()};
        const auto decoded = read_connect_request(reader);

        // 3. Assert
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(decoded->client_version, original.client_version);
        EXPECT_EQ(decoded->client_nonce, original.client_nonce);
        EXPECT_EQ(*decoded, original);
    }

    /// @brief Verifies that truncated buffers fail gracefully when deserializing connect_request.
    TEST(protocol_tests, connect_request_truncated_buffer_fails)
    {
        // 1. Setup
        auto buffer = array<byte, 4>{}; // 4 bytes is shorter than the required 10 bytes (2 + 8)
        auto reader = bit_reader{span<const byte>{buffer.data(), buffer.size()}};

        // 2. Act
        const auto decoded = read_connect_request(reader);

        // 3. Assert
        EXPECT_FALSE(decoded.has_value());
    }

    // =========================================================================
    // SECTION: Handshake Connection Accepted Wire Codec
    // =========================================================================

    /// @brief Verifies that connect_accepted payloads serialize and deserialize accurately
    /// across wire bitstreams with all session and tick parameters intact.
    TEST(protocol_tests, connect_accepted_round_trip)
    {
        // 1. Setup
        auto writer = bit_writer{};

        const auto original = connect_accepted_payload{
            .session_id = 0xFEEDFACE01020304ULL,
            .player_slot = 3,
            .initial_server_tick = 120,
            .tick_rate = 60,
        };

        // 2. Act
        write_connect_accepted(writer, original);
        writer.flush();

        auto reader = bit_reader{writer.data()};
        const auto decoded = read_connect_accepted(reader);

        // 3. Assert
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(decoded->session_id, original.session_id);
        EXPECT_EQ(decoded->player_slot, original.player_slot);
        EXPECT_EQ(decoded->initial_server_tick, original.initial_server_tick);
        EXPECT_EQ(decoded->tick_rate, original.tick_rate);
        EXPECT_EQ(*decoded, original);
    }

    /// @brief Verifies that truncated buffers fail gracefully when deserializing connect_accepted.
    TEST(protocol_tests, connect_accepted_truncated_buffer_fails)
    {
        // 1. Setup
        auto buffer = array<byte, 10>{}; // 10 bytes is shorter than the required 18 bytes (8 + 4 + 4 + 2)
        auto reader = bit_reader{span<const byte>{buffer.data(), buffer.size()}};

        // 2. Act
        const auto decoded = read_connect_accepted(reader);

        // 3. Assert
        EXPECT_FALSE(decoded.has_value());
    }

    // =========================================================================
    // SECTION: Disconnect Wire Codec
    // =========================================================================

    /// @brief Verifies that disconnect payloads round-trip cleanly with diverse termination reason codes.
    TEST(protocol_tests, disconnect_round_trip)
    {
        const auto reasons = array<disconnect_reason, 5>{
            disconnect_reason::user_quit,
            disconnect_reason::timed_out,
            disconnect_reason::server_shutdown,
            disconnect_reason::protocol_mismatch,
            disconnect_reason::kicked,
        };

        for (const auto reason : reasons)
        {
            // 1. Setup
            auto writer = bit_writer{};
            const auto original = disconnect_payload{.reason = reason};

            // 2. Act
            write_disconnect(writer, original);
            writer.flush();

            auto reader = bit_reader{writer.data()};
            const auto decoded = read_disconnect(reader);

            // 3. Assert
            ASSERT_TRUE(decoded.has_value());
            EXPECT_EQ(decoded->reason, original.reason);
            EXPECT_EQ(*decoded, original);
        }
    }
} // namespace tempest::network::tests
