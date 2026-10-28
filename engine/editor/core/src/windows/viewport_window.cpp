#include <tempest/windows/viewport_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/command_history.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/limits.hpp>
#include <tempest/logger.hpp>
#include <tempest/material.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/physics/box_collider_component.hpp>
#include <tempest/play_mode_controller.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/ui.hpp>
#include <tempest/utility.hpp>

#include <imgui.h>
#include <imgui_internal.h>

namespace tempest::editor
{
    namespace
    {
        auto intersect_ray_aabb(const math::vec3<float>& ray_origin, const math::vec3<float>& ray_direction,
                                const math::vec3<float>& box_min, const math::vec3<float>& box_max,
                                float& out_t) noexcept -> bool
        {
            auto t_min = 0.0F;
            auto t_max = tempest::numeric_limits<float>::infinity();

            for (size_t axis_index = 0; axis_index < 3; ++axis_index)
            {
                constexpr auto near_zero = 1e-6F;
                if (math::abs(ray_direction[axis_index]) < near_zero)
                {
                    if (ray_origin[axis_index] < box_min[axis_index] || ray_origin[axis_index] > box_max[axis_index])
                    {
                        return false;
                    }
                }
                else
                {
                    const auto inv_dir = 1.0F / ray_direction[axis_index];
                    auto t_entry = (box_min[axis_index] - ray_origin[axis_index]) * inv_dir;
                    auto t_exit = (box_max[axis_index] - ray_origin[axis_index]) * inv_dir;
                    if (t_entry > t_exit)
                    {
                        tempest::swap(t_entry, t_exit);
                    }
                    t_min = tempest::max(t_min, t_entry);
                    t_max = tempest::min(t_max, t_exit);
                    if (t_min > t_max)
                    {
                        return false;
                    }
                }
            }

            if (t_max < 0.0F)
            {
                return false;
            }

            out_t = (t_min >= 0.0F) ? t_min : t_max;
            return true;
        }
    } // namespace

    viewport_window::viewport_window(editor_engine_context& ctx,
                                     command_history* in_history,
                                     scene::prefab_cache* in_prefab_cache,
                                     assets::asset_database* in_asset_db)
        : _ctx{ctx}, _history{in_history}, _prefab_cache{in_prefab_cache}, _asset_db{in_asset_db},
          _viewport_size{default_viewport_size}
    {
        if (_prefab_cache == nullptr)
        {
            _owned_prefab_cache = make_unique<scene::prefab_cache>();
            _prefab_cache = _owned_prefab_cache.get();
        }
    }

    auto viewport_window::_resolve_prefab_cache() noexcept -> scene::prefab_cache*
    {
        return _prefab_cache;
    }

    auto viewport_window::_resolve_asset_database() const noexcept -> assets::asset_database*
    {
        if (_asset_db != nullptr)
        {
            return _asset_db;
        }
        auto* const scn_db = _ctx->get_scene_manager().get_asset_database();
        if (scn_db != nullptr)
        {
            return scn_db;
        }
        return &_ctx->get_assets();
    }

    auto viewport_window::deduce_asset_kind(const asset_payload& payload) const noexcept -> dropped_asset_kind
    {
        const auto type_name = payload.type_name_view();
        if (type_name == "prefab" || type_name == "model" || type_name == "entity_hierarchy")
        {
            return dropped_asset_kind::prefab;
        }
        if (type_name == "material")
        {
            return dropped_asset_kind::material;
        }

        if (payload.type_hash != 0)
        {
            if (payload.type_hash == assets::asset_type_id::of<assets::entity_hierarchy>().hash())
            {
                return dropped_asset_kind::prefab;
            }
            if (payload.type_hash == assets::asset_type_id::of<core::material>().hash())
            {
                return dropped_asset_kind::material;
            }
        }

        const auto path = payload.path_view();
        if (tempest::ends_with(path, ".tprefab") || tempest::ends_with(path, ".gltf") ||
            tempest::ends_with(path, ".glb"))
        {
            return dropped_asset_kind::prefab;
        }
        if (tempest::ends_with(path, ".tmaterial") || tempest::ends_with(path, ".mat"))
        {
            return dropped_asset_kind::material;
        }

        const auto sub_key = payload.sub_asset_key_view();
        if (tempest::starts_with(sub_key, "prefab") || tempest::starts_with(sub_key, "model"))
        {
            return dropped_asset_kind::prefab;
        }
        if (tempest::starts_with(sub_key, "material"))
        {
            return dropped_asset_kind::material;
        }

        const auto* const asset_database = _resolve_asset_database();
        if (asset_database != nullptr)
        {
            const auto* entry = asset_database->find_by_guid(payload.asset_guid);
            if (entry == nullptr && !path.empty())
            {
                entry = asset_database->find_by_path(path);
            }
            if (entry != nullptr)
            {
                if (entry->type == assets::asset_type_id::of<assets::entity_hierarchy>())
                {
                    return dropped_asset_kind::prefab;
                }
                if (entry->type == assets::asset_type_id::of<core::material>())
                {
                    return dropped_asset_kind::material;
                }
            }
        }

        if (_prefab_cache != nullptr && _prefab_cache->has_template(payload.asset_guid))
        {
            return dropped_asset_kind::prefab;
        }

        return dropped_asset_kind::unknown;
    }

