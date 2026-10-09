#ifndef TEMPEST_SERVER_SERVER_CONTEXT_HPP
#define TEMPEST_SERVER_SERVER_CONTEXT_HPP

#include <tempest/archetype.hpp>
#include <tempest/atomic.hpp>
#include <tempest/chrono.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/fixed_timestep_accumulator.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/network/connection.hpp>
#include <tempest/network/endpoint.hpp>
#include <tempest/network/protocol.hpp>
#include <tempest/network/socket.hpp>
#include <tempest/network/user_cmd.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/character_protocol.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::server
{
    /// @brief Runtime configuration for headless tempest-server.
    struct server_config
    {
        static constexpr uint16_t default_port = 7777;
        static constexpr float default_fixed_timestep = 1.0F / 60.0F;
        static constexpr float default_max_frame_delta = 0.1F;
        static constexpr float default_test_timeout_seconds = 2.0F;
        static constexpr uint32_t default_max_clients = 8;

        uint16_t port = default_port;
        float fixed_timestep = default_fixed_timestep;
        float max_frame_delta = default_max_frame_delta;
        bool test_run = false;
        float test_timeout_seconds = default_test_timeout_seconds;
        uint32_t max_clients = default_max_clients;
        bool create_ground_plane = true;
    };

    /// @brief Active client connection session maintained by the server.
    struct client_session
    {
        static constexpr auto default_timeout = chrono::seconds(5);
        static constexpr uint32_t default_applied_tick = 0;
        static constexpr size_t max_pending_commands = 32;

        uint64_t session_id = 0;
        uint32_t slot = 0;
        network::endpoint client_endpoint{};
        network::connection client_connection{};
        ecs::entity character_entity{ecs::null};
        physics::character_id char_id{physics::invalid_character_id};
        uint32_t latest_client_input_tick = 0;
        uint32_t latest_applied_input_tick = default_applied_tick;
        network::user_cmd latest_cmd{};
        vector<network::user_cmd> pending_commands{};
        chrono::steady_clock::time_point last_packet_time{};
    };

    /// @brief Encapsulates headless server subsystems (logging, jobs, physics, ECS, networking)
    /// running on a fixed 60Hz deterministic tick clock.
    class server_context
    {
      public:
        explicit server_context(server_config config = {});
        ~server_context();

        server_context(const server_context&) = delete;
        auto operator=(const server_context&) -> server_context& = delete;
        server_context(server_context&&) noexcept = delete;
        auto operator=(server_context&&) noexcept -> server_context& = delete;

        /// @brief Initializes logger, job system, physics world, and UDP listening socket.
        auto initialize() -> bool;

        /// @brief Instantiates static collision floor and obstacle geometry in the physics world.
        auto create_static_environment() -> void;

        /// @brief Runs the fixed 60Hz server tick loop until shutdown requested.
        auto run() -> int;

        /// @brief Requests graceful termination of the main loop.
        auto request_close() noexcept -> void;

        /// @brief Checks if the server loop is actively running.
        [[nodiscard]] auto is_running() const noexcept -> bool;

        /// @brief Polls incoming UDP packets from the listening socket.
        auto poll_network_packets() -> void;

        /// @brief Steps character controllers and Jolt physics simulation forward by delta_time.
        auto tick_simulation(chrono::duration<double> delta_time) -> void;
        auto tick_simulation(float delta_time) -> void
        {
            tick_simulation(chrono::duration<double>{static_cast<double>(delta_time)});
        }

        /// @brief Broadcasts world snapshot packets to all connected clients and reaps timed out sessions.
        auto broadcast_snapshots() -> void;

        /// @brief Despawns a client session, removing its character from physics and ECS.
        auto despawn_session(size_t session_index) -> void;

        [[nodiscard]] auto sessions() const noexcept -> span<const client_session>
        {
            return span<const client_session>{_sessions.data(), _sessions.size()};
        }

        [[nodiscard]] auto get_logger() noexcept -> logger&
        {
            return _logger;
        }

        [[nodiscard]] auto get_events() noexcept -> event::event_registry&
        {
            return _events;
        }

        [[nodiscard]] auto get_component_types() noexcept -> ecs::component_type_registry&
        {
            return _component_types;
        }

        [[nodiscard]] auto get_component_types() const noexcept -> const ecs::component_type_registry&
        {
            return _component_types;
        }

        [[nodiscard]] auto get_registry() noexcept -> ecs::archetype_registry&
        {
            return _registry;
        }

        [[nodiscard]] auto get_physics() noexcept -> physics::physics_world&
        {
            return _physics_world;
        }

        [[nodiscard]] auto get_socket() noexcept -> network::udp_socket&
        {
            return _socket;
        }

        [[nodiscard]] auto get_config() const noexcept -> const server_config&
        {
            return _config;
        }

        [[nodiscard]] auto total_ticks() const noexcept -> uint64_t;

      private:
        server_config _config;
        mt_stdout_log_sink _log_sink;
        logger _logger;
        profiler::profiler_session _profiler;
        unique_ptr<job::job_system> _job_system;
        network::network_context _network_context;
        network::udp_socket _socket;
        physics::physics_world _physics_world;
        event::event_registry _events;
        ecs::component_type_registry _component_types;
        ecs::archetype_registry _registry;
        fixed_timestep_accumulator _accumulator;
        atomic<bool> _running = false;
        uint64_t _tick_counter = 0;
        vector<client_session> _sessions;
        vector<jolt::shim::body_id> _static_bodies;
        vector<jolt::shim::shape_handle> _static_shapes;
        uint64_t _next_session_id = 1;
    };
} // namespace tempest::server

#endif // TEMPEST_SERVER_SERVER_CONTEXT_HPP
