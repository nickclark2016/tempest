#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/network/bit_stream.hpp>
#include <tempest/physics/character_protocol.hpp>
#include <tempest/span.hpp>

namespace tempest::physics::tests
{
    // =========================================================================
    // SECTION: Character Snapshot Wire Codec
    // =========================================================================

    /// @brief Verifies that a character_snapshot serializes and deserializes across a bitstream
    /// with zero floating-point drift and exact ground state preservation.
    TEST(character_protocol_tests, character_snapshot_round_trip)
    {
        // 1. Setup
        auto writer = network::bit_writer{};

        auto original = character_snapshot{};
        original.motion.tick = 42;
        original.motion.position = math::vec3<float>{12.345F, -6.789F, 100.5F};
        original.motion.rotation = math::quat<float>{0.1F, 0.2F, 0.3F, 0.9F};
        original.motion.linear_velocity = math::vec3<float>{3.0F, -9.81F, 4.5F};
        original.motion.angular_velocity = math::vec3<float>{0.01F, 0.5F, -0.2F};
        original.is_grounded = true;
        original.current_ground_state = jolt::shim::ground_state::on_ground;
        original.ground_normal = math::vec3<float>{0.0F, 1.0F, 0.0F};

        // 2. Act
        write_character_snapshot(writer, original);
        writer.flush();

        auto reader = network::bit_reader{writer.data()};
        const auto decoded = read_character_snapshot(reader);

        // 3. Assert
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(decoded->tick(), original.tick());
        EXPECT_FLOAT_EQ(decoded->position().x, original.position().x);
        EXPECT_FLOAT_EQ(decoded->position().y, original.position().y);
        EXPECT_FLOAT_EQ(decoded->position().z, original.position().z);

        EXPECT_FLOAT_EQ(decoded->rotation().x, original.rotation().x);
        EXPECT_FLOAT_EQ(decoded->rotation().y, original.rotation().y);
        EXPECT_FLOAT_EQ(decoded->rotation().z, original.rotation().z);
        EXPECT_FLOAT_EQ(decoded->rotation().w, original.rotation().w);

        EXPECT_FLOAT_EQ(decoded->linear_velocity().x, original.linear_velocity().x);
        EXPECT_FLOAT_EQ(decoded->linear_velocity().y, original.linear_velocity().y);
        EXPECT_FLOAT_EQ(decoded->linear_velocity().z, original.linear_velocity().z);

        EXPECT_FLOAT_EQ(decoded->motion.angular_velocity.x, original.motion.angular_velocity.x);
        EXPECT_FLOAT_EQ(decoded->motion.angular_velocity.y, original.motion.angular_velocity.y);
        EXPECT_FLOAT_EQ(decoded->motion.angular_velocity.z, original.motion.angular_velocity.z);

        EXPECT_EQ(decoded->is_grounded, original.is_grounded);
        EXPECT_EQ(decoded->current_ground_state, original.current_ground_state);

        EXPECT_FLOAT_EQ(decoded->ground_normal.x, original.ground_normal.x);
        EXPECT_FLOAT_EQ(decoded->ground_normal.y, original.ground_normal.y);
        EXPECT_FLOAT_EQ(decoded->ground_normal.z, original.ground_normal.z);

        EXPECT_EQ(*decoded, original);
    }

    /// @brief Verifies that attempting to read a character snapshot from an undersized buffer fails.
    TEST(character_protocol_tests, character_snapshot_truncated_buffer_fails)
    {
        // 1. Setup
        auto buffer = array<byte, 16>{}; // Character snapshot requires > 60 bytes
        auto reader = network::bit_reader{span<const byte>{buffer.data(), buffer.size()}};

        // 2. Act
        const auto decoded = read_character_snapshot(reader);

        // 3. Assert
        EXPECT_FALSE(decoded.has_value());
    }

    // =========================================================================
    // SECTION: World Snapshot Packet Wire Codec
    // =========================================================================

    /// @brief Verifies that an empty world_snapshot_packet encodes and decodes cleanly.
    TEST(character_protocol_tests, world_snapshot_empty_round_trip)
    {
        // 1. Setup
        auto writer = network::bit_writer{};

        const auto original = world_snapshot_packet{
            .server_tick = 100,
            .ack_client_tick = 95,
            .players = {},
        };

        // 2. Act
        write_world_snapshot(writer, original);
        writer.flush();

        auto reader = network::bit_reader{writer.data()};
        auto decoded = world_snapshot_packet{};
        const auto success = read_world_snapshot(reader, decoded);

        // 3. Assert
        ASSERT_TRUE(success);
        EXPECT_EQ(decoded.server_tick, original.server_tick);
        EXPECT_EQ(decoded.ack_client_tick, original.ack_client_tick);
        EXPECT_TRUE(decoded.players.empty());
        EXPECT_EQ(decoded, original);
    }

    /// @brief Verifies that a multi-player world snapshot containing multiple player entities
    /// preserves all sessions and character states with exact fidelity.
    TEST(character_protocol_tests, world_snapshot_multi_player_round_trip)
    {
        // 1. Setup
        auto writer = network::bit_writer{};

        auto player1 = character_snapshot{};
        player1.motion.tick = 200;
        player1.motion.position = math::vec3<float>{0.0F, 1.0F, 0.0F};
        player1.motion.rotation = math::quat<float>{0.0F, 0.0F, 0.0F, 1.0F};
        player1.is_grounded = true;
        player1.current_ground_state = jolt::shim::ground_state::on_ground;

        auto player2 = character_snapshot{};
        player2.motion.tick = 200;
        player2.motion.position = math::vec3<float>{5.0F, 2.5F, -10.0F};
        player2.motion.rotation = math::quat<float>{0.0F, 0.7071F, 0.0F, 0.7071F};
        player2.motion.linear_velocity = math::vec3<float>{0.0F, 5.0F, 0.0F};
        player2.is_grounded = false;
        player2.current_ground_state = jolt::shim::ground_state::in_air;

        auto original = world_snapshot_packet{
            .server_tick = 200,
            .ack_client_tick = 198,
        };
        original.players.push_back(player_snapshot_entry{.session_id = 1001ULL, .snapshot = player1});
        original.players.push_back(player_snapshot_entry{.session_id = 1002ULL, .snapshot = player2});

        // 2. Act
        write_world_snapshot(writer, original);
        writer.flush();

        auto reader = network::bit_reader{writer.data()};
        auto decoded = world_snapshot_packet{};
        const auto success = read_world_snapshot(reader, decoded);

        // 3. Assert
        ASSERT_TRUE(success);
        EXPECT_EQ(decoded.server_tick, original.server_tick);
        EXPECT_EQ(decoded.ack_client_tick, original.ack_client_tick);
        ASSERT_EQ(decoded.players.size(), 2U);

        EXPECT_EQ(decoded.players[0].session_id, 1001ULL);
        EXPECT_EQ(decoded.players[0].snapshot, player1);

        EXPECT_EQ(decoded.players[1].session_id, 1002ULL);
        EXPECT_EQ(decoded.players[1].snapshot, player2);

        EXPECT_EQ(decoded, original);
    }
} // namespace tempest::physics::tests
