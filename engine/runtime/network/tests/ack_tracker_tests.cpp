#include <gtest/gtest.h>

#include <tempest/network/ack_tracker.hpp>
#include <tempest/network/packet.hpp>

namespace tempest::network::tests
{
    // =========================================================================
    // SECTION: Sequence Comparison and Wrapping Tests
    // =========================================================================

    /// @brief Verifies sequence_greater_than handles normal linear sequences and 32-bit integer wrapping across 0xFFFFFFFF to 0.
    TEST(ack_tracker_tests, ack_tracker_sequence_wrapping)
    {
        // 1. Setup: Define boundary sequence numbers
        constexpr auto sequence_base = 0xFFFF'FFFEU;
        constexpr auto sequence_max = 0xFFFF'FFFFU;
        constexpr auto sequence_wrapped_zero = 0x0000'0000U;
        constexpr auto sequence_wrapped_one = 0x0000'0001U;

        // 2. Act & Assert: Verify modular sequence ordering across boundary
        EXPECT_TRUE(sequence_greater_than(sequence_max, sequence_base));
        EXPECT_TRUE(sequence_greater_than(sequence_wrapped_zero, sequence_max));
        EXPECT_TRUE(sequence_greater_than(sequence_wrapped_one, sequence_wrapped_zero));
        EXPECT_TRUE(sequence_greater_than(sequence_wrapped_one, sequence_max));
        EXPECT_TRUE(sequence_greater_than(sequence_wrapped_one, sequence_base));

        // Inverse ordering must be false
        EXPECT_FALSE(sequence_greater_than(sequence_base, sequence_max));
        EXPECT_FALSE(sequence_greater_than(sequence_max, sequence_wrapped_zero));
        EXPECT_FALSE(sequence_greater_than(sequence_wrapped_zero, sequence_wrapped_one));
        EXPECT_FALSE(sequence_greater_than(sequence_base, sequence_wrapped_one));

        // Self-comparison must be false
        EXPECT_FALSE(sequence_greater_than(sequence_base, sequence_base));
        EXPECT_FALSE(sequence_greater_than(sequence_wrapped_zero, sequence_wrapped_zero));

        // Sequence distance under modular arithmetic
        const auto distance_wrap_one = sequence_wrapped_zero - sequence_max;
        EXPECT_EQ(distance_wrap_one, 1U);

        const auto distance_wrap_three = sequence_wrapped_one - sequence_base;
        EXPECT_EQ(distance_wrap_three, 3U);

        // Tracker state wrapping
        auto tracker = ack_tracker{};
        tracker.set_next_sequence(sequence_base);
        const auto start_time = chrono::steady_clock::time_point{};

        const auto seq0 = tracker.register_outgoing_packet(start_time);
        const auto seq1 = tracker.register_outgoing_packet(start_time);
        const auto seq2 = tracker.register_outgoing_packet(start_time);
        const auto seq3 = tracker.register_outgoing_packet(start_time);

        EXPECT_EQ(seq0, sequence_base);
        EXPECT_EQ(seq1, sequence_max);
        EXPECT_EQ(seq2, sequence_wrapped_zero);
        EXPECT_EQ(seq3, sequence_wrapped_one);
    }

    // =========================================================================
    // SECTION: ACK Bitfield In-Order and Out-of-Order Tracking Tests
    // =========================================================================

