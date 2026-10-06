#include <gtest/gtest.h>

#include <tempest/network/prediction_buffer.hpp>
#include <tempest/network/user_cmd.hpp>
#include <tempest/network/bit_stream.hpp>
#include <tempest/network/packet.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/span.hpp>
#include <tempest/vec3.hpp>

namespace tempest::network::tests
{
    namespace
    {
        struct mock_character_state
        {
            math::float3 position = {};
            math::float3 velocity = {};
            float orientation_yaw = 0.0F;

            auto operator==(const mock_character_state& other) const noexcept -> bool = default;
        };
    } // namespace

    // =========================================================================
    // SECTION: User Command Quantization & Wire Codec Tests
    // =========================================================================

    /// @brief Verifies quantization precision bounds for movement axes, yaw angle, and button bitmasks.
    TEST(prediction_buffer_tests, user_cmd_quantized_codec_precision)
    {
        // 1. Setup: Define test command with various boundary and mid-range values
        const auto original_cmd = user_cmd{
            .tick = 4294967200U,
            .forward_move = 0.75F,
            .right_move = -0.5F,
            .view_yaw = math::constants::pi<float> * 0.5F,
            .buttons = static_cast<uint16_t>(user_button_jump | user_button_sprint),
        };

        // 2. Act: Serialize to bitstream and deserialize back
        auto writer = bit_writer{};
        write_user_cmd(writer, original_cmd);
        writer.flush();

        auto reader = bit_reader{writer.data()};
        const auto decoded_cmd = read_user_cmd(reader);

        // 3. Assert: Verify decoded values match within quantization tolerance
        ASSERT_TRUE(decoded_cmd.has_value());
        EXPECT_EQ(decoded_cmd->tick, original_cmd.tick);
        EXPECT_NEAR(decoded_cmd->forward_move, original_cmd.forward_move, 0.01F);
        EXPECT_NEAR(decoded_cmd->right_move, original_cmd.right_move, 0.01F);
        EXPECT_NEAR(decoded_cmd->view_yaw, original_cmd.view_yaw, 0.001F);
        EXPECT_EQ(decoded_cmd->buttons, original_cmd.buttons);
        EXPECT_TRUE((decoded_cmd->buttons & user_button_jump) != 0);
        EXPECT_TRUE((decoded_cmd->buttons & user_button_sprint) != 0);
        EXPECT_FALSE((decoded_cmd->buttons & user_button_crouch) != 0);

        // Test boundary values (-1.0F, 1.0F, angle normalization)
        const auto boundary_cmd = user_cmd{
            .tick = 1U,
            .forward_move = -1.0F,
            .right_move = 1.0F,
            .view_yaw = -math::constants::pi<float> * 0.25F, // Negative angle normalizes to ~7*pi/4
            .buttons = user_button_none,
        };

        writer.clear();
        write_user_cmd(writer, boundary_cmd);
        writer.flush();

        auto boundary_reader = bit_reader{writer.data()};
        const auto decoded_boundary = read_user_cmd(boundary_reader);

        ASSERT_TRUE(decoded_boundary.has_value());
        EXPECT_EQ(decoded_boundary->tick, 1U);
        EXPECT_NEAR(decoded_boundary->forward_move, -1.0F, 0.01F);
        EXPECT_NEAR(decoded_boundary->right_move, 1.0F, 0.01F);
        constexpr auto expected_normalized_yaw = (2.0F * math::constants::pi<float>) - (math::constants::pi<float> * 0.25F);
        EXPECT_NEAR(decoded_boundary->view_yaw, expected_normalized_yaw, 0.001F);
        EXPECT_EQ(decoded_boundary->buttons, user_button_none);
    }

    // =========================================================================
    // SECTION: Prediction Buffer Circular Ring & Overwrite Tests
    // =========================================================================

