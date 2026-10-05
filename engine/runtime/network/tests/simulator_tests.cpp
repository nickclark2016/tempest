#include <gtest/gtest.h>

#include <tempest/network/simulator.hpp>
#include <tempest/vector.hpp>

namespace tempest::network::tests
{
    // =========================================================================
    // SECTION: Network Simulator Latency and Delay Tests
    // =========================================================================

    /// @brief Verifies that packets delayed with 50ms latency are not dispatched before 50ms and all dispatched after.
    TEST(simulator_tests, simulator_latency_delay)
    {
        // 1. Setup: Configure simulator with 50ms latency and 0 jitter/loss/duplication
        auto config = network_simulator_config{
            .latency_ms = 50.0f,
            .jitter_ms = 0.0f,
            .packet_loss_rate = 0.0f,
            .packet_duplicate_rate = 0.0f,
            .random_seed = 100,
        };
        auto simulator = network_simulator{config};

        const auto start_time = chrono::steady_clock::time_point{};
        const auto sender_endpoint = endpoint(127, 0, 0, 1, 60001);
        const auto destination_endpoint = endpoint(127, 0, 0, 1, 60002);

        // 2. Act: Send 5 packets at t = 0ms
        const auto payload_data = array<byte, 4>{byte{1}, byte{2}, byte{3}, byte{4}};
        for (auto count = 0U; count < 5U; ++count)
        {
            simulator.send_packet(sender_endpoint, destination_endpoint,
                                  span<const byte>(payload_data.data(), payload_data.size()), start_time);
        }

        EXPECT_EQ(simulator.pending_packet_count(), 5U);

        // Advance to t = 49ms (prior to delivery threshold)
        const auto time_49ms = start_time + chrono::milliseconds(49);
        auto dispatched_early = 0U;
        simulator.update(time_49ms, [&](const endpoint&, const endpoint&, span<const byte>) {
            ++dispatched_early;
        });

        // 3. Assert: No packets dispatched before 50ms, all 5 pending
        EXPECT_EQ(dispatched_early, 0U);
        EXPECT_EQ(simulator.pending_packet_count(), 5U);

        // Advance to t = 50ms
        const auto time_50ms = start_time + chrono::milliseconds(50);
        auto dispatched_ontime = 0U;
        simulator.update(time_50ms, [&](const endpoint& source, const endpoint& dest, span<const byte> received_payload) {
            EXPECT_EQ(source, sender_endpoint);
            EXPECT_EQ(dest, destination_endpoint);
            EXPECT_EQ(received_payload.size(), 4U);
            ++dispatched_ontime;
        });

        // All 5 packets dispatched on time and queue is empty
        EXPECT_EQ(dispatched_ontime, 5U);
        EXPECT_EQ(simulator.pending_packet_count(), 0U);
    }

    // =========================================================================
    // SECTION: Jitter Reordering and Priority Queue Tests
    // =========================================================================

    /// @brief Verifies that high random jitter produces intentional packet reordering while preserving timestamp order.
    TEST(simulator_tests, simulator_jitter_reordering)
    {
        // 1. Setup: Configure simulator with 100ms latency and 50ms jitter
        auto config = network_simulator_config{
            .latency_ms = 100.0f,
            .jitter_ms = 50.0f,
            .packet_loss_rate = 0.0f,
            .packet_duplicate_rate = 0.0f,
            .random_seed = 42,
        };
        auto simulator = network_simulator{config};

        const auto start_time = chrono::steady_clock::time_point{};
        const auto sender_endpoint = endpoint(127, 0, 0, 1, 27001);
        const auto destination_endpoint = endpoint(127, 0, 0, 1, 27002);

        // 2. Act: Send 100 sequential packets tagged with packet index
        constexpr auto packet_count = 100U;
        for (auto index = 0U; index < packet_count; ++index)
        {
            const auto send_time = start_time + chrono::milliseconds(index * 2);
            const auto payload = array<byte, sizeof(uint32_t)>{
                static_cast<byte>(index & 0xFFU),
                static_cast<byte>((index >> 8U) & 0xFFU),
                static_cast<byte>((index >> 16U) & 0xFFU),
                static_cast<byte>((index >> 24U) & 0xFFU),
            };
            simulator.send_packet(sender_endpoint, destination_endpoint,
                                  span<const byte>(payload.data(), payload.size()), send_time);
        }

        // Drain packets across 500ms in 1ms increments
        auto received_indices = vector<uint32_t>{};
        for (auto step = 0U; step < 500U; ++step)
        {
            const auto current_time = start_time + chrono::milliseconds(step);
            simulator.update(current_time, [&](const endpoint&, const endpoint&, span<const byte> received_payload) {
                ASSERT_EQ(received_payload.size(), sizeof(uint32_t));
                auto received_index = static_cast<uint32_t>(received_payload[0]) |
                                      (static_cast<uint32_t>(received_payload[1]) << 8U) |
                                      (static_cast<uint32_t>(received_payload[2]) << 16U) |
                                      (static_cast<uint32_t>(received_payload[3]) << 24U);
                received_indices.push_back(received_index);
            });
        }

        // 3. Assert: All 100 packets delivered
        EXPECT_EQ(received_indices.size(), packet_count);

        // Check if at least one packet arrived out of order due to jitter
        auto reordering_detected = false;
        for (auto position = 1U; position < received_indices.size(); ++position)
        {
            if (received_indices[position] < received_indices[position - 1U])
            {
                reordering_detected = true;
                break;
            }
        }
        EXPECT_TRUE(reordering_detected);
    }

