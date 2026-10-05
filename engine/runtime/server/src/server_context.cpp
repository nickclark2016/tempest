#include <tempest/server/server_context.hpp>

#include <tempest/array.hpp>
#include <tempest/chrono.hpp>
#include <tempest/physics/character_controller_system.hpp>
#include <tempest/span.hpp>
#include <tempest/thread.hpp>

namespace tempest::server
{
    server_context::server_context(server_config config)
        : _config(config), _log_sink(), _logger(_log_sink), _profiler(false), _job_system(nullptr), _registry(_events),
          _accumulator(config.fixed_timestep, config.max_frame_delta), _running(false)
    {
    }

    server_context::~server_context()
    {
        _running.store(false);
        _job_system.reset();
    }

    auto server_context::initialize() -> bool
    {
        _logger.info("Initializing Tempest Headless Server...");

        _job_system = make_unique<job::job_system>(_logger, _profiler,
                                                   job::job_system_config{
                                                       .enable_core_pinning = true,
                                                       .topology = job::discover_cpu_topology(),
                                                   });

        if (!_socket.open())
        {
            _logger.error("Failed to open UDP socket.");
            return false;
        }

        const auto bind_ep = network::endpoint::any(_config.port);
        if (!_socket.bind(bind_ep))
        {
            _logger.error("Failed to bind UDP socket to port.");
            return false;
        }

        _logger.info("UDP socket bound successfully.");
        _running.store(true);
        return true;
    }

    auto server_context::request_close() noexcept -> void
    {
        _running.store(false);
    }

    auto server_context::is_running() const noexcept -> bool
    {
        return _running.load();
    }

    auto server_context::total_ticks() const noexcept -> uint64_t
    {
        return _tick_counter;
    }

    auto server_context::poll_network_packets() -> void
    {
        if (!_socket.is_open())
        {
            return;
        }

        auto buffer = array<byte, 2048>{};
        auto sender = network::endpoint{};

        while (true)
        {
            const auto bytes_read = _socket.receive_from(sender, span<byte>{buffer.data(), buffer.size()});
            if (bytes_read == 0)
            {
                break;
            }
        }
    }

    auto server_context::tick_simulation(float delta_time) -> void
    {
        physics::update_character_controllers(_physics_world, _registry, delta_time,
                                              math::vec3<float>{0.0F, -9.81F, 0.0F});
        _physics_world.step(delta_time);
    }

    auto server_context::run() -> int
    {
        if (!_running.load())
        {
            if (!initialize())
            {
                return 1;
            }
        }

        _logger.info("Server entering main loop (60Hz fixed timestep)...");

        const auto test_start = chrono::steady_clock::now();
        auto previous_time = chrono::steady_clock::now();

        while (_running.load())
        {
            const auto current_time = chrono::steady_clock::now();
            const auto elapsed_frame = chrono::duration<float>(current_time - previous_time).count();
            previous_time = current_time;

            if (_config.test_run)
            {
                const auto total_elapsed = chrono::duration<float>(current_time - test_start).count();
                if (total_elapsed >= _config.test_timeout_seconds)
                {
                    _logger.info("Test run timeout reached; cleanly exiting.");
                    request_close();
                    break;
                }
            }

            _accumulator.accumulate(elapsed_frame);
            while (_accumulator.has_pending_ticks())
            {
                poll_network_packets();
                tick_simulation(_accumulator.fixed_delta());
                _accumulator.consume_tick();
                ++_tick_counter;
            }

            this_thread::sleep_for(chrono::milliseconds(1));
        }

        _logger.info("Server shut down cleanly.");
        return 0;
    }
} // namespace tempest::server