    /// @brief Verifies circular ring buffer indexing, capacity wrapping, and slot overwrite tracking.
    TEST(prediction_buffer_tests, circular_buffer_wrap_past_capacity)
    {
        // 1. Setup: Instantiate buffer with fixed capacity of 128 frames
        constexpr auto buffer_capacity = 128U;
        auto buffer = client_prediction_buffer<mock_character_state, buffer_capacity>{};

        EXPECT_EQ(buffer.capacity(), buffer_capacity);
        EXPECT_FALSE(buffer.latest_tick().has_value());
        EXPECT_FALSE(buffer.last_acked_tick().has_value());

        // 2. Act: Record 256 consecutive ticks (two full passes of the ring buffer)
        constexpr auto total_ticks = 256U;
        for (auto tick = 0U; tick < total_ticks; ++tick)
        {
            const auto cmd = user_cmd{
                .tick = tick,
                .forward_move = 1.0F,
                .right_move = 0.0F,
                .view_yaw = 0.0F,
                .buttons = (tick % 2 == 0) ? static_cast<uint16_t>(user_button_jump) : static_cast<uint16_t>(user_button_none),
            };
            const auto state = mock_character_state{
                .position = math::float3{static_cast<float>(tick), 0.0F, 0.0F},
                .velocity = math::float3{10.0F, 0.0F, 0.0F},
                .orientation_yaw = 0.0F,
            };
            buffer.record_input(cmd, state);
        }

        // 3. Assert: Latest tick is 255
        ASSERT_TRUE(buffer.latest_tick().has_value());
        EXPECT_EQ(*buffer.latest_tick(), 255U);

        // Old frames (0..127) have been overwritten and should return nullptr
        for (auto tick = 0U; tick < 128U; ++tick)
        {
            EXPECT_EQ(buffer.get_frame(tick), nullptr);
            EXPECT_FALSE(buffer.has_frame(tick));
        }

        // Newer frames (128..255) remain valid and accessible with correct stored state
        for (auto tick = 128U; tick < total_ticks; ++tick)
        {
            const auto* frame = buffer.get_frame(tick);
            ASSERT_NE(frame, nullptr);
            EXPECT_TRUE(frame->is_valid);
            EXPECT_EQ(frame->cmd.tick, tick);
            EXPECT_EQ(frame->predicted_state.position.x, static_cast<float>(tick));
            EXPECT_TRUE(buffer.has_frame(tick));
        }
    }

    // =========================================================================
    // SECTION: Sliding Window Acknowledgments & Command Gathering Tests
    // =========================================================================

    /// @brief Verifies unacknowledged command gathering with sliding ACK boundaries and chronological ordering.
    TEST(prediction_buffer_tests, gather_unacked_commands_with_sliding_ack)
    {
        // 1. Setup: Record 10 sequential commands (ticks 1 through 10)
        auto buffer = client_prediction_buffer<mock_character_state, 128>{};
        for (auto tick = 1U; tick <= 10U; ++tick)
        {
            const auto cmd = user_cmd{
                .tick = tick,
                .forward_move = 0.5F,
                .right_move = 0.0F,
                .view_yaw = 0.0F,
                .buttons = user_button_none,
            };
            const auto state = mock_character_state{
                .position = math::float3{0.0F, 0.0F, static_cast<float>(tick)},
            };
            buffer.record_input(cmd, state);
        }

        // 2. Act & Assert: Gather commands before any ACK is received
        // With capacity for 4 commands, it should gather the 4 most recent (ticks 7, 8, 9, 10) in chronological order
        auto gathered_cmds = array<user_cmd, 4>{};
        auto count = buffer.gather_unacked_commands(span<user_cmd>{gathered_cmds.data(), gathered_cmds.size()});
        EXPECT_EQ(count, 4U);
        EXPECT_EQ(gathered_cmds[0].tick, 7U);
        EXPECT_EQ(gathered_cmds[1].tick, 8U);
        EXPECT_EQ(gathered_cmds[2].tick, 9U);
        EXPECT_EQ(gathered_cmds[3].tick, 10U);

        // 3. Act & Assert: Server ACKs tick 6
        buffer.discard_acked_inputs(6U);
        ASSERT_TRUE(buffer.last_acked_tick().has_value());
        EXPECT_EQ(*buffer.last_acked_tick(), 6U);

        // Frames 1..6 are now discarded
        for (auto tick = 1U; tick <= 6U; ++tick)
        {
            EXPECT_EQ(buffer.get_frame(tick), nullptr);
        }

        // Gathering 4 commands still yields ticks 7..10
        count = buffer.gather_unacked_commands(span<user_cmd>{gathered_cmds.data(), gathered_cmds.size()});
        EXPECT_EQ(count, 4U);
        EXPECT_EQ(gathered_cmds[0].tick, 7U);
        EXPECT_EQ(gathered_cmds[1].tick, 8U);
        EXPECT_EQ(gathered_cmds[2].tick, 9U);
        EXPECT_EQ(gathered_cmds[3].tick, 10U);

        // 4. Act & Assert: Server ACKs tick 8
        buffer.discard_acked_inputs(8U);
        EXPECT_EQ(*buffer.last_acked_tick(), 8U);

        // Unacknowledged ticks remaining are 9 and 10
        count = buffer.gather_unacked_commands(span<user_cmd>{gathered_cmds.data(), gathered_cmds.size()});
        EXPECT_EQ(count, 2U);
        EXPECT_EQ(gathered_cmds[0].tick, 9U);
        EXPECT_EQ(gathered_cmds[1].tick, 10U);

        // 5. Act & Assert: Server ACKs tick 10 (all commands acknowledged)
        buffer.discard_acked_inputs(10U);
        EXPECT_EQ(*buffer.last_acked_tick(), 10U);

        count = buffer.gather_unacked_commands(span<user_cmd>{gathered_cmds.data(), gathered_cmds.size()});
        EXPECT_EQ(count, 0U);
    }