    // =========================================================================
    // SECTION: Packet Loss Statistical Tests
    // =========================================================================

    /// @brief Verifies that 1,000 packets with 10% configured packet loss yield actual loss within statistical expectation (10% +/- 2%).
    TEST(simulator_tests, simulator_packet_loss_statistical)
    {
        // 1. Setup: Configure 10% packet loss with deterministic seed
        auto config = network_simulator_config{
            .latency_ms = 10.0f,
            .jitter_ms = 0.0f,
            .packet_loss_rate = 0.10f,
            .packet_duplicate_rate = 0.0f,
            .random_seed = 9999,
        };
        auto simulator = network_simulator{config};

        const auto start_time = chrono::steady_clock::time_point{};
        const auto sender_endpoint = endpoint(127, 0, 0, 1, 28001);
        const auto destination_endpoint = endpoint(127, 0, 0, 1, 28002);
        const auto dummy_payload = array<byte, 1>{byte{42}};

        // 2. Act: Send 1,000 packets
        constexpr auto total_sent = 1000U;
        for (auto count = 0U; count < total_sent; ++count)
        {
            simulator.send_packet(sender_endpoint, destination_endpoint,
                                  span<const byte>(dummy_payload.data(), dummy_payload.size()), start_time);
        }

        // Drain all delivered packets at t = 20ms
        const auto drain_time = start_time + chrono::milliseconds(20);
        auto delivered_count = 0U;
        simulator.update(drain_time, [&](const endpoint&, const endpoint&, span<const byte>) {
            ++delivered_count;
        });

        // 3. Assert: Dropped count is within 10% +/- 2% (between 80 and 120 dropped out of 1000)
        const auto dropped_count = total_sent - delivered_count;
        EXPECT_GE(dropped_count, 80U);
        EXPECT_LE(dropped_count, 120U);
    }

    // =========================================================================
    // SECTION: Packet Duplication Tests
    // =========================================================================

    /// @brief Verifies that configuring packet duplication generates duplicate packets with distinct delivery timestamps.
    TEST(simulator_tests, simulator_packet_duplication)
    {
        // 1. Setup: Configure simulator with 20% duplicate rate
        auto config = network_simulator_config{
            .latency_ms = 10.0f,
            .jitter_ms = 0.0f,
            .packet_loss_rate = 0.0f,
            .packet_duplicate_rate = 0.20f,
            .random_seed = 12345,
        };
        auto simulator = network_simulator{config};

        const auto start_time = chrono::steady_clock::time_point{};
        const auto sender_endpoint = endpoint(127, 0, 0, 1, 29001);
        const auto destination_endpoint = endpoint(127, 0, 0, 1, 29002);

        // 2. Act: Send 100 packets tagged with payload index
        constexpr auto total_sent = 100U;
        for (auto index = 0U; index < total_sent; ++index)
        {
            const auto payload = array<byte, 1>{static_cast<byte>(index)};
            simulator.send_packet(sender_endpoint, destination_endpoint,
                                  span<const byte>(payload.data(), payload.size()), start_time);
        }

        // Drain all packets up to t = 30ms (allowing 10ms latency + 5ms duplicate offset)
        const auto drain_time = start_time + chrono::milliseconds(30);
        auto received_count = 0U;
        auto payload_counts = array<uint32_t, total_sent>{};

        simulator.update(drain_time, [&](const endpoint&, const endpoint&, span<const byte> received_payload) {
            ASSERT_EQ(received_payload.size(), 1U);
            const auto payload_index = static_cast<size_t>(received_payload[0]);
            ASSERT_LT(payload_index, total_sent);
            ++payload_counts[payload_index];
            ++received_count;
        });

        // 3. Assert: More packets received than sent due to duplicates
        EXPECT_GT(received_count, total_sent);

        auto duplicate_detected = false;
        for (auto index = 0U; index < total_sent; ++index)
        {
            if (payload_counts[index] > 1U)
            {
                duplicate_detected = true;
                break;
            }
        }
        EXPECT_TRUE(duplicate_detected);
    }

