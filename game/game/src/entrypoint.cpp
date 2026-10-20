#define TEMPEST_GAME_EXPORT
#include <tempest/game_module.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
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
#include <tempest/physics/box_collider_component.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/character_movement_intent.hpp>
#include <tempest/physics/character_prediction.hpp>
#include <tempest/physics/character_protocol.hpp>
#include <tempest/physics/character_reconciliation.hpp>
#include <tempest/physics/character_snapshot.hpp>
#include <tempest/physics/physics_scene_system.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/physics/rigid_body_component.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/quat.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/transform_history_system.hpp>
#include <tempest/transformations.hpp>
#include <tempest/utility.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vector.hpp>

#ifndef GAME_API
#define GAME_API TEMPEST_GAME_API
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

        add_face({0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F}, {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}, color);
        add_face({0.0F, 0.0F, -1.0F}, {-1.0F, 0.0F, 0.0F}, {x, -y, -z}, {-x, -y, -z}, {-x, y, -z}, {x, y, -z}, color);
        add_face({-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {-x, -y, -z}, {-x, -y, z}, {-x, y, z}, {-x, y, -z}, color);
        add_face({1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, {x, -y, z}, {x, -y, -z}, {x, y, -z}, {x, y, z}, color);
        add_face({0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {-x, y, z}, {x, y, z}, {x, y, -z}, {-x, y, -z}, color);
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

        for (auto ring_idx = 0U; ring_idx < num_rings; ++ring_idx)
        {
            auto latitude = 0.0F;
            auto y_offset = 0.0F;

            if (ring_idx <= rings_per_cap)
            {
                const auto t = static_cast<float>(ring_idx) / static_cast<float>(rings_per_cap);
                latitude = -half_pi + (t * half_pi);
                y_offset = -cylinder_half_height;
            }
            else
            {
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
                    m.indices.push_back(v00);
                    m.indices.push_back(v10);
                    m.indices.push_back(v11);
                }
                else if (ring_idx == num_rings - 2U)
                {
                    m.indices.push_back(v00);
                    m.indices.push_back(v11);
                    m.indices.push_back(v01);
                }
                else
                {
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

        bool is_offline = false;
        bool is_focused = default_is_focused;

        tempest::network::network_context net_ctx = {};
        tempest::network::endpoint server_endpoint = {};
        tempest::network::udp_socket socket = {};
        tempest::network::connection connection = {};
        tempest::uint64_t session_id = default_session_id;
        tempest::uint32_t player_slot = default_player_slot;
        bool connected = false;
        bool first_snapshot_received = false;
        tempest::chrono::steady_clock::time_point last_connect_attempt = {};

        tempest::physics::physics_world client_physics_world = {};
        tempest::unique_ptr<tempest::physics::physics_scene_system> physics_scene = nullptr;
        tempest::ecs::entity local_player_entity = tempest::ecs::null;
        tempest::ecs::entity camera_entity = tempest::ecs::null;
        tempest::physics::character_prediction_buffer prediction_buffer = {};
        tempest::uint32_t current_client_tick = default_client_tick;

        tempest::vector<tempest::network::user_cmd> command_history = {};

        tempest::vector<remote_player_entry> remote_players = {};

        float camera_yaw = default_camera_yaw;
        float camera_pitch = default_camera_pitch;
        float last_mouse_x = default_mouse_coord;
        float last_mouse_y = default_mouse_coord;
        bool first_mouse = true;
        tempest::chrono::steady_clock::time_point last_render_time = {};

        tempest::guid local_player_mesh_id = {};
        tempest::guid remote_player_mesh_id = {};
        tempest::guid local_player_mat_id = {};
        tempest::guid remote_player_mat_id = {};
    };

    tempest::engine_context* g_engine = nullptr;
    tempest::client_context* g_client_ctx = nullptr;
    tempest::unique_ptr<multiplayer_client> g_client = nullptr;

    auto spawn_local_player(tempest::engine_context* ctx, multiplayer_client* client) -> void
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

    auto despawn_local_player(tempest::engine_context* ctx, multiplayer_client* client) -> void
    {
        if (client->local_player_entity == tempest::ecs::null)
        {
            return;
        }

        const auto* controller =
            ctx->get_entities().try_get<tempest::physics::character_controller_component>(client->local_player_entity);
        if (controller != nullptr)
        {
            client->client_physics_world.despawn_character(controller->id);
        }

        if (ctx->get_entities().is_valid(client->local_player_entity))
        {
            ctx->get_entities().destroy(client->local_player_entity);
        }
        client->local_player_entity = tempest::ecs::null;
        client->prediction_buffer.reset();
        client->command_history.clear();
    }

    auto despawn_remote_players(tempest::engine_context* ctx, multiplayer_client* client) -> void
    {
        for (const auto& remote : client->remote_players)
        {
            if (remote.entity != tempest::ecs::null && ctx->get_entities().is_valid(remote.entity))
            {
                ctx->get_entities().destroy(remote.entity);
            }
        }
        client->remote_players.clear();
    }

    auto process_world_snapshot(tempest::engine_context* ctx, multiplayer_client* client,
                                const tempest::physics::world_snapshot_packet& world_snap) -> void
    {
        auto& registry = ctx->get_entities();

        for (const auto& player_snap : world_snap.players)
        {
            if (player_snap.session_id == client->session_id)
            {
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
} // namespace

extern "C"
{
    TEMPEST_GAME_API auto tempest_game_register_types(tempest::ecs::component_type_registry& types) -> void
    {
        types.register_type<tempest::ecs::transform_component>();
        types.register_type<tempest::ecs::transform_history_component>();
        types.register_type<tempest::core::mesh_component>();
        types.register_type<tempest::core::material_component>();
        types.register_type<tempest::render_system::camera_component>();
        types.register_type<tempest::render_system::active_camera_component>();
        types.register_type<tempest::render_system::directional_light_component>();
        types.register_type<tempest::render_system::shadow_caster_component>();
        types.register_type<tempest::physics::rigid_body_component>();
        types.register_type<tempest::physics::box_collider_component>();
        types.register_type<tempest::physics::velocity_component>();
        types.register_type<tempest::physics::character_controller_component>();
        types.register_type<tempest::physics::character_movement_intent>();
        types.register_type<tempest::physics::reconciliation_smoothing_component>();
    }

    TEMPEST_GAME_API auto tempest_game_on_load(tempest::engine_context& ctx,
                                               tempest::span<const tempest::string_view> args) -> void
    {
        g_engine = &ctx;
        g_client_ctx = dynamic_cast<tempest::client_context*>(&ctx);
        g_client = tempest::make_unique<multiplayer_client>();
        auto* const client = g_client.get();

        auto server_ep = tempest::optional<tempest::network::endpoint>{};
        for (const auto& arg : args)
        {
            constexpr auto prefix = tempest::string_view{"--connect="};
            if (tempest::starts_with(arg, prefix))
            {
                const auto target = substr(arg, prefix.size());
                server_ep = tempest::network::endpoint::parse(target);
                if (!server_ep.has_value())
                {
                    ctx.get_logger().warn("Failed to parse connect endpoint, falling back to standalone.");
                }
            }
        }

        if (server_ep.has_value())
        {
            if (server_ep->port() == 0)
            {
                server_ep = tempest::network::endpoint{server_ep->address(), tempest::play_settings::default_port};
            }
            client->server_endpoint = *server_ep;
            client->is_offline = false;
        }
        else
        {
            client->is_offline = true;
        }

        client->physics_scene = tempest::make_unique<tempest::physics::physics_scene_system>(
            client->client_physics_world, ctx.get_entities(), ctx.get_events());

        if (g_client_ctx != nullptr)
        {
            client->local_player_mesh_id = g_client_ctx->get_meshes().register_mesh(
                create_box_mesh({0.18F, 0.08F, 0.22F}, {1.0F, 1.0F, 1.0F, 1.0F}, {0.0F, -0.82F, 0.15F}));
            client->remote_player_mesh_id = g_client_ctx->get_meshes().register_mesh(
                create_capsule_mesh(0.5F, 0.4F, {1.0F, 1.0F, 1.0F, 1.0F}));

            auto local_mat = tempest::core::material{};
            local_mat.set_vec4(tempest::core::material::base_color_factor_name, {0.95F, 0.55F, 0.15F, 1.0F});
            local_mat.set_vec3(tempest::core::material::emissive_factor_name, {0.0F, 0.0F, 0.0F});
            local_mat.set_scalar(tempest::core::material::metallic_factor_name, 0.1F);
            local_mat.set_scalar(tempest::core::material::roughness_factor_name, 0.5F);
            client->local_player_mat_id = g_client_ctx->get_materials().register_material(tempest::move(local_mat));

            auto remote_mat = tempest::core::material{};
            remote_mat.set_vec4(tempest::core::material::base_color_factor_name, {0.15F, 0.75F, 0.95F, 1.0F});
            remote_mat.set_vec3(tempest::core::material::emissive_factor_name, {0.0F, 0.0F, 0.0F});
            remote_mat.set_scalar(tempest::core::material::metallic_factor_name, 0.1F);
            remote_mat.set_scalar(tempest::core::material::roughness_factor_name, 0.5F);
            client->remote_player_mat_id = g_client_ctx->get_materials().register_material(tempest::move(remote_mat));
        }

        ctx.register_on_initialize_callback([client](tempest::engine_context& engine_ctx) -> void {
            auto* const cl_ctx = dynamic_cast<tempest::client_context*>(&engine_ctx);
            if (cl_ctx == nullptr)
            {
                return;
            }

            const auto win = cl_ctx->get_main_window();
            if (win.is_valid())
            {
                auto& wm = cl_ctx->get_window_manager();
                client->is_focused = wm.is_focused(win);
                if (cl_ctx->is_play_mode())
                {
                    wm.set_cursor_mode(win, tempest::cursor_mode::disabled);
                }
                else
                {
                    wm.set_cursor_mode(win, tempest::cursor_mode::normal);
                }

                wm.register_cursor_pos_callback(win, [client, cl_ctx](float xpos, float ypos) -> void {
                    if (!cl_ctx->is_play_mode() || !client->is_focused)
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

                wm.register_key_callback(win, [cl_ctx](tempest::core::key_state state) -> void {
                    if (state.k == tempest::core::key::escape && state.action == tempest::core::key_action::press)
                    {
                        if (cl_ctx->is_play_mode())
                        {
                            const auto main_win = cl_ctx->get_main_window();
                            if (main_win.is_valid())
                            {
                                cl_ctx->get_window_manager().set_cursor_mode(main_win, tempest::cursor_mode::normal);
                            }
                        }
                        else
                        {
                            cl_ctx->request_close(true);
                        }
                    }
                });
            }
        });

        ctx.register_on_fixed_update_callback([client](tempest::engine_context& engine_ctx,
                                                       tempest::chrono::duration<double> delta_time) -> void {
            if (!engine_ctx.is_play_mode())
            {
                return;
            }

            const auto now = tempest::chrono::steady_clock::now();

            if (!client->is_offline)
            {
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

                                engine_ctx.get_logger().info("Connected to server! Handshake complete.");
                                spawn_local_player(&engine_ctx, client);
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
                                    engine_ctx.get_logger().info("Received initial authoritative world snapshot.");
                                }
                                process_world_snapshot(&engine_ctx, client, world_snap);
                            }
                        }
                    }
                }
            }

            if (client->physics_scene != nullptr)
            {
                client->physics_scene->pre_simulation_flush();
            }

            const auto can_simulate = client->is_offline || (client->connected && client->local_player_entity != tempest::ecs::null);
            if (can_simulate && client->local_player_entity != tempest::ecs::null)
            {
                auto* const cl_ctx = dynamic_cast<tempest::client_context*>(&engine_ctx);
                const auto win = (cl_ctx != nullptr) ? cl_ctx->get_main_window() : tempest::window_handle{};
                auto forward_move = 0.0F;
                auto right_move = 0.0F;
                auto buttons = static_cast<tempest::uint16_t>(tempest::network::user_button_none);

                if (cl_ctx != nullptr && client->is_focused && win.is_valid() && engine_ctx.is_play_mode())
                {
                    auto& wm = cl_ctx->get_window_manager();
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

                const auto* controller =
                    engine_ctx.get_entities().try_get<tempest::physics::character_controller_component>(client->local_player_entity);
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
                    tempest::physics::step_character_simulation(client->client_physics_world, engine_ctx.get_entities(),
                                                               client->local_player_entity, cmd, delta_time);
                }
                else
                {
                    tempest::physics::step_client_prediction(client->client_physics_world, engine_ctx.get_entities(),
                                                             client->local_player_entity, cmd, delta_time,
                                                             client->prediction_buffer);

                    client->command_history.push_back(cmd);
                    if (client->command_history.size() > multiplayer_client::max_command_history)
                    {
                        client->command_history.erase(client->command_history.begin());
                    }

                    const auto total_history = client->command_history.size();
                    const auto send_count = tempest::min(total_history, tempest::network::max_commands_per_packet);
                    auto packet_cmds = tempest::inplace_vector<tempest::network::user_cmd, 4>{};
                    for (auto cmd_idx = total_history - send_count; cmd_idx < total_history; ++cmd_idx)
                    {
                        packet_cmds.push_back(client->command_history[cmd_idx]);
                    }

                    auto writer = tempest::network::bit_writer{};
                    const auto count = packet_cmds.size();
                    const auto count_code = static_cast<tempest::uint32_t>(count - 1U);
                    writer.write_bits(count_code, 2);
                    for (auto cmd_idx = 0ULL; cmd_idx < count; ++cmd_idx)
                    {
                        tempest::network::write_user_cmd(writer, packet_cmds[cmd_idx]);
                    }
                    writer.flush();

                    const auto packet_bytes =
                        client->connection.build_packet(tempest::network::packet_type::input, writer.data(), now);
                    static_cast<void>(client->socket.send_to(
                        client->server_endpoint,
                        tempest::span<const tempest::byte>{packet_bytes.data(), packet_bytes.size()}));
                }
            }
            else
            {
                client->client_physics_world.step(delta_time);
            }

            if (client->physics_scene != nullptr)
            {
                client->physics_scene->post_simulation_sync();
            }
        });

        if (g_client_ctx != nullptr)
        {
            g_client_ctx->register_on_interpolate_callback([client](tempest::engine_context& engine_ctx,
                                                                   [[maybe_unused]] float alpha) -> void {
                if (!engine_ctx.is_play_mode() || client->local_player_entity == tempest::ecs::null)
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

                tempest::physics::update_reconciliation_smoothing(registry, dt);

                const auto* player_tx = registry.try_get<tempest::ecs::transform_component>(client->local_player_entity);
                if (player_tx == nullptr)
                {
                    return;
                }

                auto updated_tx = *player_tx;
                updated_tx.rotation(
                    tempest::math::quat<float>(tempest::math::vec3<float>{0.0F, client->camera_yaw, 0.0F}));
                registry.assign_or_replace(client->local_player_entity, updated_tx);

                if (client->camera_entity == tempest::ecs::null)
                {
                    const auto cam_opt = registry.find_first_with_name("Camera");
                    if (cam_opt.has_value())
                    {
                        client->camera_entity = *cam_opt;
                    }
                    else
                    {
                        for (const auto [self, cam] :
                             registry.with<tempest::ecs::self_component, tempest::render_system::camera_component>())
                        {
                            client->camera_entity = self.entity;
                            break;
                        }
                    }
                }

                if (client->camera_entity != tempest::ecs::null)
                {
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
                }

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
        }

        ctx.register_on_close_callback([]([[maybe_unused]] tempest::engine_context& engine_ctx) -> void {
            tempest_game_on_play_end();
            tempest_game_on_unload();
        });
    }

    TEMPEST_GAME_API auto tempest_game_on_play_begin(const tempest::play_settings& settings) -> void
    {
        if (g_client == nullptr || g_engine == nullptr)
        {
            return;
        }

        auto* const client = g_client.get();
        auto* const ctx = g_engine;

        client->is_offline = (settings.mode == tempest::network_mode::standalone_offline);

        if (client->physics_scene != nullptr)
        {
            client->physics_scene->init_from_registry();
        }

        tempest::ecs::sync_transform_history_from_transforms(ctx->get_entities());

        client->camera_entity = tempest::ecs::null;
        const auto cam_opt = ctx->get_entities().find_first_with_name("Camera");
        if (cam_opt.has_value())
        {
            client->camera_entity = *cam_opt;
        }
        else
        {
            for (const auto [self, cam] :
                 ctx->get_entities().with<tempest::ecs::self_component, tempest::render_system::camera_component>())
            {
                client->camera_entity = self.entity;
                break;
            }
        }

        client->camera_yaw = multiplayer_client::default_camera_yaw;
        client->camera_pitch = multiplayer_client::default_camera_pitch;
        client->first_mouse = true;
        client->last_mouse_x = multiplayer_client::default_mouse_coord;
        client->last_mouse_y = multiplayer_client::default_mouse_coord;

        if (client->camera_entity != tempest::ecs::null)
        {
            const auto cam_rot = tempest::math::quat<float>(tempest::math::vec3<float>{
                -client->camera_pitch,
                client->camera_yaw,
                0.0F,
            });
            const auto spawn_pos = client->is_offline
                                       ? tempest::math::vec3<float>{0.0F, 1.0F, 0.0F}
                                       : tempest::math::vec3<float>{static_cast<float>(client->player_slot) * 2.0F, 1.0F, 0.0F};
            auto cam_tx = tempest::ecs::transform_component{};
            cam_tx.position(spawn_pos + tempest::math::vec3<float>{0.0F, 0.70F, 0.0F});
            cam_tx.rotation(cam_rot);
            ctx->get_entities().assign_or_replace(client->camera_entity, cam_tx);
        }

        if (!client->is_offline)
        {
            auto ep_opt = tempest::network::endpoint::parse(settings.remote_address);
            auto ep = ep_opt.has_value() ? *ep_opt : tempest::network::endpoint::loopback(settings.port);
            if (ep.port() == 0)
            {
                ep = tempest::network::endpoint{ep.address(), settings.port};
            }
            client->server_endpoint = ep;

            if (!client->socket.open())
            {
                ctx->get_logger().error("Failed to open UDP socket on client.");
            }
            else
            {
                if (client->socket.bind(tempest::network::endpoint::any(0)))
                {
                    ctx->get_logger().info("Client UDP socket opened and bound successfully.");
                }
                else
                {
                    ctx->get_logger().error("Failed to bind UDP socket on client.");
                }
            }
            client->connection.set_remote_endpoint(client->server_endpoint);
        }

        if (client->is_offline)
        {
            spawn_local_player(ctx, client);
        }

        if (g_client_ctx != nullptr)
        {
            const auto win = g_client_ctx->get_main_window();
            if (win.is_valid())
            {
                g_client_ctx->get_window_manager().set_cursor_mode(win, tempest::cursor_mode::disabled);
            }
        }
        client->first_mouse = true;
    }

    TEMPEST_GAME_API auto tempest_game_on_play_end() -> void
    {
        if (g_client == nullptr || g_engine == nullptr)
        {
            return;
        }

        auto* const client = g_client.get();
        auto* const ctx = g_engine;

        despawn_local_player(ctx, client);
        despawn_remote_players(ctx, client);

        if (client->physics_scene != nullptr)
        {
            client->physics_scene->clear();
        }

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
                const auto packet_bytes =
                    client->connection.build_packet(tempest::network::packet_type::disconnect, writer.data(), now);
                static_cast<void>(client->socket.send_to(
                    client->server_endpoint,
                    tempest::span<const tempest::byte>{packet_bytes.data(), packet_bytes.size()}));
            }
            client->socket.close();
        }
        client->connected = false;
        client->first_snapshot_received = false;
        client->session_id = multiplayer_client::default_session_id;

        if (g_client_ctx != nullptr)
        {
            const auto win = g_client_ctx->get_main_window();
            if (win.is_valid())
            {
                g_client_ctx->get_window_manager().set_cursor_mode(win, tempest::cursor_mode::normal);
            }
        }
        client->camera_yaw = multiplayer_client::default_camera_yaw;
        client->camera_pitch = multiplayer_client::default_camera_pitch;
        client->first_mouse = true;
        client->last_mouse_x = multiplayer_client::default_mouse_coord;
        client->last_mouse_y = multiplayer_client::default_mouse_coord;
        client->camera_entity = tempest::ecs::null;
    }

    TEMPEST_GAME_API auto tempest_game_on_unload() -> void
    {
        if (g_client != nullptr)
        {
            g_client->physics_scene.reset();
            g_client.reset();
        }
        g_engine = nullptr;
        g_client_ctx = nullptr;
    }

    GAME_API void on_load(tempest::client_context* ctx, [[maybe_unused]] tempest::span<tempest::string_view> args)
    {
        tempest_game_register_types(ctx->get_components());

        auto const_args = tempest::vector<tempest::string_view>{};
        for (const auto& arg : args)
        {
            const_args.push_back(arg);
        }
        tempest_game_on_load(*ctx, tempest::span<const tempest::string_view>{const_args.data(), const_args.size()});

        auto settings = tempest::play_settings{};
        for (const auto& arg : args)
        {
            constexpr auto prefix = tempest::string_view{"--connect="};
            if (tempest::starts_with(arg, prefix))
            {
                settings.mode = tempest::network_mode::connect_remote;
                settings.remote_address = substr(arg, prefix.size());
            }
        }
        tempest_game_on_play_begin(settings);
    }

    GAME_API void on_unload()
    {
        tempest_game_on_play_end();
        tempest_game_on_unload();
    }
}