    // =========================================================================
    // SECTION: Input Packet Redundancy & Packet Loss Recovery Tests
    // =========================================================================

    /// @brief Simulates client-server input packet streaming, packet drop, and redundant tick recovery.
    TEST(prediction_buffer_tests, input_packet_redundancy_loss_recovery)
    {
        // 1. Setup: Initialize client prediction buffer and simulate client generating ticks 101 to 105
        auto client_buffer = client_prediction_buffer<mock_character_state, 128>{};
        for (auto tick = 101U; tick <= 105U; ++tick)
        {
            const auto cmd = user_cmd{
                .tick = tick,
                .forward_move = 1.0F,
                .right_move = 0.0F,
                .view_yaw = 0.1F * static_cast<float>(tick),
                .buttons = (tick == 104U) ? static_cast<uint16_t>(user_button_jump) : static_cast<uint16_t>(user_button_none),
            };
            client_buffer.record_input(cmd, mock_character_state{});
        }

        constexpr auto session_id = 0xABCD1234EF567890ULL;
        auto server_last_processed_tick = 100U;
        auto server_processed_ticks = array<uint32_t, 16>{};
        auto server_processed_count = static_cast<size_t>(0);

        // 2. Act - Packet 1 (at tick 103): Client sends ticks 101..103
        auto client_cmds = array<user_cmd, max_commands_per_packet>{};
        // Buffer only has up to tick 103 at this moment in simulation
        auto early_buffer = client_prediction_buffer<mock_character_state, 128>{};
        for (auto tick = 101U; tick <= 103U; ++tick)
        {
            const auto* frame = client_buffer.get_frame(tick);
            ASSERT_NE(frame, nullptr);
            early_buffer.record_input(frame->cmd, frame->predicted_state);
        }

        const auto early_count = early_buffer.gather_unacked_commands(span<user_cmd>{client_cmds.data(), client_cmds.size()});
        EXPECT_EQ(early_count, 3U);

        auto writer = bit_writer{};
        write_input_packet(writer, session_id, 1U, 0U, 0U, span<const user_cmd>{client_cmds.data(), early_count});
        writer.flush();

        // Server receives Packet 1
        auto server_reader = bit_reader{writer.data()};
        auto received_header = packet_header{};
        auto received_cmds = array<user_cmd, 8>{};
        const auto read_count_1 = read_input_packet(server_reader, received_header, span<user_cmd>{received_cmds.data(), received_cmds.size()});

        EXPECT_EQ(read_count_1, 3U);
        EXPECT_EQ(received_header.session_id, session_id);
        EXPECT_EQ(received_header.sequence_id, 1U);
        EXPECT_EQ(received_header.type, static_cast<uint8_t>(packet_type::input));

        for (size_t i = 0; i < read_count_1; ++i)
        {
            if (received_cmds[i].tick > server_last_processed_tick)
            {
                server_processed_ticks[server_processed_count++] = received_cmds[i].tick;
                server_last_processed_tick = received_cmds[i].tick;
            }
        }
        EXPECT_EQ(server_last_processed_tick, 103U);
        EXPECT_EQ(server_processed_count, 3U);

        // 3. Act - Packet 2 (at tick 104) is DROPPED over simulated lossy network.
        // Server does NOT receive Packet 2. server_last_processed_tick remains 103.

        // 4. Act - Packet 3 (at tick 105): Client has not yet received ACK for tick 103,
        // so client gathers unacked commands up to max_commands_per_packet (4 commands: 102, 103, 104, 105)
        const auto full_count = client_buffer.gather_unacked_commands(span<user_cmd>{client_cmds.data(), client_cmds.size()});
        EXPECT_EQ(full_count, 4U);
        EXPECT_EQ(client_cmds[0].tick, 102U);
        EXPECT_EQ(client_cmds[1].tick, 103U);
        EXPECT_EQ(client_cmds[2].tick, 104U);
        EXPECT_EQ(client_cmds[3].tick, 105U);

        writer.clear();
        write_input_packet(writer, session_id, 3U, 0U, 0U, span<const user_cmd>{client_cmds.data(), full_count});
        writer.flush();

        // Server receives Packet 3 (Packet 2 was dropped!)
        server_reader = bit_reader{writer.data()};
        const auto read_count_3 = read_input_packet(server_reader, received_header, span<user_cmd>{received_cmds.data(), received_cmds.size()});
        EXPECT_EQ(read_count_3, 4U);

        for (size_t i = 0; i < read_count_3; ++i)
        {
            if (received_cmds[i].tick > server_last_processed_tick)
            {
                server_processed_ticks[server_processed_count++] = received_cmds[i].tick;
                server_last_processed_tick = received_cmds[i].tick;
            }
        }

        // 5. Assert: Server recovered ticks 104 and 105 seamlessly without any gaps!
        EXPECT_EQ(server_last_processed_tick, 105U);
        EXPECT_EQ(server_processed_count, 5U);
        EXPECT_EQ(server_processed_ticks[0], 101U);
        EXPECT_EQ(server_processed_ticks[1], 102U);
        EXPECT_EQ(server_processed_ticks[2], 103U);
        EXPECT_EQ(server_processed_ticks[3], 104U);
        EXPECT_EQ(server_processed_ticks[4], 105U);
    }