    // =========================================================================
    // SECTION: Burst Ordering and Delivery Performance Tests
    // =========================================================================

    /// @brief Verifies that enqueuing 200 packets with varied delays in reverse order
    /// maintains exact ascending deliver_time order via binary search insertion.
    TEST(simulator_tests, simulator_burst_ordering_performance)
    {
        // 1. Setup: Configure simulator with 0 jitter, 0 loss, 0 duplication, 0 base latency
        auto config = network_simulator_config{
            .latency_ms = 0.0f,
            .jitter_ms = 0.0f,
            .packet_loss_rate = 0.0f,
            .packet_duplicate_rate = 0.0f,
            .random_seed = 777,
        };
        auto simulator = network_simulator{config};

        const auto start_time = chrono::steady_clock::time_point{};
        const auto sender_endpoint = endpoint(127, 0, 0, 1, 30001);
        const auto destination_endpoint = endpoint(127, 0, 0, 1, 30002);

        // 2. Act: Enqueue 200 packets with varied delays in reverse order (longest delay first)
        constexpr auto packet_count = 200U;
        for (auto index = 0U; index < packet_count; ++index)
        {
            // Delay ranges from (200 * 2 = 400ms) down to 2ms
            const auto delay_ms = static_cast<uint32_t>((packet_count - index) * 2U);
            const auto send_time = start_time + chrono::milliseconds(delay_ms);

            const auto payload = array<byte, sizeof(uint32_t)>{
                static_cast<byte>(delay_ms & 0xFFU),
                static_cast<byte>((delay_ms >> 8U) & 0xFFU),
                static_cast<byte>((delay_ms >> 16U) & 0xFFU),
                static_cast<byte>((delay_ms >> 24U) & 0xFFU),
            };

            simulator.send_packet(sender_endpoint, destination_endpoint,
                                  span<const byte>(payload.data(), payload.size()), send_time);
        }

        EXPECT_EQ(simulator.pending_packet_count(), packet_count);

        // Drain all packets at t = 1000ms
        const auto drain_time = start_time + chrono::milliseconds(1000);
        auto delivered_delays = vector<uint32_t>{};
        delivered_delays.reserve(packet_count);

        const auto dispatched = simulator.update(drain_time, [&](const endpoint& source, const endpoint& dest,
                                                                 span<const byte> received_payload) {
            EXPECT_EQ(source, sender_endpoint);
            EXPECT_EQ(dest, destination_endpoint);
            ASSERT_EQ(received_payload.size(), sizeof(uint32_t));

            const auto delay_val = static_cast<uint32_t>(received_payload[0]) |
                                   (static_cast<uint32_t>(received_payload[1]) << 8U) |
                                   (static_cast<uint32_t>(received_payload[2]) << 16U) |
                                   (static_cast<uint32_t>(received_payload[3]) << 24U);
            delivered_delays.push_back(delay_val);
        });

        // 3. Assert: All 200 packets were dispatched in strictly ascending delay order
        EXPECT_EQ(dispatched, packet_count);
        EXPECT_EQ(delivered_delays.size(), packet_count);
        EXPECT_EQ(simulator.pending_packet_count(), 0U);

        for (auto index = 1U; index < delivered_delays.size(); ++index)
        {
            EXPECT_LE(delivered_delays[index - 1U], delivered_delays[index]);
        }
    }
} // namespace tempest::network::tests