    /// @brief Verifies that sending 50 sequential packets in order results in a bitfield of all 1s.
    TEST(ack_tracker_tests, ack_bitfield_tracking_in_order)
    {
        // 1. Setup: Instantiate tracker and simulated start time
        auto receiver_tracker = ack_tracker{};
        auto current_time = chrono::steady_clock::time_point{};

        // 2. Act: Process 50 sequential packets from sequence 1 to 50
        for (auto sequence = 1U; sequence <= 50U; ++sequence)
        {
            auto header = packet_header{};
            header.sequence_id = sequence;
            receiver_tracker.process_incoming_header(header, current_time);
        }

        // 3. Assert: Remote sequence matches 50 and the sliding bitfield has all 32 bits set
        EXPECT_EQ(receiver_tracker.remote_sequence(), 50U);
        EXPECT_EQ(receiver_tracker.ack_bitfield(), 0xFFFF'FFFFU);

        // Verify outgoing packet header population
        auto reply_header = packet_header{};
        receiver_tracker.populate_header_ack_fields(reply_header);
        EXPECT_EQ(reply_header.ack_sequence_id, 50U);
        EXPECT_EQ(reply_header.ack_bitfield, 0xFFFF'FFFFU);
    }

    /// @brief Verifies that deliberate packet drops are correctly marked as 0 in the bitfield and reported accurately.
    TEST(ack_tracker_tests, ack_bitfield_tracking_packet_drops)
    {
        // 1. Setup: Instantiate receiver tracker and drop sequence set
        auto receiver_tracker = ack_tracker{};
        auto sender_tracker = ack_tracker{};
        auto current_time = chrono::steady_clock::time_point{};

        // We will send 100 packets (sequences 1 to 100).
        // Deliberately drop 10 packets: 10, 20, 30, 40, 50, 60, 70, 80, 90, 95.
        auto is_dropped = [](uint32_t seq) {
            return seq == 10U || seq == 20U || seq == 30U || seq == 40U || seq == 50U ||
                   seq == 60U || seq == 70U || seq == 80U || seq == 90U || seq == 95U;
        };

        // 2. Act: Sender registers 100 packets; receiver receives 90 packets
        for (auto count = 1U; count <= 100U; ++count)
        {
            const auto outgoing_sequence = sender_tracker.register_outgoing_packet(current_time);
            if (!is_dropped(outgoing_sequence))
            {
                auto incoming_header = packet_header{};
                incoming_header.sequence_id = outgoing_sequence;
                receiver_tracker.process_incoming_header(incoming_header, current_time);
            }
        }

        // 3. Assert: Receiver remote sequence is 100
        EXPECT_EQ(receiver_tracker.remote_sequence(), 100U);

        // Receiver has sent 0 packets, so it reports 0% unhandled packet loss for its own outgoing stream
        EXPECT_EQ(receiver_tracker.packet_loss_rate(), 0.0f);

        // Check receiver's bitfield for sequence 100:
        // Dropped sequence in the last 32 (sequences 68..99):
        // 70: bit = 100 - 70 - 1 = 29
        // 80: bit = 100 - 80 - 1 = 19
        // 90: bit = 100 - 90 - 1 = 9
        // 95: bit = 100 - 95 - 1 = 4
        const auto bitfield = receiver_tracker.ack_bitfield();
        EXPECT_EQ((bitfield & (1U << 4)), 0U);   // Sequence 95 dropped
        EXPECT_EQ((bitfield & (1U << 9)), 0U);   // Sequence 90 dropped
        EXPECT_EQ((bitfield & (1U << 19)), 0U);  // Sequence 80 dropped
        EXPECT_EQ((bitfield & (1U << 29)), 0U);  // Sequence 70 dropped

        // Other nearby packets must be acknowledged (bit = 1)
        EXPECT_NE((bitfield & (1U << 0)), 0U);   // Sequence 99 received
        EXPECT_NE((bitfield & (1U << 1)), 0U);   // Sequence 98 received
        EXPECT_NE((bitfield & (1U << 2)), 0U);   // Sequence 97 received
        EXPECT_NE((bitfield & (1U << 3)), 0U);   // Sequence 96 received
        EXPECT_NE((bitfield & (1U << 5)), 0U);   // Sequence 94 received

        // Now feed receiver's ACK back to the sender
        auto ack_header = packet_header{};
        receiver_tracker.populate_header_ack_fields(ack_header);
        sender_tracker.process_incoming_header(ack_header, current_time);

        // Sender calculates packet loss over sent history up to 100
        // Sent history holds 100 packets; dropped in history eligible window:
        // Dropped: 70, 80, 90, 95 (4 in last 32). Older dropped (10..60) were not in this single ACK bitfield.
        // The sender packet loss rate must be non-zero and accurately tracked
        EXPECT_GT(sender_tracker.packet_loss_rate(), 0.0f);
    }

    /// @brief Verifies out-of-order packets (1, 3, 2, 6, 4, 5) correctly populate the bitfield without corruption.
    TEST(ack_tracker_tests, ack_bitfield_tracking_out_of_order)
    {
        // 1. Setup: Instantiate receiver tracker
        auto receiver_tracker = ack_tracker{};
        const auto current_time = chrono::steady_clock::time_point{};

        // 2. Act: Deliver packets in out-of-order sequence: 1, 3, 2, 6, 4, 5
        auto header = packet_header{};

        // Receive packet 1
        header.sequence_id = 1U;
        receiver_tracker.process_incoming_header(header, current_time);
        EXPECT_EQ(receiver_tracker.remote_sequence(), 1U);
        EXPECT_EQ(receiver_tracker.ack_bitfield(), 0U);

        // Receive packet 3 (packet 2 skipped)
        header.sequence_id = 3U;
        receiver_tracker.process_incoming_header(header, current_time);
        EXPECT_EQ(receiver_tracker.remote_sequence(), 3U);
        // Bit 0 (3 - 2 - 1 = 0) is packet 2: 0
        // Bit 1 (3 - 1 - 1 = 1) is packet 1: 1
        EXPECT_EQ(receiver_tracker.ack_bitfield(), 0b0010U);

        // Receive packet 2 (late / out-of-order)
        header.sequence_id = 2U;
        receiver_tracker.process_incoming_header(header, current_time);
        EXPECT_EQ(receiver_tracker.remote_sequence(), 3U);
        // Bit 0 is now 1, bit 1 is 1 -> binary 0011 (3)
        EXPECT_EQ(receiver_tracker.ack_bitfield(), 0b0011U);

        // Receive packet 6 (packets 4 and 5 skipped)
        header.sequence_id = 6U;
        receiver_tracker.process_incoming_header(header, current_time);
        EXPECT_EQ(receiver_tracker.remote_sequence(), 6U);
        // For remote sequence 6:
        // Bit 0 (6 - 5 - 1 = 0) is packet 5: 0
        // Bit 1 (6 - 4 - 1 = 1) is packet 4: 0
        // Bit 2 (6 - 3 - 1 = 2) is packet 3: 1
        // Bit 3 (6 - 2 - 1 = 3) is packet 2: 1
        // Bit 4 (6 - 1 - 1 = 4) is packet 1: 1
        EXPECT_EQ(receiver_tracker.ack_bitfield(), 0b0001'1100U);

        // Receive packet 4 (late)
        header.sequence_id = 4U;
        receiver_tracker.process_incoming_header(header, current_time);
        EXPECT_EQ(receiver_tracker.remote_sequence(), 6U);
        // Bit 1 (6 - 4 - 1 = 1) is now set
        EXPECT_EQ(receiver_tracker.ack_bitfield(), 0b0001'1110U);

        // Receive packet 5 (late)
        header.sequence_id = 5U;
        receiver_tracker.process_incoming_header(header, current_time);
        EXPECT_EQ(receiver_tracker.remote_sequence(), 6U);
        // Bit 0 (6 - 5 - 1 = 0) is now set
        // Now all packets 1..5 preceding 6 are acknowledged!
        EXPECT_EQ(receiver_tracker.ack_bitfield(), 0b0001'1111U);

        // 3. Assert: Verify all 5 preceding packets are set to 1 in the bitfield
        EXPECT_EQ(receiver_tracker.ack_bitfield(), 31U);
    }

    // =========================================================================
    // SECTION: Smoothed Roundtrip Time (EWMA) Tests
    // =========================================================================

    /// @brief Verifies that roundtrip ACK samples with fixed 100ms latency converge to ~100ms EWMA smoothed RTT.
    TEST(ack_tracker_tests, ack_tracker_rtt_ewma)
    {
        // 1. Setup: Instantiate tracker and fixed time base
        auto tracker = ack_tracker{};
        const auto base_time = chrono::steady_clock::time_point{};
        constexpr auto fixed_rtt = chrono::milliseconds(100);

        // 2. Act: Send and ACK 20 packets with exactly 100ms roundtrip delay
        for (auto step = 0U; step < 20U; ++step)
        {
            const auto send_time = base_time + chrono::milliseconds(step * 50);
            const auto sequence = tracker.register_outgoing_packet(send_time);

            const auto ack_time = send_time + fixed_rtt;
            auto ack_header = packet_header{};
            ack_header.sequence_id = 1000U + step;
            ack_header.ack_sequence_id = sequence;
            ack_header.ack_bitfield = 0U;

            tracker.process_incoming_header(ack_header, ack_time);
        }

        // 3. Assert: Smoothed RTT must converge to approximately 100ms
        EXPECT_NEAR(tracker.smoothed_rtt_ms(), 100.0f, 0.5f);
    }

    // =========================================================================
    // SECTION: Sequence Delta Boundary Tests
    // =========================================================================

    /// @brief Verifies that incoming sequence delta exactly at boundary 32 does not trigger 32-bit shift UB
    /// and correctly leaves only bit 31 set in the ACK bitfield, while delta > 32 resets the bitfield to 0.
    TEST(ack_tracker_tests, ack_tracker_sequence_delta_boundary_32)
    {
        // 1. Setup: Instantiate tracker and base timestamp
        auto tracker = ack_tracker{};
        const auto timestamp = chrono::steady_clock::time_point{};

        // 2. Act & Assert Step A: Send sequence 1
        auto header_1 = packet_header{};
        header_1.sequence_id = 1U;
        tracker.process_incoming_header(header_1, timestamp);
        EXPECT_EQ(tracker.remote_sequence(), 1U);
        EXPECT_EQ(tracker.ack_bitfield(), 0U);

        // 2. Act & Assert Step B: Send sequence 33 (delta == 32)
        auto header_33 = packet_header{};
        header_33.sequence_id = 33U;
        tracker.process_incoming_header(header_33, timestamp);
        EXPECT_EQ(tracker.remote_sequence(), 33U);
        // Only bit 31 is set (representing sequence 1 received, sequences 2..32 skipped)
        EXPECT_EQ(tracker.ack_bitfield(), 1U << 31);
        EXPECT_EQ(tracker.ack_bitfield(), 0x8000'0000U);

        // 2. Act & Assert Step C: Send sequence 66 (delta == 33 > 32)
        auto header_66 = packet_header{};
        header_66.sequence_id = 66U;
        tracker.process_incoming_header(header_66, timestamp);
        EXPECT_EQ(tracker.remote_sequence(), 66U);
        // Bitfield must be completely reset to 0
        EXPECT_EQ(tracker.ack_bitfield(), 0U);
    }
    // =========================================================================
    // SECTION: Packet Loss and Jitter Resilience Tests
    // =========================================================================

    /// @brief Verifies that temporary in-flight packet jitter within the 32-bit bitfield window
    /// does not produce false 100% or erratic packet loss spikes, and only true drops register as loss.
    TEST(ack_tracker_tests, ack_tracker_loss_rate_resilient_to_jitter)
    {
        // 1. Setup: Instantiate sender and receiver trackers with base timestamp
        auto sender_tracker = ack_tracker{};
        auto receiver_tracker = ack_tracker{};
        const auto base_time = chrono::steady_clock::time_point{};

        // 2. Act: Send packets 1, 2, 3 with small timestamps
        const auto seq1 = sender_tracker.register_outgoing_packet(base_time);
        const auto seq2 = sender_tracker.register_outgoing_packet(base_time + chrono::milliseconds(5));
        const auto seq3 = sender_tracker.register_outgoing_packet(base_time + chrono::milliseconds(10));

        // Receiver receives packet 1 and 3; packet 2 is delayed in flight due to network jitter
        auto header1 = packet_header{};
        header1.sequence_id = seq1;
        receiver_tracker.process_incoming_header(header1, base_time + chrono::milliseconds(20));

        auto header3 = packet_header{};
        header3.sequence_id = seq3;
        receiver_tracker.process_incoming_header(header3, base_time + chrono::milliseconds(25));

        // Receiver responds with ACK acknowledging sequence 3, with packet 1 acked in bitfield and packet 2 unacked
        auto ack_header = packet_header{};
        receiver_tracker.populate_header_ack_fields(ack_header);
        EXPECT_EQ(ack_header.ack_sequence_id, 3U);

        // Sender receives ACK at 30ms (well within the in-flight transit timeout threshold of >=100ms)
        sender_tracker.process_incoming_header(ack_header, base_time + chrono::milliseconds(30));

        // 3. Assert: Packet 2 is within the bitfield window (distance 1 <= 32) and in-flight transit window (<100ms),
        // so it must NOT trigger an erratic packet loss spike
        EXPECT_FLOAT_EQ(sender_tracker.packet_loss_rate(), 0.0F);

        // 4. Act: Jittered packet 2 now arrives at receiver at 35ms
        auto header2 = packet_header{};
        header2.sequence_id = seq2;
        receiver_tracker.process_incoming_header(header2, base_time + chrono::milliseconds(35));

        // Receiver generates updated ACK reflecting packet 2 received
        auto updated_ack_header = packet_header{};
        receiver_tracker.populate_header_ack_fields(updated_ack_header);
        sender_tracker.process_incoming_header(updated_ack_header, base_time + chrono::milliseconds(40));

        // Loss rate remains 0%
        EXPECT_FLOAT_EQ(sender_tracker.packet_loss_rate(), 0.0F);

        // 5. Act & Assert: Simulate genuine packet drop: send packet 4 (which will be dropped),
        // then send packets 5 through 45 (advancing remote sequence so packet 4 falls behind bitfield > 32).
        [[maybe_unused]] const auto seq_dropped =
            sender_tracker.register_outgoing_packet(base_time + chrono::milliseconds(50));
        for (auto count = 5U; count <= 45U; ++count)
        {
            const auto seq = sender_tracker.register_outgoing_packet(base_time + chrono::milliseconds(50 + count));
            auto subsequent_header = packet_header{};
            subsequent_header.sequence_id = seq;
            receiver_tracker.process_incoming_header(subsequent_header, base_time + chrono::milliseconds(60 + count));
        }

        auto final_ack = packet_header{};
        receiver_tracker.populate_header_ack_fields(final_ack);
        sender_tracker.process_incoming_header(final_ack, base_time + chrono::milliseconds(200));

        // Dropped packet 4 fell behind the 32-packet bitfield window and must be marked as lost
        EXPECT_GT(sender_tracker.packet_loss_rate(), 0.0F);
    }
} // namespace tempest::network::tests
