#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/chrono.hpp>
#include <tempest/guid.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/int.hpp>
#include <tempest/logger.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/network/bit_stream.hpp>
#include <tempest/network/connection.hpp>
#include <tempest/network/endpoint.hpp>
#include <tempest/network/packet.hpp>
#include <tempest/network/protocol.hpp>
#include <tempest/network/socket.hpp>
#include <tempest/network/user_cmd.hpp>
#include <tempest/optional.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/character_prediction.hpp>
#include <tempest/physics/character_protocol.hpp>
#include <tempest/physics/character_reconciliation.hpp>
#include <tempest/physics/character_snapshot.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/quat.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/utility.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vector.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GAME_API __declspec(dllexport)
#elif defined(TEMPEST_PLATFORM_LINUX)
#define GAME_API __attribute__((visibility("default")))
#else
#error "Unsupported platform"
#endif

namespace
{
    auto substr(tempest::string_view sv, size_t offset) noexcept -> tempest::string_view
    {
        if (offset >= sv.size())
        {
            return {};
        }
        return tempest::string_view{sv.data() + offset, sv.size() - offset};
    }

    auto starts_with(tempest::string_view sv, tempest::string_view prefix) noexcept -> bool
    {
        if (sv.size() < prefix.size())
        {
            return false;
        }
        for (size_t i = 0; i < prefix.size(); ++i)
        {
            if (sv[i] != prefix[i])
            {
                return false;
            }
        }
        return true;
    }

    auto create_plane_mesh(float width, float depth, float uv_scale = 1.0F,
                           tempest::math::vec4<float> color = {1.0F, 1.0F, 1.0F, 1.0F}) -> tempest::core::mesh
    {
        auto m = tempest::core::mesh{};
        const auto hw = width * 0.5F;
        const auto hd = depth * 0.5F;

        // Front-facing clockwise winding order in Vulkan screen coords (normal: 0, 1, 0):
        m.vertices.push_back(tempest::core::vertex{.position = {-hw, 0.0F, hd},
                                                   .uv = {0.0F, 0.0F},
                                                   .normal = {0.0F, 1.0F, 0.0F},
                                                   .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                                   .color = color});
        m.vertices.push_back(tempest::core::vertex{.position = {hw, 0.0F, hd},
                                                   .uv = {uv_scale, 0.0F},
                                                   .normal = {0.0F, 1.0F, 0.0F},
                                                   .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                                   .color = color});
        m.vertices.push_back(tempest::core::vertex{.position = {hw, 0.0F, -hd},
                                                   .uv = {uv_scale, uv_scale},
                                                   .normal = {0.0F, 1.0F, 0.0F},
                                                   .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                                   .color = color});
        m.vertices.push_back(tempest::core::vertex{.position = {-hw, 0.0F, -hd},
                                                   .uv = {0.0F, uv_scale},
                                                   .normal = {0.0F, 1.0F, 0.0F},
                                                   .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                                   .color = color});

        m.indices.push_back(0);
        m.indices.push_back(1);
        m.indices.push_back(2);
        m.indices.push_back(2);
        m.indices.push_back(3);
        m.indices.push_back(0);

        return m;
    }

