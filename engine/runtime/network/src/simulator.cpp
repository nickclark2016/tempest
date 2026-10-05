#include <tempest/network/simulator.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/utility.hpp>

namespace tempest::network
{
    network_simulator::network_simulator()
        : _config{}, _rng{pcg32::default_seed}
    {
    }

    network_simulator::network_simulator(const network_simulator_config& config)
        : _config{config}, _rng{config.random_seed != 0U ? config.random_seed : pcg32::default_seed}
    {
    }

    auto network_simulator::set_config(const network_simulator_config& config) -> void
    {
        _config = config;
        _rng.seed(_config.random_seed != 0U ? _config.random_seed : pcg32::default_seed);
    }

    auto network_simulator::clear() noexcept -> void
    {
        _packets.clear();
    }

    auto network_simulator::enqueue_packet(simulated_packet packet) -> void
    {
        const auto insert_it = lower_bound(_packets.begin(), _packets.end(), packet,
            [](const simulated_packet& left_packet, const simulated_packet& right_packet) {
                return left_packet.deliver_time < right_packet.deliver_time;
            });
        _packets.insert(insert_it, tempest::move(packet));
    }

    auto network_simulator::send_packet(const endpoint& sender, const endpoint& destination,
                                        span<const byte> payload, chrono::steady_clock::time_point now) -> void
    {
        // 1. Loss check
        if (_config.packet_loss_rate > 0.0F)
        {
            uniform_real_distribution<float> loss_distribution(0.0F, 1.0F);
            if (loss_distribution(_rng) < _config.packet_loss_rate)
            {
                return; // Dropped packet
            }
        }

        // 2. Jitter and latency calculation
        auto simulated_delay_ms = _config.latency_ms;
        if (_config.jitter_ms > 0.0F)
        {
            uniform_real_distribution<float> jitter_distribution(-_config.jitter_ms, _config.jitter_ms);
            simulated_delay_ms += jitter_distribution(_rng);
        }
        
        simulated_delay_ms = max(0.0F, simulated_delay_ms);

        const auto delay_microseconds = static_cast<int64_t>(simulated_delay_ms * 1000.0F);
        const auto deliver_time = now + chrono::microseconds(delay_microseconds);

        // 3. Enqueue original packet
        vector<byte> payload_buffer(payload.size());
        if (!payload.empty())
        {
            tempest::memcpy(payload_buffer.data(), payload.data(), payload.size());
        }

        enqueue_packet(simulated_packet{
            .sender = sender,
            .destination = destination,
            .payload = tempest::move(payload_buffer),
            .deliver_time = deliver_time,
        });

        // 4. Duplicate check
        if (_config.packet_duplicate_rate > 0.0F)
        {
            uniform_real_distribution<float> duplicate_distribution(0.0F, 1.0F);
            if (duplicate_distribution(_rng) < _config.packet_duplicate_rate)
            {
                // Enqueue duplicate with small additional delay (5ms)
                constexpr auto duplicate_offset = chrono::milliseconds(5);
                const auto duplicate_deliver_time = deliver_time + duplicate_offset;

                vector<byte> duplicate_buffer(payload.size());
                if (!payload.empty())
                {
                    tempest::memcpy(duplicate_buffer.data(), payload.data(), payload.size());
                }

                enqueue_packet(simulated_packet{
                    .sender = sender,
                    .destination = destination,
                    .payload = tempest::move(duplicate_buffer),
                    .deliver_time = duplicate_deliver_time,
                });
            }
        }
    }
} // namespace tempest::network
