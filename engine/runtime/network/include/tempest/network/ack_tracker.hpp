#ifndef tempest_network_ack_tracker_hpp
#define tempest_network_ack_tracker_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/chrono.hpp>
#include <tempest/int.hpp>
#include <tempest/network/packet.hpp>

namespace tempest::network
{
    /// @brief Evaluates whether sequence number s1 is strictly greater than s2 under 32-bit modular arithmetic.
    /// Handles 2^32 - 1 integer wrapping seamlessly.
    [[nodiscard]] constexpr auto sequence_greater_than(uint32_t first_sequence, uint32_t second_sequence) noexcept
        -> bool
    {
        return ((first_sequence > second_sequence) && (first_sequence - second_sequence < 0x80000000U)) ||
               ((first_sequence < second_sequence) && (second_sequence - first_sequence > 0x80000000U));
    }

    /// @brief Record tracking outgoing packets in flight for roundtrip latency and packet loss estimation.
    struct sent_packet_record
    {
        uint32_t sequence_id = 0;
        chrono::steady_clock::time_point send_time;
        bool acked = false;
        bool valid = false;
    };

    /// @brief Sliding window ACK bitfield and roundtrip time tracker.
    /// Manages outgoing sequence numbers, sliding 32-bit inbound ACK bitfields, and EWMA smoothed RTT calculation.
    class TEMPEST_API ack_tracker
    {
      public:
        ack_tracker() = default;

        /// @brief Registers an outgoing packet with the next sequence number and timestamps it for RTT tracking.
        /// @param now Current timestamp.
        /// @return The assigned sequence ID.
        auto register_outgoing_packet(chrono::steady_clock::time_point now) -> uint32_t;

        /// @brief Populates the ACK sequence ID and 32-bit sliding bitfield into an outgoing packet header.
        /// @param header Packet header to populate.
        auto populate_header_ack_fields(packet_header& header) const noexcept -> void;

        /// @brief Processes an incoming packet header, updating the inbound sliding bitfield and matching ACKs.
        /// @param header Received packet header.
        /// @param now Current timestamp.
        auto process_incoming_header(const packet_header& header, chrono::steady_clock::time_point now) -> void;

        /// @brief Returns the Exponentially Weighted Moving Average (EWMA) smoothed RTT in milliseconds.
        [[nodiscard]] auto smoothed_rtt_ms() const noexcept -> float
        {
            return _smoothed_rtt_ms;
        }

        /// @brief Returns the estimated packet loss rate in range [0.0, 1.0].
        [[nodiscard]] auto packet_loss_rate() const noexcept -> float
        {
            return _packet_loss_rate;
        }

        /// @brief Returns the highest received remote sequence number.
        [[nodiscard]] auto remote_sequence() const noexcept -> uint32_t
        {
            return _remote_sequence;
        }

        /// @brief Returns the 32-bit sliding ACK bitfield representing the 32 packets preceding remote_sequence.
        [[nodiscard]] auto ack_bitfield() const noexcept -> uint32_t
        {
            return _ack_bitfield;
        }

        /// @brief Returns the next sequence number that will be assigned.
        [[nodiscard]] auto next_sequence() const noexcept -> uint32_t
        {
            return _next_sequence;
        }

        /// @brief Configures the next sequence number (useful for testing wrapping boundaries).
        auto set_next_sequence(uint32_t sequence) noexcept -> void
        {
            _next_sequence = sequence;
        }

      private:
        static constexpr size_t history_capacity = 128;
        array<sent_packet_record, history_capacity> _sent_history = {};
        uint32_t _next_sequence = 1;
        uint32_t _remote_sequence = 0;
        uint32_t _ack_bitfield = 0;
        bool _has_remote_sequence = false;

        float _smoothed_rtt_ms = 0.0F;
        float _packet_loss_rate = 0.0F;
        bool _has_loss_measurement = false;
    };
} // namespace tempest::network

#endif // tempest_network_ack_tracker_hpp