    auto viewport_window::desired_initial_dock() const -> editor_window::dock_location
    {
        return dock_location::center;
    }

    auto viewport_window::window_name() const -> string_view
    {
        return "Viewport";
    }

    auto viewport_window::compute_camera_ray(math::vec2<float> mouse_pos, math::vec2<float> vp_size) const
        -> camera_ray
    {
        const auto& cam = _ctx->get_editor_camera();
        const auto cam_position = cam.get_position();
        const auto cam_forward = cam.get_forward_vector();

        if (vp_size.x <= 0.0F || vp_size.y <= 0.0F)
        {
            return camera_ray{.origin = cam_position, .direction = cam_forward};
        }

        const auto aspect = vp_size.x / vp_size.y;
        const auto vertical_fov = cam.get_vertical_fov();
        const auto half_tan_fov = math::tan(vertical_fov * 0.5F);

        // Map mouse_pos from viewport screen space [0, vp_size] to normalized coordinates [-1, 1]
        const auto ndc_x = (2.0F * mouse_pos.x / vp_size.x) - 1.0F;
        const auto ndc_y = 1.0F - (2.0F * mouse_pos.y / vp_size.y);

        const auto cam_right = cam.get_right_vector();
        const auto cam_up = cam.get_up_vector();

        const auto ray_direction = cam_forward +
                                   (cam_right * (ndc_x * half_tan_fov * aspect)) +
                                   (cam_up * (ndc_y * half_tan_fov));

        return camera_ray{.origin = cam_position, .direction = math::normalize(ray_direction)};
    }

    auto viewport_window::calculate_placement_position(math::vec2<float> mouse_pos, math::vec2<float> vp_size) const
        -> placement_result
    {
        const auto ray = compute_camera_ray(mouse_pos, vp_size);
        auto closest_distance = tempest::numeric_limits<float>::infinity();
        auto hit_entity = static_cast<ecs::entity>(ecs::tombstone);
        auto hit_position = math::vec3<float>{};
        auto has_hit = false;

        const auto& entity_registry = _ctx->get_entities();

        // Check scene entities with transform_component
        for (const auto [self, transform] :
             entity_registry.with<ecs::self_component, ecs::transform_component>())
        {
            if (!entity_registry.is_valid(self.entity))
            {
                continue;
            }

            const auto* const collider = entity_registry.try_get<physics::box_collider_component>(self.entity);
            auto half_extents = math::vec3<float>{};
            auto box_center = math::vec3<float>{};

            if (collider != nullptr)
            {
                const auto scale = transform.scale();
                half_extents = math::vec3<float>{
                    math::abs(collider->half_extents.x * scale.x),
                    math::abs(collider->half_extents.y * scale.y),
                    math::abs(collider->half_extents.z * scale.z),
                };
                box_center = transform.position() + collider->offset;
            }
            else
            {
                const auto scale = transform.scale();
                constexpr auto default_half_extent = 0.5F;
                constexpr auto min_extent = 0.1F;
                half_extents = math::vec3<float>{
                    tempest::max(min_extent, math::abs(default_half_extent * scale.x)),
                    tempest::max(min_extent, math::abs(default_half_extent * scale.y)),
                    tempest::max(min_extent, math::abs(default_half_extent * scale.z)),
                };
                box_center = transform.position();
            }

            const auto box_min = box_center - half_extents;
            const auto box_max = box_center + half_extents;

            auto t_hit = 0.0F;
            if (intersect_ray_aabb(ray.origin, ray.direction, box_min, box_max, t_hit))
            {
                constexpr auto min_hit_distance = 0.0001F;
                if (t_hit > min_hit_distance && t_hit < closest_distance)
                {
                    closest_distance = t_hit;
                    hit_entity = self.entity;
                    hit_position = ray.origin + (ray.direction * t_hit);
                    has_hit = true;
                }
            }
        }

        // Test ground plane (Y = 0, normal (0, 1, 0)) if ray points downwards
        constexpr auto min_downward_dir_y = -0.0001F;
        if (ray.direction.y < min_downward_dir_y)
        {
            const auto t_ground = -ray.origin.y / ray.direction.y;
            constexpr auto min_hit_dist = 0.0001F;
            if (t_ground > min_hit_dist && t_ground < closest_distance)
            {
                closest_distance = t_ground;
                hit_position = ray.origin + (ray.direction * t_ground);
                hit_position.y = 0.0F;
                hit_entity = static_cast<ecs::entity>(ecs::tombstone);
                has_hit = true;
            }
        }

        // Fallback: 5.0m in front of camera
        if (!has_hit)
        {
            constexpr auto fallback_distance = 5.0F;
            hit_position = ray.origin + (ray.direction * fallback_distance);
            hit_entity = static_cast<ecs::entity>(ecs::tombstone);
        }

        return placement_result{
            .position = hit_position,
            .hit_entity = hit_entity,
            .has_hit = has_hit,
        };
    }

