#ifndef tempest_network_simulator_hpp
#define tempest_network_simulator_hpp

#include <tempest/api.hpp>
#include <tempest/chrono.hpp>
#include <tempest/int.hpp>
#include <tempest/network/endpoint.hpp>
#include <tempest/random.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::network
{
    /// @brief Configuration settings for simulating adverse network conditions.
    struct network_simulator_config
    {
        static constexpr float default_latency_ms = 100.0F;
        static constexpr float default_jitter_ms = 15.0F;
        static constexpr float default_packet_loss_rate = 0.05F;
        static constexpr float default_packet_duplicate_rate = 0.01F;

        float latency_ms = default_latency_ms; // Base one-way latency in milliseconds
        float jitter_ms = default_jitter_ms;   // Uniform random jitter in milliseconds [-jitter_ms, +jitter_ms]
        float packet_loss_rate = default_packet_loss_rate;           // Packet loss probability [0.0, 1.0]
        float packet_duplicate_rate = default_packet_duplicate_rate; // Packet duplication probability [0.0, 1.0]
        uint64_t random_seed = 0;                                    // Deterministic PRNG seed (0 uses default seed)
    };

    /// @brief In-flight packet delayed in simulator queue.
    struct simulated_packet
    {
        endpoint sender;
        endpoint destination;
        vector<byte> payload;
        chrono::steady_clock::time_point deliver_time;
    };

    /// @brief Network condition simulator that queues packets and delivers them according to simulated
    /// latency, random jitter with realistic packet reordering, packet loss, and duplication.
    class TEMPEST_API network_simulator
    {
      public:
        network_simulator();
        explicit network_simulator(const network_simulator_config& config);

        /// @brief Enqueues a packet with simulated latency, jitter, loss, and duplication.
        /// @param sender Sender endpoint.
        /// @param destination Destination endpoint.
        /// @param payload Packet data.
        /// @param now Current timestamp.
        auto send_packet(const endpoint& sender, const endpoint& destination, span<const byte> payload,
                         chrono::steady_clock::time_point now) -> void;

        /// @brief Dispatches and removes all packets whose deliver_time has passed (<= now).
        /// @tparam PacketHandler Invocable matching void(const endpoint&, const endpoint&, span<const byte>).
        /// @param now Current timestamp.
        /// @param handler Callback invoked for each delivered packet.
        /// @return Number of packets dispatched.
        template <typename PacketHandler>
        auto update(chrono::steady_clock::time_point now, PacketHandler&& handler) -> size_t
        {
            auto ready_count = static_cast<size_t>(0);
            while (ready_count < _packets.size() && _packets[ready_count].deliver_time <= now)
            {
                ++ready_count;
            }

            if (ready_count == 0)
            {
                return 0;
            }

            for (auto index = static_cast<size_t>(0); index < ready_count; ++index)
            {
                handler(_packets[index].sender, _packets[index].destination,
                        span<const byte>(_packets[index].payload.data(), _packets[index].payload.size()));
            }

            _packets.erase(_packets.begin(), _packets.begin() + ready_count);

            return ready_count;
        }

        /// @brief Clears all queued packets in flight.
        auto clear() noexcept -> void;

        /// @brief Returns the number of packets currently queued in flight.
        [[nodiscard]] auto pending_packet_count() const noexcept -> size_t
        {
            return _packets.size();
        }

        /// @brief Returns the active simulator configuration.
        [[nodiscard]] auto config() const noexcept -> const network_simulator_config&
        {
            return _config;
        }

        /// @brief Reconfigures the simulator and re-seeds the random number generator.
        auto set_config(const network_simulator_config& config) -> void;

      private:
        auto enqueue_packet(simulated_packet packet) -> void;

        network_simulator_config _config{};
        pcg32 _rng;
        vector<simulated_packet> _packets;
    };
} // namespace tempest::network

#endif // tempest_network_simulator_hpp