    // =========================================================================
    // SECTION: Buffer Reset & Boundary Condition Tests
    // =========================================================================

    /// @brief Verifies buffer reset clears frames and ticks, and handles empty state lookups gracefully.
    TEST(prediction_buffer_tests, buffer_reset_and_boundaries)
    {
        // 1. Setup: Fill buffer with some entries
        auto buffer = client_prediction_buffer<mock_character_state, 16>{};
        for (auto tick = 0U; tick < 8U; ++tick)
        {
            buffer.record_input(user_cmd{.tick = tick}, mock_character_state{});
        }
        EXPECT_EQ(*buffer.latest_tick(), 7U);
        EXPECT_TRUE(buffer.has_frame(0));
        EXPECT_TRUE(buffer.has_frame(7));
        EXPECT_FALSE(buffer.has_frame(8));

        // 2. Act: Reset the buffer
        buffer.reset();

        // 3. Assert: State is completely cleared
        EXPECT_FALSE(buffer.latest_tick().has_value());
        EXPECT_FALSE(buffer.last_acked_tick().has_value());
        for (auto tick = 0U; tick < 16U; ++tick)
        {
            EXPECT_EQ(buffer.get_frame(tick), nullptr);
            EXPECT_FALSE(buffer.has_frame(tick));
        }

        auto cmds = array<user_cmd, 4>{};
        EXPECT_EQ(buffer.gather_unacked_commands(span<user_cmd>{cmds.data(), cmds.size()}), 0U);
    }

    /// @brief Verifies corrupted or truncated input packets do not crash and return zero read count.
    TEST(prediction_buffer_tests, input_packet_truncation_resilience)
    {
        // 1. Setup: Prepare valid packet
        auto writer = bit_writer{};
        const auto cmd = user_cmd{.tick = 1, .forward_move = 1.0F};
        write_input_packet(writer, 12345ULL, 1U, 0U, 0U, span<const user_cmd>{&cmd, 1});
        writer.flush();

        // 2. Act: Read with truncated stream (only header, missing payload bits)
        const auto truncated_data = writer.data().subspan(0, sizeof(packet_header));
        auto reader = bit_reader{truncated_data};
        auto out_cmds = array<user_cmd, 2>{};
        const auto count = read_input_packet(reader, span<user_cmd>{out_cmds.data(), out_cmds.size()});

        // 3. Assert: 0 commands read, no crash
        EXPECT_EQ(count, 0U);
    }
} // namespace tempest::network::tests