    auto viewport_window::handle_asset_drop(const asset_payload& payload, math::vec2<float> mouse_pos,
                                            math::vec2<float> vp_size) -> bool
    {
        const auto kind = deduce_asset_kind(payload);
        if (kind == dropped_asset_kind::unknown)
        {
            return false;
        }

        const auto placement = calculate_placement_position(mouse_pos, vp_size);

        if (kind == dropped_asset_kind::prefab)
        {
            auto& scene_mgr = _ctx->get_scene_manager();
            auto target_scene = scene_mgr.get_active_scene();
            if (!scene_mgr.is_valid(target_scene))
            {
                const auto loaded_scenes = scene_mgr.get_loaded_scenes();
                if (!loaded_scenes.empty())
                {
                    target_scene = loaded_scenes.front();
                }
            }

            auto cmd_ctx = command_context{
                .registry = _ctx->get_entities(),
                .scene_manager = scene_mgr,
                .prefab_cache = _resolve_prefab_cache(),
                .events = &_ctx->get_events(),
                .log = &_ctx->get_logger(),
            };

            auto inst_cmd = make_unique<instantiate_prefab_command>(
                payload.asset_guid, target_scene, nullopt, placement.position);

            if (_history != nullptr)
            {
                _history->execute(tempest::move(inst_cmd), cmd_ctx);
            }
            else
            {
                inst_cmd->apply(cmd_ctx);
            }

            return true;
        }

        if (kind == dropped_asset_kind::material)
        {
            if (!placement.has_hit || placement.hit_entity == ecs::tombstone ||
                !_ctx->get_entities().is_valid(placement.hit_entity))
            {
                return false;
            }

            auto cmd_ctx = command_context{
                .registry = _ctx->get_entities(),
                .scene_manager = _ctx->get_scene_manager(),
                .prefab_cache = _resolve_prefab_cache(),
                .events = &_ctx->get_events(),
                .log = &_ctx->get_logger(),
            };

            const auto target_address = compute_entity_address(_ctx->get_entities(), placement.hit_entity);
            auto mat_cmd = make_unique<assign_material_command>(target_address, payload.asset_guid);

            if (_history != nullptr)
            {
                _history->execute(tempest::move(mat_cmd), cmd_ctx);
            }
            else
            {
                mat_cmd->apply(cmd_ctx);
            }

            return true;
        }

        return false;
    }

