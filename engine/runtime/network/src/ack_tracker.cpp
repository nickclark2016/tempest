#include <tempest/network/ack_tracker.hpp>
#include <tempest/algorithm.hpp>

namespace tempest::network
{
    auto ack_tracker::register_outgoing_packet(chrono::steady_clock::time_point now) -> uint32_t
    {
        const auto sequence = _next_sequence++;
        const auto history_index = sequence % history_capacity;
        _sent_history[history_index] = sent_packet_record{
            .sequence_id = sequence,
            .send_time = now,
            .acked = false,
            .valid = true,
        };
        return sequence;
    }

    auto ack_tracker::populate_header_ack_fields(packet_header& header) const noexcept -> void
    {
        if (_has_remote_sequence)
        {
            header.ack_sequence_id = _remote_sequence;
            header.ack_bitfield = _ack_bitfield;
        }
        else
        {
            header.ack_sequence_id = 0U;
            header.ack_bitfield = 0U;
        }
    }

    auto ack_tracker::process_incoming_header(const packet_header& header, chrono::steady_clock::time_point now) -> void
    {
        // 1. Process remote sequence and update sliding ACK bitfield
        if (!_has_remote_sequence)
        {
            _remote_sequence = header.sequence_id;
            _ack_bitfield = 0U;
            _has_remote_sequence = true;
        }
        else if (sequence_greater_than(header.sequence_id, _remote_sequence))
        {
            const auto sequence_delta = header.sequence_id - _remote_sequence;
            if (sequence_delta < packet_header::ack_bitfield_width)
            {
                _ack_bitfield = (_ack_bitfield << sequence_delta) | (1U << (sequence_delta - 1U));
            }
            else if (sequence_delta == packet_header::ack_bitfield_width)
            {
                _ack_bitfield = 1U << (packet_header::ack_bitfield_width - 1U);
            }
            else
            {
                _ack_bitfield = 0U;
            }
            _remote_sequence = header.sequence_id;
        }
        else if (sequence_greater_than(_remote_sequence, header.sequence_id))
        {
            const auto sequence_delta = _remote_sequence - header.sequence_id;
            if (sequence_delta >= 1U && sequence_delta <= packet_header::ack_bitfield_width)
            {
                _ack_bitfield |= (1U << (sequence_delta - 1U));
            }
        }

        // 2. Process incoming ACKs for sent packets
        if (header.ack_sequence_id != 0U)
        {
            auto acknowledge_packet = [this, now](uint32_t acked_sequence, bool update_rtt) {
                auto& record = _sent_history[acked_sequence % history_capacity];
                if (record.valid && record.sequence_id == acked_sequence && !record.acked)
                {
                    record.acked = true;
                    if (update_rtt)
                    {
                        const auto elapsed_microseconds =
                            chrono::duration_cast<chrono::microseconds>(now - record.send_time).count();
                        const auto sample_rtt_ms = static_cast<float>(elapsed_microseconds) / 1000.0F;
                        if (_smoothed_rtt_ms <= 0.0F)
                        {
                            _smoothed_rtt_ms = sample_rtt_ms;
                        }
                        else
                        {
                            constexpr auto ewma_alpha = 0.1F;
                            _smoothed_rtt_ms = ((1.0F - ewma_alpha) * _smoothed_rtt_ms) + (ewma_alpha * sample_rtt_ms);
                        }
                    }
                }
            };

            // Acknowledge the primary ack_sequence_id
            acknowledge_packet(header.ack_sequence_id, true);

            // Acknowledge previous 32 packets from sliding bitfield
            for (auto bit_index = 0U; bit_index < packet_header::ack_bitfield_width; ++bit_index)
            {
                if ((header.ack_bitfield & (1U << bit_index)) != 0U)
                {
                    const auto preceding_sequence = header.ack_sequence_id - (bit_index + 1U);
                    acknowledge_packet(preceding_sequence, false);
                }
            }

            // 3. Compute packet loss rate over decided sent history
            auto total_decided_packets = 0U;
            auto total_lost_packets = 0U;

            const auto loss_timeout_us =
                tempest::max(int64_t{100'000}, static_cast<int64_t>(_smoothed_rtt_ms * 2000.0F));

            for (const auto& record : _sent_history)
            {
                if (!record.valid)
                {
                    continue;
                }

                // Packet is eligible if it was sent at or before header.ack_sequence_id,
                // and within the history_capacity window.
                const auto is_at_or_before = !sequence_greater_than(record.sequence_id, header.ack_sequence_id);
                const auto sequence_distance = header.ack_sequence_id - record.sequence_id;
                if (!is_at_or_before || sequence_distance >= history_capacity)
                {
                    continue;
                }

                if (record.acked)
                {
                    ++total_decided_packets;
                }
                else
                {
                    // A packet is decided as lost if it was never acked and:
                    // 1. It has fallen behind the sliding bitfield (cannot be acknowledged in future ACKs), OR
                    // 2. Its elapsed transit time exceeds the RTT-adaptive timeout threshold.
                    // Packets still within the bitfield window and transit timeout are considered in-flight/jittered.
                    const auto elapsed_us =
                        chrono::duration_cast<chrono::microseconds>(now - record.send_time).count();
                    const auto is_lost =
                        (sequence_distance > packet_header::ack_bitfield_width) || (elapsed_us >= loss_timeout_us);
                    if (is_lost)
                    {
                        ++total_decided_packets;
                        ++total_lost_packets;
                    }
                }
            }

            if (total_decided_packets > 0U)
            {
                const auto instantaneous_sample =
                    static_cast<float>(total_lost_packets) / static_cast<float>(total_decided_packets);
                if (!_has_loss_measurement)
                {
                    _packet_loss_rate = instantaneous_sample;
                    _has_loss_measurement = true;
                }
                else
                {
                    constexpr auto loss_alpha = 0.1F;
                    _packet_loss_rate = ((1.0F - loss_alpha) * _packet_loss_rate) + (loss_alpha * instantaneous_sample);
                }
            }
        }
    }
} // namespace tempest::network
