#include <tempest/server/server_context.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/chrono.hpp>
#include <tempest/physics/character_controller_system.hpp>
#include <tempest/physics/character_movement_intent.hpp>
#include <tempest/physics/character_prediction.hpp>
#include <tempest/physics/character_snapshot.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/span.hpp>
#include <tempest/thread.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/utility.hpp>

namespace tempest::server
{
    server_context::server_context(server_config config)
        : _config(config), _log_sink(), _logger(_log_sink), _profiler(false), _job_system(nullptr),
          _registry(_events, _component_types),
          _accumulator(config.fixed_timestep, config.max_frame_delta), _running(false)
    {
    }

    server_context::~server_context()
    {
        _running.store(false);

        while (!_sessions.empty())
        {
            despawn_session(_sessions.size() - 1);
        }

        auto* const system = _physics_world.physics_system();
        if (system != nullptr)
        {
            for (const auto body : _static_bodies)
            {
                system->remove_body(body);
                system->destroy_body(body);
            }
            _static_bodies.clear();

            for (auto* const shape : _static_shapes)
            {
                if (shape != nullptr)
                {
                    system->destroy_shape(shape);
                }
            }
            _static_shapes.clear();
        }

        _job_system.reset();
    }

    auto server_context::create_static_environment() -> void
    {
        auto* const system = _physics_world.physics_system();
        if (system == nullptr)
        {
            return;
        }

        // 1. Static floor box: 40m x 1m x 40m centered at (0, -0.5, 0) so top surface is at Y = 0.0m
        const auto floor_shape = system->create_box_shape(jolt::shim::vec3{20.0F, 0.5F, 20.0F});
        _static_shapes.push_back(floor_shape);

        const auto floor_desc = jolt::shim::body_desc{
            .shape = floor_shape,
            .position = jolt::shim::vec3{0.0F, -0.5F, 0.0F},
            .rotation = jolt::shim::quat{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto floor_body = system->create_body(floor_desc);
        system->add_body(floor_body, false);
        _static_bodies.push_back(floor_body);

        // 2. Static step box: 2m x 0.2m x 2m step at (0, 0.1, 5.0) for stair-stepping verification
        const auto step_shape = system->create_box_shape(jolt::shim::vec3{1.0F, 0.1F, 1.0F});
        _static_shapes.push_back(step_shape);

        const auto step_desc = jolt::shim::body_desc{
            .shape = step_shape,
            .position = jolt::shim::vec3{0.0F, 0.1F, 5.0F},
            .rotation = jolt::shim::quat{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto step_body = system->create_body(step_desc);
        system->add_body(step_body, false);
        _static_bodies.push_back(step_body);
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

        if (_config.create_ground_plane)
        {
            create_static_environment();
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

    auto server_context::despawn_session(size_t session_index) -> void
    {
        if (session_index >= _sessions.size())
        {
            return;
        }

        auto& s = _sessions[session_index];
        if (s.char_id != physics::invalid_character_id)
        {
            _physics_world.despawn_character(s.char_id);
            s.char_id = physics::invalid_character_id;
        }

        if (s.character_entity != ecs::null)
        {
            _registry.destroy(s.character_entity);
            s.character_entity = ecs::null;
        }

        _sessions.erase(_sessions.begin() + session_index);
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

            if (bytes_read < sizeof(network::packet_header))
            {
                continue;
            }

            auto header = network::packet_header{};
            tempest::memcpy(&header, buffer.data(), sizeof(network::packet_header));

            if (header.protocol_magic != network::packet_header::default_protocol_magic ||
                header.protocol_version != network::packet_header::default_protocol_version)
            {
                continue;
            }

            const auto ptype = static_cast<network::packet_type>(header.type);

            if (ptype == network::packet_type::connect_request)
            {
                // Check if an active session already exists for this sender
                auto found_existing = false;
                for (auto& s : _sessions)
                {
                    if (s.client_endpoint == sender)
                    {
                        // Retransmit connect_accepted
                        auto writer = network::bit_writer{};
                        const auto accepted = network::connect_accepted_payload{
                            .session_id = s.session_id,
                            .player_slot = s.slot,
                            .initial_server_tick = static_cast<uint32_t>(_tick_counter),
                            .tick_rate = 60,
                        };
                        network::write_connect_accepted(writer, accepted);
                        writer.flush();

                        const auto packet_bytes = s.client_connection.build_packet(
                            network::packet_type::connect_accepted, writer.data(), chrono::steady_clock::now());
                        static_cast<void>(_socket.send_to(s.client_endpoint,
                                                          span<const byte>{packet_bytes.data(), packet_bytes.size()}));
                        found_existing = true;
                        break;
                    }
                }

                if (!found_existing && _sessions.size() < _config.max_clients)
                {
                    // Find lowest unused slot
                    auto slot = uint32_t{0};
                    while (true)
                    {
                        auto slot_in_use = false;
                        for (const auto& s : _sessions)
                        {
                            if (s.slot == slot)
                            {
                                slot_in_use = true;
                                break;
                            }
                        }
                        if (!slot_in_use)
                        {
                            break;
                        }
                        ++slot;
                    }

                    const auto session_id = _next_session_id++;
                    const auto spawn_pos = math::vec3<float>{static_cast<float>(slot) * 2.0F, 1.0F, 0.0F};

                    auto controller = physics::character_controller_component{
                        .character_height = 1.8F,
                        .character_radius = 0.4F,
                        .jump_height = 1.2F,
                    };
                    const auto char_id = _physics_world.spawn_character(controller, spawn_pos);
                    controller.id = char_id;

                    const auto char_entity = _registry.create<physics::character_controller_component,
                                                             physics::velocity_component,
                                                             physics::character_movement_intent,
                                                             ecs::transform_component,
                                                             ecs::transform_history_component>();
                    auto char_tx = ecs::transform_component{};
                    char_tx.position(spawn_pos);

                    _registry.replace(char_entity, controller);
                    _registry.replace(char_entity, physics::velocity_component{});
                    _registry.replace(char_entity, physics::character_movement_intent{});
                    _registry.replace(char_entity, char_tx);
                    _registry.replace(char_entity, ecs::transform_history_component::create(spawn_pos));

                    auto conn = network::connection{session_id, sender};
                    conn.set_state(network::connection_state::connected);

                    auto new_session = client_session{
                        .session_id = session_id,
                        .slot = slot,
                        .client_endpoint = sender,
                        .client_connection = tempest::move(conn),
                        .character_entity = char_entity,
                        .char_id = char_id,
                        .latest_client_input_tick = 0,
                        .latest_applied_input_tick = 0,
                        .latest_cmd = {},
                        .last_packet_time = chrono::steady_clock::now(),
                    };

                    _sessions.push_back(tempest::move(new_session));
                    auto& registered_session = _sessions.back();

                    // Send connect_accepted
                    auto writer = network::bit_writer{};
                    const auto accepted = network::connect_accepted_payload{
                        .session_id = session_id,
                        .player_slot = slot,
                        .initial_server_tick = static_cast<uint32_t>(_tick_counter),
                        .tick_rate = 60,
                    };
                    network::write_connect_accepted(writer, accepted);
                    writer.flush();

                    const auto packet_bytes = registered_session.client_connection.build_packet(
                        network::packet_type::connect_accepted, writer.data(), chrono::steady_clock::now());
                    static_cast<void>(_socket.send_to(registered_session.client_endpoint,
                                                      span<const byte>{packet_bytes.data(), packet_bytes.size()}));
                }
            }
            else if (ptype == network::packet_type::input)
            {
                client_session* target_session = nullptr;
                for (auto& s : _sessions)
                {
                    if (s.session_id == header.session_id || s.client_endpoint == sender)
                    {
                        target_session = &s;
                        break;
                    }
                }

                if (target_session != nullptr)
                {
                    target_session->last_packet_time = chrono::steady_clock::now();
                    const auto incoming = target_session->client_connection.process_packet(
                        span<const byte>{buffer.data(), bytes_read}, target_session->last_packet_time);
                    if (incoming.has_value())
                    {
                        auto input_reader = network::bit_reader{incoming->payload};
                        auto commands = array<network::user_cmd, network::max_commands_per_packet>{};
                        const auto cmd_count = network::read_input_packet(
                            input_reader, span<network::user_cmd>{commands.data(), commands.size()});

                        for (size_t c = 0; c < cmd_count; ++c)
                        {
                            const auto& cmd = commands[c];
                            if (cmd.tick > target_session->latest_applied_input_tick)
                            {
                                auto exists = false;
                                for (const auto& existing : target_session->pending_commands)
                                {
                                    if (existing.tick == cmd.tick)
                                    {
                                        exists = true;
                                        break;
                                    }
                                }
                                if (!exists)
                                {
                                    target_session->pending_commands.push_back(cmd);
                                }
                            }
                            if (cmd.tick > target_session->latest_client_input_tick)
                            {
                                target_session->latest_client_input_tick = cmd.tick;
                            }
                        }

                        tempest::sort(target_session->pending_commands.begin(),
                                      target_session->pending_commands.end(),
                                      [](const auto& a, const auto& b) { return a.tick < b.tick; });

                        while (target_session->pending_commands.size() > client_session::max_pending_commands)
                        {
                            target_session->pending_commands.erase(target_session->pending_commands.begin());
                        }
                    }
                }
            }
            else if (ptype == network::packet_type::disconnect)
            {
                for (size_t i = 0; i < _sessions.size(); ++i)
                {
                    if (_sessions[i].session_id == header.session_id || _sessions[i].client_endpoint == sender)
                    {
                        despawn_session(i);
                        break;
                    }
                }
            }
        }
    }

    auto server_context::tick_simulation(chrono::duration<double> delta_time) -> void
    {
        for (auto& s : _sessions)
        {
            if (!s.pending_commands.empty())
            {
                const auto next_cmd = s.pending_commands.front();
                s.pending_commands.erase(s.pending_commands.begin());

                s.latest_applied_input_tick = next_cmd.tick;
                s.latest_cmd = next_cmd;

                const auto intent = physics::apply_user_cmd(s.latest_cmd);
                _registry.replace(s.character_entity, intent);

                // Orient server character to match view yaw
                auto* char_obj = _physics_world.get_character(s.char_id);
                if (char_obj != nullptr)
                {
                    const auto yaw_rot =
                        math::quat<float>(math::vec3<float>{0.0F, s.latest_cmd.view_yaw, 0.0F});
                    char_obj->set_rotation(jolt::shim::quat{yaw_rot.x, yaw_rot.y, yaw_rot.z, yaw_rot.w});
                }
            }
            else
            {
                // Starvation: no pending commands, assign neutral zero intent
                const auto zero_intent = physics::character_movement_intent{};
                _registry.replace(s.character_entity, zero_intent);
            }
        }

        physics::update_character_controllers(_physics_world, _registry, delta_time,
                                              math::vec3<float>{0.0F, -9.81F, 0.0F});
        _physics_world.step(delta_time);
    }

    auto server_context::broadcast_snapshots() -> void
    {
        const auto now = chrono::steady_clock::now();

        // 1. Reap timed-out client sessions
        for (size_t i = _sessions.size(); i > 0; --i)
        {
            const auto idx = i - 1;
            if (now - _sessions[idx].last_packet_time > client_session::default_timeout)
            {
                _logger.warn("Client session timed out; despawning.");
                despawn_session(idx);
            }
        }

        if (_sessions.empty())
        {
            return;
        }

        // 2. Capture snapshots for all active player entities
        auto player_snapshots = vector<physics::player_snapshot_entry>{};
        player_snapshots.reserve(_sessions.size());

        for (const auto& s : _sessions)
        {
            const auto snap = physics::capture_character_snapshot(static_cast<uint32_t>(_tick_counter), _registry,
                                                                 s.character_entity);
            player_snapshots.push_back(physics::player_snapshot_entry{
                .session_id = s.session_id,
                .snapshot = snap,
            });
        }

        // 3. Compose and dispatch personalized world snapshot to each client
        for (auto& s : _sessions)
        {
            const auto packet = physics::world_snapshot_packet{
                .server_tick = static_cast<uint32_t>(_tick_counter),
                .ack_client_tick = s.latest_applied_input_tick,
                .players = player_snapshots,
            };

            auto writer = network::bit_writer{};
            physics::write_world_snapshot(writer, packet);
            writer.flush();

            const auto packet_bytes = s.client_connection.build_packet(network::packet_type::snapshot,
                                                                       writer.data(), now);
            static_cast<void>(_socket.send_to(s.client_endpoint, span<const byte>{packet_bytes.data(), packet_bytes.size()}));
        }
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
            const auto elapsed_frame =
                chrono::duration_cast<chrono::duration<double>>(current_time - previous_time);
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
                broadcast_snapshots();
                _accumulator.consume_tick();
                ++_tick_counter;
            }

            this_thread::sleep_for(chrono::milliseconds(1));
        }

        _logger.info("Server shut down cleanly.");
        return 0;
    }
} // namespace tempest::server