    auto viewport_window::_render_drop_target(math::vec2<float> child_pos, math::vec2<float> child_size) -> void
    {
        if (child_size.x <= 0.0F || child_size.y <= 0.0F)
        {
            return;
        }

        const auto target_rect = ImRect(ImVec2(child_pos.x, child_pos.y),
                                        ImVec2(child_pos.x + child_size.x, child_pos.y + child_size.y));
        const auto target_id = ImGui::GetID("##ViewportDropTarget");
        if (ImGui::BeginDragDropTargetCustom(target_rect, target_id))
        {
            constexpr auto drop_flags = ImGuiDragDropFlags_AcceptBeforeDelivery;
            const auto* const drop_payload = ImGui::AcceptDragDropPayload(asset_payload_type, drop_flags);
            if (drop_payload != nullptr && drop_payload->Data != nullptr)
            {
                const auto payload =
                    asset_payload::from_raw_payload(drop_payload->Data, static_cast<size_t>(drop_payload->DataSize));
                const auto mouse_screen = ImGui::GetMousePos();
                const auto rel_mouse_pos =
                    math::vec2<float>{mouse_screen.x - child_pos.x, mouse_screen.y - child_pos.y};
                const auto vp_size = math::vec2<float>{child_size.x, child_size.y};

                const auto placement = calculate_placement_position(rel_mouse_pos, vp_size);
                const auto kind = deduce_asset_kind(payload);

                auto* const draw_list = ImGui::GetWindowDrawList();
                if (draw_list != nullptr)
                {
                    constexpr auto color_preview = IM_COL32(50, 180, 255, 230);
                    constexpr auto fill_preview = IM_COL32(50, 180, 255, 60);
                    constexpr auto color_hit = IM_COL32(255, 200, 50, 230);
                    constexpr auto radius = 18.0F;

                    const auto marker_color =
                        (placement.has_hit && placement.hit_entity != ecs::tombstone) ? color_hit : color_preview;
                    draw_list->AddCircleFilled(mouse_screen, radius, fill_preview, 24);
                    draw_list->AddCircle(mouse_screen, radius, marker_color, 24, 2.5F);

                    constexpr auto cross_length = 24.0F;
                    draw_list->AddLine(ImVec2(mouse_screen.x - cross_length, mouse_screen.y),
                                       ImVec2(mouse_screen.x + cross_length, mouse_screen.y),
                                       marker_color, 1.5F);
                    draw_list->AddLine(ImVec2(mouse_screen.x, mouse_screen.y - cross_length),
                                       ImVec2(mouse_screen.x, mouse_screen.y + cross_length),
                                       marker_color, 1.5F);
                }

                ImGui::BeginTooltip();
                const auto asset_name = !payload.path_view().empty() ? payload.path_view() : payload.type_name_view();
                if (kind == dropped_asset_kind::prefab)
                {
                    ImGui::Text("Place Prefab: %.*s", static_cast<int>(asset_name.size()), asset_name.data());
                    ImGui::Text("Position: (%.2f, %.2f, %.2f)", placement.position.x, placement.position.y,
                                placement.position.z);
                    if (placement.has_hit && placement.hit_entity != ecs::tombstone)
                    {
                        const auto name_opt = _ctx->get_entities().name(placement.hit_entity);
                        if (name_opt.has_value())
                        {
                            ImGui::Text("On Entity: %.*s", static_cast<int>(name_opt->size()), name_opt->data());
                        }
                    }
                    else if (placement.has_hit)
                    {
                        ImGui::TextUnformatted("On Surface: Ground (Y = 0)");
                    }
                    else
                    {
                        ImGui::TextUnformatted("In Air (5m fallback)");
                    }
                }
                else if (kind == dropped_asset_kind::material)
                {
                    ImGui::Text("Assign Material: %.*s", static_cast<int>(asset_name.size()), asset_name.data());
                    if (placement.has_hit && placement.hit_entity != ecs::tombstone)
                    {
                        const auto name_opt = _ctx->get_entities().name(placement.hit_entity);
                        if (name_opt.has_value())
                        {
                            ImGui::Text("Target: %.*s", static_cast<int>(name_opt->size()), name_opt->data());
                        }
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "Hover over an entity to assign");
                    }
                }
                ImGui::EndTooltip();

                if (drop_payload->IsDelivery())
                {
                    handle_asset_drop(payload, rel_mouse_pos, vp_size);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    auto viewport_window::draw() -> void
    {
        const auto name = window_name();

        constexpr auto window_padding_y = 2.0F;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, window_padding_y));

        if (ImGui::Begin(name.data(), &_open))
        {
            auto& controller = _ctx->get_play_mode_controller();
            const auto current_state = controller.get_state();

            const auto is_playing = (current_state == simulation_state::playing);
            if (is_playing)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button("Play"))
            {
                controller.play();
            }
            if (is_playing)
            {
                ImGui::EndDisabled();
                ImGui::PopStyleColor();
            }

            ImGui::SameLine();

            const auto is_paused = (current_state == simulation_state::paused);
            const auto can_pause = is_playing;
            if (!can_pause && !is_paused)
            {
                ImGui::BeginDisabled(true);
            }
            if (is_paused)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button("Pause"))
            {
                controller.pause();
            }
            if (is_paused)
            {
                ImGui::EndDisabled();
                ImGui::PopStyleColor();
            }
            else if (!can_pause)
            {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            const auto can_step = is_paused;
            if (!can_step)
            {
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button("Step"))
            {
                controller.step();
            }
            if (!can_step)
            {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            const auto can_stop = (current_state != simulation_state::edit);
            if (!can_stop)
            {
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button("Stop"))
            {
                controller.stop();
            }
            if (!can_stop)
            {
                ImGui::EndDisabled();
            }

            if (_ctx->get_config().headless)
            {
                ImGui::BeginChild("ViewportChild", ImVec2(0, 0), 0,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::TextUnformatted("Headless Viewport (No Renderer)");

                const auto child_pos = ImGui::GetWindowPos();
                const auto child_size = ImGui::GetWindowSize();
                _render_drop_target(math::vec2<float>{child_pos.x, child_pos.y},
                                    math::vec2<float>{child_size.x, child_size.y});

                ImGui::EndChild();
            }
            else
            {
                auto& renderer = _ctx->get_renderer();

                ImGui::BeginChild("ViewportChild", ImVec2(0, 0), 0,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                const auto content_size = ImGui::GetContentRegionAvail();
                const auto new_width = static_cast<uint32_t>(tempest::max(0.0F, content_size.x));
                const auto new_height = static_cast<uint32_t>(tempest::max(0.0F, content_size.y));

                if ((new_width != _viewport_size.x || new_height != _viewport_size.y) && new_width > 0 &&
                    new_height > 0)
                {
                    _viewport_size.x = new_width;
                    _viewport_size.y = new_height;
                    renderer.resize(new_width, new_height);
                }

                auto rg_tex_id = renderer.get_tonemapped_color_texture();
                const auto* alloc = renderer.get_render_graph().get_allocator().get_texture(rg_tex_id.id);
                if (alloc && alloc->sampled_descriptor.index != ~0U && _viewport_size.x > 0 && _viewport_size.y > 0)
                {
                    ui::image(alloc->sampled_descriptor, _viewport_size.x, _viewport_size.y);
                }

                const auto child_pos = ImGui::GetWindowPos();
                const auto child_size = ImGui::GetWindowSize();
                _render_drop_target(math::vec2<float>{child_pos.x, child_pos.y},
                                    math::vec2<float>{child_size.x, child_size.y});

                const auto is_hovered =
                    ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) || ImGui::IsItemHovered();
                const auto is_active = ImGui::IsItemActive();
                auto& io = ImGui::GetIO();
                auto& cam = _ctx->get_editor_camera();

                // Left click in viewport while in play mode to capture cursor
                if (current_state == simulation_state::playing && (is_hovered || is_active) &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    const auto main_win = _ctx->get_main_window();
                    if (main_win.is_valid())
                    {
                        _ctx->get_window_manager().set_cursor_mode(main_win, cursor_mode::disabled);
                    }
                }

                // Right click hold for fly camera navigation (active in edit/pause modes)
                if (current_state != simulation_state::playing && (is_hovered || is_active) &&
                    ImGui::IsMouseDown(ImGuiMouseButton_Right))
                {
                    // Mouse look
                    const auto mouse_delta = io.MouseDelta;
                    if (mouse_delta.x != 0.0F || mouse_delta.y != 0.0F)
                    {
                        constexpr auto sensitivity = 0.003F;
                        cam.rotate(mouse_delta.x * sensitivity, mouse_delta.y * sensitivity);
                    }

                    // Speed modifier
                    auto speed = cam.get_move_speed();
                    if (io.KeyShift)
                    {
                        speed *= 2.5F;
                    }

                    const auto dt = io.DeltaTime > 0.0F ? io.DeltaTime : (1.0F / 60.0F);

                    // WASD / QE navigation
                    auto forward = 0.0F;
                    auto right = 0.0F;
                    auto up = 0.0F;

                    if (ImGui::IsKeyDown(ImGuiKey_W))
                    {
                        forward += speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_S))
                    {
                        forward -= speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_D))
                    {
                        right += speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_A))
                    {
                        right -= speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_E))
                    {
                        up += speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_Q))
                    {
                        up -= speed * dt;
                    }

                    if (forward != 0.0F || right != 0.0F || up != 0.0F)
                    {
                        cam.move(forward, right, up);
                    }

                    // Mouse wheel speed adjustment
                    if (io.MouseWheel != 0.0F)
                    {
                        cam.adjust_move_speed(io.MouseWheel * 0.5F);
                    }
                }

                ImGui::EndChild();
            }
        }

        ImGui::End();

        ImGui::PopStyleVar(1);
    }

    auto viewport_window::aspect_ratio() const -> float
    {
        return _viewport_size.y > 0 ? static_cast<float>(_viewport_size.x) / static_cast<float>(_viewport_size.y)
                                    : 1.0F;
    }
} // namespace tempest::editor