    auto create_box_mesh(tempest::math::vec3<float> half_extents, tempest::math::vec4<float> color,
                         tempest::math::vec3<float> center = {0.0F, 0.0F, 0.0F})
        -> tempest::core::mesh
    {
        auto m = tempest::core::mesh{};

        auto add_face = [&](tempest::math::vec3<float> normal, tempest::math::vec3<float> tangent,
                            tempest::math::vec3<float> v0, tempest::math::vec3<float> v1,
                            tempest::math::vec3<float> v2, tempest::math::vec3<float> v3,
                            tempest::math::vec4<float> c) -> void {
            const auto base_idx = static_cast<tempest::uint32_t>(m.vertices.size());
            m.vertices.push_back(tempest::core::vertex{.position = v0 + center,
                                                       .uv = {0.0F, 0.0F},
                                                       .normal = normal,
                                                       .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                       .color = c});
            m.vertices.push_back(tempest::core::vertex{.position = v1 + center,
                                                       .uv = {1.0F, 0.0F},
                                                       .normal = normal,
                                                       .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                       .color = c});
            m.vertices.push_back(tempest::core::vertex{.position = v2 + center,
                                                       .uv = {1.0F, 1.0F},
                                                       .normal = normal,
                                                       .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                       .color = c});
            m.vertices.push_back(tempest::core::vertex{.position = v3 + center,
                                                       .uv = {0.0F, 1.0F},
                                                       .normal = normal,
                                                       .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                       .color = c});

            m.indices.push_back(base_idx + 0);
            m.indices.push_back(base_idx + 1);
            m.indices.push_back(base_idx + 2);
            m.indices.push_back(base_idx + 2);
            m.indices.push_back(base_idx + 3);
            m.indices.push_back(base_idx + 0);
        };

        const auto x = half_extents.x;
        const auto y = half_extents.y;
        const auto z = half_extents.z;

        // Clockwise winding order matching Vulkan pipeline front_face = clockwise:
        // Front (+Z): normal {0, 0, 1}
        add_face({0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F}, {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}, color);
        // Back (-Z): normal {0, 0, -1}
        add_face({0.0F, 0.0F, -1.0F}, {-1.0F, 0.0F, 0.0F}, {x, -y, -z}, {-x, -y, -z}, {-x, y, -z}, {x, y, -z}, color);
        // Left (-X): normal {-1, 0, 0}
        add_face({-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {-x, -y, -z}, {-x, -y, z}, {-x, y, z}, {-x, y, -z}, color);
        // Right (+X): normal {1, 0, 0}
        add_face({1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, {x, -y, z}, {x, -y, -z}, {x, y, -z}, {x, y, z}, color);
        // Top (+Y): normal {0, 1, 0}
        add_face({0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {-x, y, z}, {x, y, z}, {x, y, -z}, {-x, y, -z}, color);
        // Bottom (-Y): normal {0, -1, 0}
        add_face({0.0F, -1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {-x, -y, -z}, {x, -y, -z}, {x, -y, z}, {-x, -y, z}, color);

        return m;
    }

    auto create_capsule_mesh(float cylinder_half_height, float radius,
                             tempest::math::vec4<float> color = {1.0F, 1.0F, 1.0F, 1.0F},
                             tempest::uint32_t radial_segments = 32,
                             tempest::uint32_t rings_per_cap = 8,
                             tempest::math::vec3<float> center = {0.0F, 0.0F, 0.0F})
        -> tempest::core::mesh
    {
        auto m = tempest::core::mesh{};

        const auto num_rings = (2U * rings_per_cap) + 2U;
        const auto verts_per_ring = radial_segments + 1U;
        const auto pi = tempest::math::constants::pi<float>;
        const auto half_pi = tempest::math::constants::half_pi<float>;

        // 1. Generate vertices ring by ring from south pole (-Y) to north pole (+Y)
        for (auto ring_idx = 0U; ring_idx < num_rings; ++ring_idx)
        {
            auto latitude = 0.0F;
            auto y_offset = 0.0F;

            if (ring_idx <= rings_per_cap)
            {
                // Bottom hemisphere: latitude goes from -pi/2 up to 0.0
                const auto t = static_cast<float>(ring_idx) / static_cast<float>(rings_per_cap);
                latitude = -half_pi + (t * half_pi);
                y_offset = -cylinder_half_height;
            }
            else
            {
                // Top hemisphere: latitude goes from 0.0 up to +pi/2
                const auto t = static_cast<float>(ring_idx - (rings_per_cap + 1U)) / static_cast<float>(rings_per_cap);
                latitude = t * half_pi;
                y_offset = cylinder_half_height;
            }

            const auto cos_lat = tempest::math::cos(latitude);
            const auto sin_lat = tempest::math::sin(latitude);
            const auto ring_radius = radius * cos_lat;
            const auto y_pos = y_offset + (radius * sin_lat);
            const auto normal_y = sin_lat;
            const auto normal_r = cos_lat;
            const auto v_coord = static_cast<float>(ring_idx) / static_cast<float>(num_rings - 1U);

            for (auto seg_idx = 0U; seg_idx <= radial_segments; ++seg_idx)
            {
                const auto u_coord = static_cast<float>(seg_idx) / static_cast<float>(radial_segments);
                const auto theta = u_coord * 2.0F * pi;
                const auto cos_theta = tempest::math::cos(theta);
                const auto sin_theta = tempest::math::sin(theta);

                const auto pos = center + tempest::math::vec3<float>{
                    ring_radius * cos_theta,
                    y_pos,
                    ring_radius * sin_theta,
                };

                const auto norm = tempest::math::vec3<float>{
                    normal_r * cos_theta,
                    normal_y,
                    normal_r * sin_theta,
                };

                const auto tangent = tempest::math::vec4<float>{
                    -sin_theta,
                    0.0F,
                    cos_theta,
                    1.0F,
                };

                m.vertices.push_back(tempest::core::vertex{
                    .position = pos,
                    .uv = {u_coord, v_coord},
                    .normal = norm,
                    .tangent = tangent,
                    .color = color,
                });
            }
        }

        // 2. Generate clockwise indices for triangles connecting consecutive rings
        for (auto ring_idx = 0U; ring_idx < num_rings - 1U; ++ring_idx)
        {
            const auto ring_start = ring_idx * verts_per_ring;
            const auto next_ring_start = (ring_idx + 1U) * verts_per_ring;

            for (auto seg_idx = 0U; seg_idx < radial_segments; ++seg_idx)
            {
                const auto v00 = ring_start + seg_idx;
                const auto v01 = ring_start + (seg_idx + 1U);
                const auto v10 = next_ring_start + seg_idx;
                const auto v11 = next_ring_start + (seg_idx + 1U);

                if (ring_idx == 0U)
                {
                    // Bottom pole: emit single non-degenerate triangle (v00, v10, v11)
                    m.indices.push_back(v00);
                    m.indices.push_back(v10);
                    m.indices.push_back(v11);
                }
                else if (ring_idx == num_rings - 2U)
                {
                    // Top pole: emit single non-degenerate triangle (v00, v11, v01)
                    m.indices.push_back(v00);
                    m.indices.push_back(v11);
                    m.indices.push_back(v01);
                }
                else
                {
                    // Quad connecting rings
                    m.indices.push_back(v00);
                    m.indices.push_back(v10);
                    m.indices.push_back(v11);

                    m.indices.push_back(v00);
                    m.indices.push_back(v11);
                    m.indices.push_back(v01);
                }
            }
        }

        return m;
    }

    struct remote_player_entry
    {
        static constexpr tempest::uint64_t default_session_id = 0;
        static constexpr float default_interpolation_speed = 30.0F;

        tempest::uint64_t session_id = default_session_id;
        tempest::ecs::entity entity = tempest::ecs::null;
        tempest::math::vec3<float> current_pos{0.0F, 0.0F, 0.0F};
        tempest::math::vec3<float> target_pos{0.0F, 0.0F, 0.0F};
        tempest::math::quat<float> current_rot{0.0F, 0.0F, 0.0F, 1.0F};
        tempest::math::quat<float> target_rot{0.0F, 0.0F, 0.0F, 1.0F};
        float interp_factor = 1.0F;
    };

    struct multiplayer_client
    {
        static constexpr float default_mouse_sensitivity = 0.0025F;
        static constexpr float min_camera_pitch = -1.45F;
        static constexpr float max_camera_pitch = 1.45F;
        static constexpr float default_camera_yaw = 0.0F;
        static constexpr float default_camera_pitch = -0.35F;
        static constexpr float default_mouse_coord = 0.0F;
        static constexpr tempest::uint64_t default_session_id = 0;
        static constexpr tempest::uint32_t default_player_slot = 0;
        static constexpr tempest::uint32_t default_client_tick = 0;
        static constexpr bool default_is_focused = true;
        static constexpr size_t max_command_history = 16;
        static constexpr size_t max_packets_per_tick = 64;
        static constexpr tempest::chrono::milliseconds connect_retransmit_interval{500};

        // Mode
        bool is_offline = false;
        bool is_focused = default_is_focused;

        // Network connection state
        tempest::network::network_context net_ctx = {};
        tempest::network::endpoint server_endpoint = {};
        tempest::network::udp_socket socket = {};
        tempest::network::connection connection = {};
        tempest::uint64_t session_id = default_session_id;
        tempest::uint32_t player_slot = default_player_slot;
        bool connected = false;
        bool first_snapshot_received = false;
        tempest::chrono::steady_clock::time_point last_connect_attempt = {};

        // Authoritative simulation & client prediction
        tempest::physics::physics_world client_physics_world = {};
        tempest::vector<jolt::shim::body_id> static_bodies = {};
        tempest::vector<jolt::shim::shape_handle> static_shapes = {};
        tempest::ecs::entity local_player_entity = tempest::ecs::null;
        tempest::ecs::entity camera_entity = tempest::ecs::null;
        tempest::physics::character_prediction_buffer prediction_buffer = {};
        tempest::uint32_t current_client_tick = default_client_tick;

        // Redundant command history for packet loss mitigation
        tempest::vector<tempest::network::user_cmd> command_history = {};

        // Remote player visual proxies
        tempest::vector<remote_player_entry> remote_players = {};

        // First-person camera view
        float camera_yaw = default_camera_yaw;
        float camera_pitch = default_camera_pitch;
        float last_mouse_x = default_mouse_coord;
        float last_mouse_y = default_mouse_coord;
        bool first_mouse = true;
        tempest::chrono::steady_clock::time_point last_render_time = {};

        // Registered assets
        tempest::guid local_player_mesh_id = {};
        tempest::guid remote_player_mesh_id = {};
        tempest::guid local_player_mat_id = {};
        tempest::guid remote_player_mat_id = {};
        tempest::guid floor_mesh_id = {};
        tempest::guid floor_mat_id = {};
        tempest::guid step_mesh_id = {};
        tempest::guid step_mat_id = {};
    };

    auto setup_client_static_environment(tempest::physics::physics_world& world,
                                         tempest::vector<jolt::shim::body_id>& bodies,
                                         tempest::vector<jolt::shim::shape_handle>& shapes) -> void
    {
        auto* const system = world.physics_system();
        if (system == nullptr)
        {
            return;
        }

        // 1. Static floor box: 40m x 1m x 40m centered at (0, -0.5, 0) so top surface is at Y = 0.0m
        const auto floor_shape = system->create_box_shape(jolt::shim::vec3{20.0F, 0.5F, 20.0F});
        shapes.push_back(floor_shape);

        const auto floor_desc = jolt::shim::body_desc{
            .shape = floor_shape,
            .position = jolt::shim::vec3{0.0F, -0.5F, 0.0F},
            .rotation = jolt::shim::quat{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto floor_body = system->create_body(floor_desc);
        system->add_body(floor_body, false);
        bodies.push_back(floor_body);

        // 2. Static step box: 2m x 0.2m x 2m step at (0, 0.1, 5.0) for stair-stepping verification
        const auto step_shape = system->create_box_shape(jolt::shim::vec3{1.0F, 0.1F, 1.0F});
        shapes.push_back(step_shape);

        const auto step_desc = jolt::shim::body_desc{
            .shape = step_shape,
            .position = jolt::shim::vec3{0.0F, 0.1F, 5.0F},
            .rotation = jolt::shim::quat{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 1.0F},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto step_body = system->create_body(step_desc);
        system->add_body(step_body, false);
        bodies.push_back(step_body);
    }

    auto cleanup_client_static_environment(tempest::physics::physics_world& world,
                                           tempest::vector<jolt::shim::body_id>& bodies,
                                           tempest::vector<jolt::shim::shape_handle>& shapes) -> void
    {
        auto* const system = world.physics_system();
        if (system != nullptr)
        {
            for (const auto body : bodies)
            {
                system->remove_body(body);
                system->destroy_body(body);
            }
            bodies.clear();

            for (auto* const shape : shapes)
            {
                if (shape != nullptr)
                {
                    system->destroy_shape(shape);
                }
            }
            shapes.clear();
        }
    }

    auto spawn_local_player(tempest::client_context* ctx, multiplayer_client* client) -> void
    {
        auto& registry = ctx->get_entities();
        const auto spawn_pos = client->is_offline
                                   ? tempest::math::vec3<float>{0.0F, 1.0F, 0.0F}
                                   : tempest::math::vec3<float>{static_cast<float>(client->player_slot) * 2.0F, 1.0F, 0.0F};

        auto controller = tempest::physics::character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
            .jump_height = 1.2F,
        };
        const auto char_id = client->client_physics_world.spawn_character(controller, spawn_pos);
        controller.id = char_id;

        auto char_tx = tempest::ecs::transform_component{};
        char_tx.position(spawn_pos);

        client->local_player_entity = registry.create();
        registry.assign(client->local_player_entity, controller);
        registry.assign(client->local_player_entity, tempest::physics::velocity_component{});
        registry.assign(client->local_player_entity, tempest::physics::character_movement_intent{});
        registry.assign(client->local_player_entity, char_tx);
        registry.assign(client->local_player_entity, tempest::ecs::transform_history_component::create(spawn_pos));
        registry.assign(client->local_player_entity, tempest::physics::reconciliation_smoothing_component{});
        registry.assign(client->local_player_entity,
                        tempest::core::mesh_component{.mesh_id = client->local_player_mesh_id});
        registry.assign(client->local_player_entity,
                        tempest::core::material_component{.material_id = client->local_player_mat_id});
        registry.name(client->local_player_entity, "LocalPlayer");

        client->prediction_buffer.reset();

        ctx->get_logger().info("Spawned local player character.");
    }

    auto process_world_snapshot(tempest::client_context* ctx, multiplayer_client* client,
                                const tempest::physics::world_snapshot_packet& world_snap) -> void
    {
        auto& registry = ctx->get_entities();

        for (const auto& player_snap : world_snap.players)
        {
            if (player_snap.session_id == client->session_id)
            {
                // Local player reconciliation against authoritative server state
                if (client->local_player_entity != tempest::ecs::null)
                {
                    auto local_snap = player_snap.snapshot;
                    local_snap.motion.tick = world_snap.ack_client_tick;

                    tempest::physics::reconcile_client_character(
                        client->client_physics_world, registry, client->local_player_entity,
                        client->prediction_buffer, local_snap);
                }
            }
            else
            {
                // Remote player visual proxy
                auto found_proxy = false;
                for (auto& remote : client->remote_players)
                {
                    if (remote.session_id == player_snap.session_id)
                    {
                        found_proxy = true;
                        const auto* const current_tx =
                            registry.try_get<tempest::ecs::transform_component>(remote.entity);
                        remote.current_pos = (current_tx != nullptr) ? current_tx->position() : remote.target_pos;
                        remote.current_rot =
                            (current_tx != nullptr) ? current_tx->rotation_quat() : remote.target_rot;
                        remote.target_pos = player_snap.snapshot.position();
                        remote.target_rot = player_snap.snapshot.rotation();
                        remote.interp_factor = 0.0F;

                        if (const auto* hist_ptr =
                                registry.try_get<tempest::ecs::transform_history_component>(remote.entity))
                        {
                            auto hist = *hist_ptr;
                            hist.current_position = player_snap.snapshot.position();
                            hist.current_rotation = player_snap.snapshot.rotation();
                            registry.replace(remote.entity, hist);
                        }
                        break;
                    }
                }

                if (!found_proxy)
                {
                    const auto proxy_entity = registry.create();
                    auto proxy_tx = tempest::ecs::transform_component{};
                    proxy_tx.position(player_snap.snapshot.position());
                    proxy_tx.rotation(player_snap.snapshot.rotation());

                    registry.assign(proxy_entity, proxy_tx);
                    registry.assign(proxy_entity, tempest::ecs::transform_history_component::create(
                                                      player_snap.snapshot.position(), player_snap.snapshot.rotation()));
                    registry.assign(proxy_entity,
                                    tempest::core::mesh_component{.mesh_id = client->remote_player_mesh_id});
                    registry.assign(proxy_entity,
                                    tempest::core::material_component{.material_id = client->remote_player_mat_id});
                    registry.name(proxy_entity, "RemotePlayer");

                    client->remote_players.push_back(remote_player_entry{
                        .session_id = player_snap.session_id,
                        .entity = proxy_entity,
                        .current_pos = player_snap.snapshot.position(),
                        .target_pos = player_snap.snapshot.position(),
                        .current_rot = player_snap.snapshot.rotation(),
                        .target_rot = player_snap.snapshot.rotation(),
                        .interp_factor = 1.0F,
                    });

                    ctx->get_logger().info("Spawned remote player proxy.");
                }
            }
        }

        // Despawn visual proxies for sessions that disconnected
        for (auto* it = client->remote_players.begin(); it != client->remote_players.end();)
        {
            auto still_active = false;
            for (const auto& player_snap : world_snap.players)
            {
                if (player_snap.session_id == it->session_id)
                {
                    still_active = true;
                    break;
                }
            }

            if (!still_active)
            {
                registry.destroy(it->entity);
                it = client->remote_players.erase(it);
                ctx->get_logger().info("Despawned disconnected remote player proxy.");
            }
            else
            {
                ++it;
            }
        }
    }

    auto setup_multiplayer_client(tempest::client_context* ctx, const tempest::network::endpoint& server_ep,
                                  bool is_offline) -> void
    {
        auto& logger = ctx->get_logger();
        if (is_offline)
        {
            logger.info("Initializing Game Scene in standalone offline mode...");
        }
        else
        {
            logger.info("Initializing Game Scene in networked multiplayer mode...");
        }

        auto client_owner = tempest::make_unique<multiplayer_client>();
        auto* const client = client_owner.get();
        client->is_offline = is_offline;
        client->server_endpoint = server_ep;

        if (!is_offline)
        {
            if (!client->socket.open())
            {
                logger.error("Failed to open UDP socket on client.");
            }
            else
            {
                if (client->socket.bind(tempest::network::endpoint::any(0)))
                {
                    logger.info("Client UDP socket opened and bound successfully.");
                }
                else
                {
                    logger.error("Failed to bind UDP socket on client.");
                }
            }

            client->connection.set_remote_endpoint(server_ep);
        }

        // Setup static physics environment in client physics world for collision
        setup_client_static_environment(client->client_physics_world, client->static_bodies, client->static_shapes);

        // Register procedural sandbox meshes and materials
        // 1. Floor: 40m x 40m plane mesh with clockwise winding
        client->floor_mesh_id =
            ctx->get_meshes().register_mesh(create_plane_mesh(40.0F, 40.0F, 10.0F, {1.0F, 1.0F, 1.0F, 1.0F}));
        auto floor_mat = tempest::core::material{};
        floor_mat.set_vec4(tempest::core::material::base_color_factor_name, {0.65F, 0.68F, 0.72F, 1.0F});
        floor_mat.set_vec3(tempest::core::material::emissive_factor_name, {0.0F, 0.0F, 0.0F});
        floor_mat.set_scalar(tempest::core::material::metallic_factor_name, 0.02F);
        floor_mat.set_scalar(tempest::core::material::roughness_factor_name, 0.65F);
        client->floor_mat_id = ctx->get_materials().register_material(tempest::move(floor_mat));

        // 2. Step obstacle: 2m x 0.2m x 2m (half-extents 1.0, 0.1, 1.0)
        client->step_mesh_id =
            ctx->get_meshes().register_mesh(create_box_mesh({1.0F, 0.1F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}));
        auto step_mat = tempest::core::material{};
        step_mat.set_vec4(tempest::core::material::base_color_factor_name, {0.75F, 0.78F, 0.82F, 1.0F});
        step_mat.set_vec3(tempest::core::material::emissive_factor_name, {0.0F, 0.0F, 0.0F});
        step_mat.set_scalar(tempest::core::material::metallic_factor_name, 0.05F);
        step_mat.set_scalar(tempest::core::material::roughness_factor_name, 0.6F);
        client->step_mat_id = ctx->get_materials().register_material(tempest::move(step_mat));

        // 3. Local player mesh (feet indicator):
        // Small indicator cube placed at the feet (half-extents 0.18, 0.08, 0.22) offset forward (+0.15m)
        // so the player can verify position/orientation when looking down, avoiding phantom torso shadows.
        client->local_player_mesh_id = ctx->get_meshes().register_mesh(
            create_box_mesh({0.18F, 0.08F, 0.22F}, {1.0F, 1.0F, 1.0F, 1.0F}, {0.0F, -0.82F, 0.15F}));

        // 4. Remote player proxy mesh: capsule matching physics character controller (half-height 0.5m, radius 0.4m, total height 1.8m)
        client->remote_player_mesh_id =
            ctx->get_meshes().register_mesh(create_capsule_mesh(0.5F, 0.4F, {1.0F, 1.0F, 1.0F, 1.0F}));

        // 5. Local player material: Warm Amber / Orange
        auto local_mat = tempest::core::material{};
        local_mat.set_vec4(tempest::core::material::base_color_factor_name, {0.95F, 0.55F, 0.15F, 1.0F});
        local_mat.set_vec3(tempest::core::material::emissive_factor_name, {0.0F, 0.0F, 0.0F});
        local_mat.set_scalar(tempest::core::material::metallic_factor_name, 0.1F);
        local_mat.set_scalar(tempest::core::material::roughness_factor_name, 0.5F);
        client->local_player_mat_id = ctx->get_materials().register_material(tempest::move(local_mat));

        // 6. Remote player material: Vibrant Cyan
        auto remote_mat = tempest::core::material{};
        remote_mat.set_vec4(tempest::core::material::base_color_factor_name, {0.15F, 0.75F, 0.95F, 1.0F});
        remote_mat.set_vec3(tempest::core::material::emissive_factor_name, {0.0F, 0.0F, 0.0F});
        remote_mat.set_scalar(tempest::core::material::metallic_factor_name, 0.1F);
        remote_mat.set_scalar(tempest::core::material::roughness_factor_name, 0.5F);
        client->remote_player_mat_id = ctx->get_materials().register_material(tempest::move(remote_mat));

        // Register engine callbacks
        ctx->register_on_initialize_callback([client, ctx](tempest::engine_context& engine_ctx) -> void {
            auto& registry = engine_ctx.get_entities();

            // First-person camera: Eye position at character eye level (~1.6m above floor)
            const auto camera = registry.create();
            registry.name(camera, "Camera");
            const auto camera_data = tempest::render_system::camera_component{
                .aspect_ratio = 16.0F / 9.0F,
                .vertical_fov = tempest::math::as_radians(60.0F),
                .near_plane = 0.05F,
            };
            registry.assign(camera, camera_data);
            auto camera_tx = tempest::ecs::transform_component{};
            camera_tx.position({0.0F, 1.6F, 0.0F});
            camera_tx.rotation(
                tempest::math::quat<float>(tempest::math::vec3<float>{-client->camera_pitch, client->camera_yaw, 0.0F}));
            registry.assign(camera, camera_tx);
            client->camera_entity = camera;

            // Directional Sun Light
            const auto sun = registry.create();
            registry.name(sun, "Sun");
            const auto sun_data = tempest::render_system::directional_light_component{
                .color = {1.0F, 0.98F, 0.95F},
                .intensity = 2.0F,
            };
            const auto sun_shadows = tempest::render_system::shadow_caster_component{
                .resolution = 2048,
                .num_cascades = 4,
                .split_lambda = 0.5F,
                .max_shadow_distance = 100.0F,
                .normal_bias = 0.015F,
                .depth_bias = 0.001F,
            };
            auto sun_tx = tempest::ecs::transform_component{};
            sun_tx.rotation({tempest::math::as_radians(65.0F), tempest::math::as_radians(25.0F), 0.0F});
            registry.assign_or_replace(sun, sun_shadows);
            registry.assign_or_replace(sun, sun_data);
            registry.assign_or_replace(sun, sun_tx);

            // Static Floor Entity
            const auto floor_entity = registry.create();
            auto floor_tx = tempest::ecs::transform_component{};
            floor_tx.position({0.0F, 0.0F, 0.0F});
            registry.assign(floor_entity, floor_tx);
            registry.assign(floor_entity, tempest::core::mesh_component{.mesh_id = client->floor_mesh_id});
            registry.assign(floor_entity, tempest::core::material_component{.material_id = client->floor_mat_id});
            registry.name(floor_entity, "FloorPlane");

            // Static Step Obstacle Entity
            const auto step_entity = registry.create();
            auto step_tx = tempest::ecs::transform_component{};
            step_tx.position({0.0F, 0.1F, 5.0F});
            registry.assign(step_entity, step_tx);
            registry.assign(step_entity, tempest::core::mesh_component{.mesh_id = client->step_mesh_id});
            registry.assign(step_entity, tempest::core::material_component{.material_id = client->step_mat_id});
            registry.name(step_entity, "StepObstacle");

            // In offline mode, spawn local player immediately
            if (client->is_offline)
            {
                spawn_local_player(ctx, client);

                // Spawn a stationary capsule proxy target in offline mode so the user can verify
                // the capsule mesh and shadow edge alignment without needing a dedicated server.
                const auto dummy_proxy = registry.create();
                auto dummy_tx = tempest::ecs::transform_component{};
                dummy_tx.position({0.0F, 0.92F, 2.5F});
                registry.assign(dummy_proxy, dummy_tx);
                registry.assign(dummy_proxy,
                                tempest::ecs::transform_history_component::create(dummy_tx.position()));
                registry.assign(dummy_proxy,
                                tempest::core::mesh_component{.mesh_id = client->remote_player_mesh_id});
                registry.assign(dummy_proxy,
                                tempest::core::material_component{.material_id = client->remote_player_mat_id});
                registry.name(dummy_proxy, "CapsuleProxyTarget");
            }

            // Window Input & Cursor Capture
            const auto win = ctx->get_main_window();
            if (win.is_valid())
            {
                auto& wm = ctx->get_window_manager();
                client->is_focused = wm.is_focused(win);
                wm.set_cursor_mode(win, tempest::cursor_mode::disabled);

                wm.register_cursor_pos_callback(win, [client](float xpos, float ypos) -> void {
                    if (!client->is_focused)
                    {
                        client->first_mouse = true;
                        return;
                    }

                    if (client->first_mouse)
                    {
                        client->last_mouse_x = xpos;
                        client->last_mouse_y = ypos;
                        client->first_mouse = false;
                        return;
                    }

                    const auto dx = xpos - client->last_mouse_x;
                    const auto dy = ypos - client->last_mouse_y;
                    client->last_mouse_x = xpos;
                    client->last_mouse_y = ypos;

                    client->camera_yaw += dx * multiplayer_client::default_mouse_sensitivity;
                    constexpr auto two_pi = tempest::math::constants::pi<float> * 2.0F;
                    while (client->camera_yaw > two_pi)
                    {
                        client->camera_yaw -= two_pi;
                    }
                    while (client->camera_yaw < 0.0F)
                    {
                        client->camera_yaw += two_pi;
                    }

                    client->camera_pitch = tempest::math::clamp(
                        client->camera_pitch - dy * multiplayer_client::default_mouse_sensitivity,
                        multiplayer_client::min_camera_pitch, multiplayer_client::max_camera_pitch);
                });

                wm.register_focus_callback(win, [client](bool focused) -> void {
                    client->is_focused = focused;
                    client->first_mouse = true;
                });

                wm.register_key_callback(win, [ctx](tempest::core::key_state state) -> void {
                    if (state.k == tempest::core::key::escape && state.action == tempest::core::key_action::press)
                    {
                        ctx->request_close(true);
                    }
                });
            }
        });

        ctx->register_on_fixed_update_callback([client, ctx]([[maybe_unused]] tempest::engine_context& engine_ctx,
                                                             tempest::chrono::duration<double> delta_time) -> void {
            const auto now = tempest::chrono::steady_clock::now();

            if (!client->is_offline)
            {
                // 1. Connection Handshake Retransmission
                if (!client->connected)
                {
                    if (now - client->last_connect_attempt > multiplayer_client::connect_retransmit_interval)
                    {
                        client->last_connect_attempt = now;
                        auto writer = tempest::network::bit_writer{};
                        const auto req = tempest::network::connect_request_payload{
                            .client_version = tempest::network::packet_header::default_protocol_version,
                            .client_nonce = 42ULL,
                        };
                        tempest::network::write_connect_request(writer, req);
                        writer.flush();

                        const auto packet_bytes = client->connection.build_packet(
                            tempest::network::packet_type::connect_request, writer.data(), now);
                        static_cast<void>(client->socket.send_to(
                            client->server_endpoint,
                            tempest::span<const tempest::byte>{packet_bytes.data(), packet_bytes.size()}));
                    }
                }

                // 2. Ingest Incoming Network Packets
                auto buffer = tempest::array<tempest::byte, 2048>{};
                auto sender = tempest::network::endpoint{};
                auto packets_processed = size_t{0};

                while (packets_processed < multiplayer_client::max_packets_per_tick)
                {
                    const auto bytes_read = client->socket.receive_from(
                        sender, tempest::span<tempest::byte>{buffer.data(), buffer.size()});
                    if (bytes_read == 0)
                    {
                        break;
                    }
                    ++packets_processed;

                    if (bytes_read < sizeof(tempest::network::packet_header))
                    {
                        continue;
                    }

                    const auto incoming = client->connection.process_packet(
                        tempest::span<const tempest::byte>{buffer.data(), bytes_read}, now);
                    if (!incoming.has_value())
                    {
                        continue;
                    }

                    const auto ptype = static_cast<tempest::network::packet_type>(incoming->header.type);
                    if (ptype == tempest::network::packet_type::connect_accepted)
                    {
                        if (!client->connected)
                        {
                            auto reader = tempest::network::bit_reader{incoming->payload};
                            const auto accepted_opt = tempest::network::read_connect_accepted(reader);
                            if (accepted_opt.has_value())
                            {
                                client->session_id = accepted_opt->session_id;
                                client->player_slot = accepted_opt->player_slot;
                                client->current_client_tick = accepted_opt->initial_server_tick;
                                client->connection.set_session_id(client->session_id);
                                client->connection.set_state(tempest::network::connection_state::connected);
                                client->connected = true;

                                ctx->get_logger().info("Connected to server! Handshake complete.");
                                spawn_local_player(ctx, client);
                            }
                        }
                    }
                    else if (ptype == tempest::network::packet_type::snapshot)
                    {
                        if (client->connected)
                        {
                            auto reader = tempest::network::bit_reader{incoming->payload};
                            auto world_snap = tempest::physics::world_snapshot_packet{};
                            if (tempest::physics::read_world_snapshot(reader, world_snap))
                            {
                                if (!client->first_snapshot_received)
                                {
                                    client->first_snapshot_received = true;
                                    ctx->get_logger().info("Received initial authoritative world snapshot.");
                                }
                                process_world_snapshot(ctx, client, world_snap);
                            }
                        }
                    }
                }
            }

            // 3. Player Input & Simulation
            const auto can_simulate = client->is_offline || (client->connected && client->local_player_entity != tempest::ecs::null);
            if (can_simulate && client->local_player_entity != tempest::ecs::null)
            {
                const auto win = ctx->get_main_window();
                auto forward_move = 0.0F;
                auto right_move = 0.0F;
                auto buttons = static_cast<tempest::uint16_t>(tempest::network::user_button_none);

                if (client->is_focused && win.is_valid())
                {
                    auto& wm = ctx->get_window_manager();
                    const auto& kb = wm.get_keyboard(win);

                    if (kb.is_key_down(tempest::core::key::w))
                    {
                        forward_move += 1.0F;
                    }
                    if (kb.is_key_down(tempest::core::key::s))
                    {
                        forward_move -= 1.0F;
                    }
                    if (kb.is_key_down(tempest::core::key::d))
                    {
                        right_move += 1.0F;
                    }
                    if (kb.is_key_down(tempest::core::key::a))
                    {
                        right_move -= 1.0F;
                    }

                    if (kb.is_key_down(tempest::core::key::space))
                    {
                        buttons |= tempest::network::user_button_jump;
                    }
                    if (kb.is_key_down(tempest::core::key::left_shift))
                    {
                        buttons |= tempest::network::user_button_sprint;
                    }
                    if (kb.is_key_down(tempest::core::key::c))
                    {
                        buttons |= tempest::network::user_button_crouch;
                    }
                }

                ++client->current_client_tick;
                const auto cmd = tempest::network::user_cmd{
                    .tick = client->current_client_tick,
                    .forward_move = forward_move,
                    .right_move = right_move,
                    .view_yaw = client->camera_yaw,
                    .buttons = buttons,
                };

                // Orient physics character according to camera yaw
                const auto* controller =
                    ctx->get_entities().try_get<tempest::physics::character_controller_component>(client->local_player_entity);
                if (controller != nullptr)
                {
                    auto* char_obj = client->client_physics_world.get_character(controller->id);
                    if (char_obj != nullptr)
                    {
                        const auto yaw_rot =
                            tempest::math::quat<float>(tempest::math::vec3<float>{0.0F, client->camera_yaw, 0.0F});
                        char_obj->set_rotation(jolt::shim::quat{yaw_rot.x, yaw_rot.y, yaw_rot.z, yaw_rot.w});
                    }
                }

                if (client->is_offline)
                {
                    // Offline local simulation
                    tempest::physics::step_character_simulation(client->client_physics_world, ctx->get_entities(),
                                                               client->local_player_entity, cmd, delta_time);
                }
                else
                {
                    // Online predictive simulation
                    tempest::physics::step_client_prediction(client->client_physics_world, ctx->get_entities(),
                                                             client->local_player_entity, cmd, delta_time,
                                                             client->prediction_buffer);

                    // Add to redundant command history
                    client->command_history.push_back(cmd);
                    if (client->command_history.size() > multiplayer_client::max_command_history)
                    {
                        client->command_history.erase(client->command_history.begin());
                    }

                    // Transmit input packet with redundant commands
                    const auto total_history = client->command_history.size();
                    const auto send_count = tempest::min(total_history, tempest::network::max_commands_per_packet);
                    auto packet_cmds = tempest::inplace_vector<tempest::network::user_cmd, 4>{};
                    for (size_t i = total_history - send_count; i < total_history; ++i)
                    {
                        packet_cmds.push_back(client->command_history[i]);
                    }

                    auto writer = tempest::network::bit_writer{};
                    const auto count = packet_cmds.size();
                    const auto count_code = static_cast<tempest::uint32_t>(count - 1U);
                    writer.write_bits(count_code, 2);
                    for (size_t i = 0; i < count; ++i)
                    {
                        tempest::network::write_user_cmd(writer, packet_cmds[i]);
                    }
                    writer.flush();

                    const auto packet_bytes =
                        client->connection.build_packet(tempest::network::packet_type::input, writer.data(), now);
                    static_cast<void>(client->socket.send_to(
                        client->server_endpoint,
                        tempest::span<const tempest::byte>{packet_bytes.data(), packet_bytes.size()}));
                }
            }
        });

        ctx->register_on_interpolate_callback([client](tempest::engine_context& engine_ctx,
                                                       [[maybe_unused]] float alpha) -> void {
            if (client->local_player_entity == tempest::ecs::null)
            {
                return;
            }

            auto& registry = engine_ctx.get_entities();
            const auto now = tempest::chrono::steady_clock::now();
            const auto dt = (client->last_render_time.time_since_epoch().count() == 0)
                                ? tempest::chrono::duration<double>{1.0 / 60.0}
                                : tempest::chrono::duration_cast<tempest::chrono::duration<double>>(
                                      now - client->last_render_time);
            client->last_render_time = now;

            // 1. Decay visual reconciliation error smoothing
            tempest::physics::update_reconciliation_smoothing(registry, dt);

            // 2. Position first-person camera relative to local player's transform
            const auto* player_tx = registry.try_get<tempest::ecs::transform_component>(client->local_player_entity);
            if (player_tx == nullptr)
            {
                return;
            }

            // Keep local player orientation aligned with camera yaw
            auto updated_tx = *player_tx;
            updated_tx.rotation(
                tempest::math::quat<float>(tempest::math::vec3<float>{0.0F, client->camera_yaw, 0.0F}));
            registry.assign_or_replace(client->local_player_entity, updated_tx);

            // Camera positioned at eye level (0.7m above character center, ~1.6m above floor)
            const auto char_pos = updated_tx.position();
            const auto cam_pos = char_pos + tempest::math::vec3<float>{0.0F, 0.70F, 0.0F};
            const auto cam_rot = tempest::math::quat<float>(tempest::math::vec3<float>{
                -client->camera_pitch,
                client->camera_yaw,
                0.0F,
            });

            auto cam_tx = tempest::ecs::transform_component{};
            cam_tx.position(cam_pos);
            cam_tx.rotation(cam_rot);
            registry.assign_or_replace(client->camera_entity, cam_tx);

            // 3. Smoothly interpolate remote player visual proxies between snapshots
            for (auto& remote : client->remote_players)
            {
                if (remote.entity == tempest::ecs::null)
                {
                    continue;
                }

                remote.interp_factor =
                    tempest::min(1.0F, remote.interp_factor +
                                           static_cast<float>(dt.count()) * remote_player_entry::default_interpolation_speed);
                const auto interp_pos =
                    tempest::math::lerp(remote.current_pos, remote.target_pos, remote.interp_factor);
                const auto interp_rot =
                    tempest::math::slerp(remote.current_rot, remote.target_rot, remote.interp_factor);

                auto proxy_tx = tempest::ecs::transform_component{};
                proxy_tx.position(interp_pos);
                proxy_tx.rotation(interp_rot);
                registry.assign_or_replace(remote.entity, proxy_tx);
            }
        });

        ctx->register_on_close_callback([client]([[maybe_unused]] tempest::engine_context& engine_ctx) -> void {
            if (client != nullptr)
            {
                if (client->socket.is_open())
                {
                    if (client->connected)
                    {
                        auto writer = tempest::network::bit_writer{};
                        const auto disco = tempest::network::disconnect_payload{
                            .reason = tempest::network::disconnect_reason::user_quit,
                        };
                        tempest::network::write_disconnect(writer, disco);
                        writer.flush();

                        const auto now = tempest::chrono::steady_clock::now();
                        const auto packet_bytes = client->connection.build_packet(
                            tempest::network::packet_type::disconnect, writer.data(), now);
                        static_cast<void>(client->socket.send_to(
                            client->server_endpoint,
                            tempest::span<const tempest::byte>{packet_bytes.data(), packet_bytes.size()}));
                    }
                    client->socket.close();
                }

                cleanup_client_static_environment(client->client_physics_world, client->static_bodies,
                                                  client->static_shapes);
                delete client;
            }
        });

        // Transfer unique ownership to the registered on_close engine lifecycle callback
        static_cast<void>(client_owner.release());
    }
} // namespace

extern "C"
{
    GAME_API void on_load(tempest::client_context* ctx, [[maybe_unused]] tempest::span<tempest::string_view> args)
    {
        auto& logger = ctx->get_logger();
        auto server_ep = tempest::optional<tempest::network::endpoint>{};

        for (const auto& arg : args)
        {
            constexpr auto prefix = tempest::string_view{"--connect="};
            if (arg.size() >= prefix.size() && starts_with(arg, prefix))
            {
                const auto target = substr(arg, prefix.size());
                server_ep = tempest::network::endpoint::parse(target);
                if (!server_ep.has_value())
                {
                    logger.warn("Failed to parse connect endpoint, falling back to standalone.");
                }
            }
        }

        if (server_ep.has_value())
        {
            logger.info("Starting in multiplayer mode, connecting to server...");
            setup_multiplayer_client(ctx, *server_ep, false);
        }
        else
        {
            logger.info("Starting in standalone offline mode with first-person character and feet indicator.");
            setup_multiplayer_client(ctx, tempest::network::endpoint::any(0), true);
        }
    }

    GAME_API void on_unload()
    {
        // Cleanup code for the game goes here
    }
}