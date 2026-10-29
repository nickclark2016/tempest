#include <tempest/windows/hierarchy_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/command_history.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/format.hpp>
#include <tempest/logger.hpp>
#include <tempest/material.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/payloads/asset_payload.hpp>
#include <tempest/play_session_diff.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/utility.hpp>

#include <imgui.h>
#include <imgui_internal.h>

namespace tempest::editor
{
    hierarchy_window::hierarchy_window(ecs::archetype_registry& registry, scene::scene_manager& in_scene_mgr,
                                       editor_selection& in_selection, logger& in_logger,
                                       command_history* in_history,
                                       const play_session_diff* in_session_diff,
                                       scene::prefab_cache* in_prefab_cache,
                                       assets::asset_database* in_asset_db)
        : _entity_registry{registry}, _scene_mgr{in_scene_mgr}, _selection{in_selection}, _log{in_logger},
          _history{in_history}, _session_diff{in_session_diff}, _prefab_cache{in_prefab_cache}, _asset_db{in_asset_db}
    {
        if (_prefab_cache == nullptr)
        {
            _owned_prefab_cache = make_unique<scene::prefab_cache>();
            _prefab_cache = _owned_prefab_cache.get();
        }

        _cache.attach_event_registry(_entity_registry->event_registry());

        _entity_destroyed_sub =
            _entity_registry->event_registry().dispatcher<ecs::entity_destroyed_event<ecs::entity>>().subscribe(
                [this](const ecs::entity_destroyed_event<ecs::entity>& evt) {
                    if (_selected_entity == evt.entity)
                    {
                        _selected_entity = default_selected_entity;
                    }
                    if (_anchor_entity == evt.entity)
                    {
                        _anchor_entity = default_selected_entity;
                        _anchor_index = default_anchor_index;
                    }
                    if (_renaming_entity == evt.entity)
                    {
                        _renaming_entity = default_selected_entity;
                    }
                });
    }

    hierarchy_window::~hierarchy_window()
    {
        if (_entity_destroyed_sub != event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>)
        {
            [[maybe_unused]] const auto unsub =
                _entity_registry->event_registry()
                    .dispatcher<ecs::entity_destroyed_event<ecs::entity>>()
                    .unsubscribe(_entity_destroyed_sub);
            _entity_destroyed_sub = {};
        }
    }

    auto hierarchy_window::desired_initial_dock() const -> editor_window::dock_location
    {
        return dock_location::left;
    }

    auto hierarchy_window::window_name() const -> string_view
    {
        return "Hierarchy";
    }

    auto hierarchy_window::_resolve_prefab_cache() noexcept -> scene::prefab_cache*
    {
        return _prefab_cache;
    }

    auto hierarchy_window::_resolve_asset_database() const noexcept -> assets::asset_database*
    {
        if (_asset_db != nullptr)
        {
            return _asset_db;
        }
        return _scene_mgr->get_asset_database();
    }

    auto hierarchy_window::_deduce_asset_kind(const asset_payload& payload) const noexcept -> dropped_asset_kind
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

        const auto* const db = _resolve_asset_database();
        if (db != nullptr)
        {
            const auto* entry = db->find_by_guid(payload.asset_guid);
            if (entry == nullptr && !path.empty())
            {
                entry = db->find_by_path(path);
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

    auto hierarchy_window::handle_asset_drop_on_entity(ecs::entity target_entity, const asset_payload& payload) -> bool
    {
        if (!_entity_registry->is_valid(target_entity))
        {
            return false;
        }

        const auto kind = _deduce_asset_kind(payload);
        if (kind == dropped_asset_kind::prefab)
        {
            auto cmd_ctx = command_context{
                .registry = *_entity_registry,
                .scene_manager = *_scene_mgr,
                .prefab_cache = _resolve_prefab_cache(),
                .events = &_entity_registry->event_registry(),
                .log = _log.get(),
            };

            const auto parent_addr = compute_entity_address(*_entity_registry, target_entity);
            auto inst_cmd = make_unique<instantiate_prefab_command>(payload.asset_guid, parent_addr);
            const auto* const cmd_ptr = inst_cmd.get();
            if (_history != nullptr)
            {
                _history->execute(tempest::move(inst_cmd), cmd_ctx);
            }
            else
            {
                inst_cmd->apply(cmd_ctx);
            }

            if (!cmd_ptr->instantiated_address().id_path.empty())
            {
                const auto created_ent =
                    resolve_entity_address(*_entity_registry, *_scene_mgr, cmd_ptr->instantiated_address());
                if (created_ent != ecs::tombstone && _entity_registry->is_valid(created_ent))
                {
                    _selected_entity = created_ent;
                    _selection->set_selection(cmd_ptr->instantiated_address(), created_ent);
                }
            }

            _cache.mark_dirty();
            return true;
        }
        if (kind == dropped_asset_kind::material)
        {
            auto cmd_ctx = command_context{
                .registry = *_entity_registry,
                .scene_manager = *_scene_mgr,
                .prefab_cache = _resolve_prefab_cache(),
                .events = &_entity_registry->event_registry(),
                .log = _log.get(),
            };

            const auto target_addr = compute_entity_address(*_entity_registry, target_entity);
            auto mat_cmd = make_unique<assign_asset_command>(
                assign_asset_command::for_material(target_addr, payload.asset_guid));
            if (_history != nullptr)
            {
                _history->execute(tempest::move(mat_cmd), cmd_ctx);
            }
            else
            {
                mat_cmd->apply(cmd_ctx);
            }

            _cache.mark_dirty();
            return true;
        }

        return false;
    }

    auto hierarchy_window::handle_asset_drop_on_scene(scene::scene_handle target_scene,
                                                     const asset_payload& payload) -> bool
    {
        const auto kind = _deduce_asset_kind(payload);
        if (kind != dropped_asset_kind::prefab)
        {
            return false;
        }

        auto cmd_ctx = command_context{
            .registry = *_entity_registry,
            .scene_manager = *_scene_mgr,
            .prefab_cache = _resolve_prefab_cache(),
            .events = &_entity_registry->event_registry(),
            .log = _log.get(),
        };

        auto inst_cmd = make_unique<instantiate_prefab_command>(payload.asset_guid, target_scene);
        const auto* const cmd_ptr = inst_cmd.get();
        if (_history != nullptr)
        {
            _history->execute(tempest::move(inst_cmd), cmd_ctx);
        }
        else
        {
            inst_cmd->apply(cmd_ctx);
        }

        if (!cmd_ptr->instantiated_address().id_path.empty())
        {
            const auto created_ent =
                resolve_entity_address(*_entity_registry, *_scene_mgr, cmd_ptr->instantiated_address());
            if (created_ent != ecs::tombstone && _entity_registry->is_valid(created_ent))
            {
                _selected_entity = created_ent;
                _selection->set_selection(cmd_ptr->instantiated_address(), created_ent);
            }
        }

        _cache.mark_dirty();
        return true;
    }

    auto hierarchy_window::handle_asset_drop_on_empty_space(const asset_payload& payload) -> bool
    {
        const auto target_sc = _scene_mgr->get_active_scene();
        return handle_asset_drop_on_scene(target_sc, payload);
    }

    auto hierarchy_window::draw() -> void
    {
        const auto name = window_name();
        const auto name_str = string{name};
        if (!ImGui::Begin(name_str.c_str(), &_open))
        {
            ImGui::End();
            return;
        }

        _handle_shortcuts();

        _cache.rebuild(*_entity_registry, *_scene_mgr, _session_diff);
        const auto visible_rows = _cache.get_visible_rows();

        auto clipper = ImGuiListClipper{};
        clipper.Begin(static_cast<int>(visible_rows.size()));

        while (clipper.Step())
        {
            for (auto display_index = clipper.DisplayStart; display_index < clipper.DisplayEnd; ++display_index)
            {
                _draw_row(static_cast<size_t>(display_index), visible_rows[static_cast<size_t>(display_index)]);
            }
        }
        clipper.End();

        const auto avail = ImGui::GetContentRegionAvail();
        if (avail.y > 0.0F)
        {
            const auto cursor_pos = ImGui::GetCursorScreenPos();
            const auto empty_rect = ImRect(cursor_pos, ImVec2(cursor_pos.x + avail.x, cursor_pos.y + avail.y));
            const auto empty_target_id = ImGui::GetID("##HierarchyEmptyDropTarget");
            if (ImGui::BeginDragDropTargetCustom(empty_rect, empty_target_id))
            {
                if (const auto* const payload = ImGui::AcceptDragDropPayload(asset_payload_type))
                {
                    const auto asset =
                        asset_payload::from_raw_payload(payload->Data, static_cast<size_t>(payload->DataSize));
                    handle_asset_drop_on_empty_space(asset);
                }
                ImGui::EndDragDropTarget();
            }
        }

        // Right click in empty window space
        if (ImGui::BeginPopupContextWindow("hierarchy_window_empty_context",
                                           ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty Entity"))
            {
                const auto active_scene = _scene_mgr->get_active_scene();
                auto created = static_cast<ecs::entity>(ecs::tombstone);
                if (active_scene.is_valid())
                {
                    created = _scene_mgr->create_entity(active_scene, "Entity");
                }
                else
                {
                    created = _entity_registry->create();
                    _entity_registry->name(created, "Entity");
                }

                _selected_entity = created;
                _selection->set_selection(compute_entity_address(*_entity_registry, created), created);
                _cache.mark_dirty();
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    auto hierarchy_window::_draw_row(size_t index, const hierarchy_row& row) -> void
    {
        if (row.type == row_type::entity_node)
        {
            ImGui::PushID("ent");
            ImGui::PushID(static_cast<int>(static_cast<uint64_t>(row.entity) & 0xFFFFFFFFU));
            ImGui::PushID(static_cast<int>(static_cast<uint64_t>(row.entity) >> 32U));
        }
        else if (row.type == row_type::scene_header)
        {
            ImGui::PushID("scene");
            ImGui::PushID(static_cast<int>(row.scene.index));
            ImGui::PushID(static_cast<int>(row.scene.generation));
        }
        else
        {
            ImGui::PushID("dynamic");
            ImGui::PushID(0);
            ImGui::PushID(0);
        }

        const auto indent_px = static_cast<float>(row.depth) * 16.0F;
        if (indent_px > 0.0F)
        {
            ImGui::Indent(indent_px);
        }

        auto is_selected = false;
        if (row.type == row_type::entity_node)
        {
            is_selected = _selection->is_selected(row.entity);
        }

        char label_buffer[256] = {};
        if (row.type == row_type::scene_header)
        {
            if (row.is_active_scene && row.is_dirty)
            {
                tempest::format_to_buffer(label_buffer, "{} (Active) *", row.name);
            }
            else if (row.is_active_scene)
            {
                tempest::format_to_buffer(label_buffer, "{} (Active)", row.name);
            }
            else if (row.is_dirty)
            {
                tempest::format_to_buffer(label_buffer, "{} *", row.name);
            }
            else
            {
                tempest::format_to_buffer(label_buffer, "{}", row.name);
            }
        }
        else if (row.type == row_type::entity_node)
        {
            if (row.is_dynamic && row.is_networked)
            {
                tempest::format_to_buffer(label_buffer, "[~] [net] {}", row.name);
            }
            else if (row.is_dynamic)
            {
                tempest::format_to_buffer(label_buffer, "[~] {}", row.name);
            }
            else if (row.is_networked)
            {
                tempest::format_to_buffer(label_buffer, "[net] {}", row.name);
            }
            else
            {
                tempest::format_to_buffer(label_buffer, "{}", row.name);
            }
        }
        else
        {
            tempest::format_to_buffer(label_buffer, "{}", row.name);
        }

        if (row.type == row_type::entity_node && _renaming_entity == row.entity)
        {
            if (_focus_rename)
            {
                ImGui::SetKeyboardFocusHere();
                _focus_rename = false;
            }
            ImGui::SetNextItemWidth(-1.0F);
            const auto enter_pressed =
                ImGui::InputText("##rename_input", _rename_buffer, sizeof(_rename_buffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const auto escape_pressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false);

            if (enter_pressed || ImGui::IsItemDeactivatedAfterEdit())
            {
                _entity_registry->name(row.entity, string_view{static_cast<const char*>(_rename_buffer)});
                _renaming_entity = default_selected_entity;
                _cache.mark_dirty();
            }
            else if (escape_pressed || (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit()))
            {
                _renaming_entity = default_selected_entity;
            }
        }
        else
        {
            auto node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                              ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (!row.has_children)
            {
                node_flags |= ImGuiTreeNodeFlags_Leaf;
            }
            if (is_selected)
            {
                node_flags |= ImGuiTreeNodeFlags_Selected;
            }
            if (row.type == row_type::scene_header || row.type == row_type::dynamic_header)
            {
                node_flags |= ImGuiTreeNodeFlags_Framed;
            }

            ImGui::SetNextItemOpen(row.is_expanded);

            const auto push_dynamic_color = (row.is_dynamic && row.type == row_type::entity_node);
            const auto push_prefab_color = (row.is_prefab_instance && row.type == row_type::entity_node);

            if (push_dynamic_color)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.82F, 0.35F, 1.0F));
            }
            else if (push_prefab_color)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35F, 0.65F, 1.0F, 1.0F));
            }

            const auto node_open = ImGui::TreeNodeEx("##node", node_flags, "%s", label_buffer);

            if (push_dynamic_color || push_prefab_color)
            {
                ImGui::PopStyleColor();
            }

            if (row.has_children && ImGui::IsItemToggledOpen())
            {
                if (row.type == row_type::scene_header)
                {
                    _cache.set_expanded(row.scene, node_open);
                }
                else if (row.type == row_type::dynamic_header)
                {
                    _cache.set_dynamic_section_expanded(node_open);
                }
                else if (row.type == row_type::entity_node)
                {
                    _cache.set_expanded(row.entity, node_open);
                }
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
            {
                _handle_selection(index, row);
            }

            if (ImGui::BeginPopupContextItem())
            {
                _draw_context_menu(row);
                ImGui::EndPopup();
            }

            if (row.type == row_type::entity_node)
            {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    auto drag_entity = row.entity;
                    ImGui::SetDragDropPayload("TEMPEST_DND_ENTITY", &drag_entity, sizeof(ecs::entity));
                    ImGui::Text("Move %s", row.name.c_str());
                    ImGui::EndDragDropSource();
                }
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const auto* const payload = ImGui::AcceptDragDropPayload("TEMPEST_DND_ENTITY"))
                {
                    if (payload->DataSize == sizeof(ecs::entity))
                    {
                        const auto dropped_entity = *reinterpret_cast<const ecs::entity*>(payload->Data);
                        if (dropped_entity != ecs::tombstone && _entity_registry->is_valid(dropped_entity))
                        {
                            const auto* const old_sec =
                                _entity_registry->try_get<scene::scene_entity_component>(dropped_entity);
                            const auto old_scene = (old_sec != nullptr) ? old_sec->scene : scene::scene_handle{};

                            if (row.type == row_type::entity_node)
                            {
                                auto is_cycle = (row.entity == dropped_entity);
                                auto current_ancestor = row.entity;
                                while (!is_cycle && current_ancestor != ecs::tombstone && _entity_registry->is_valid(current_ancestor))
                                {
                                    const auto* const rel_parent =
                                        _entity_registry->try_get<ecs::relationship_component<ecs::entity>>(current_ancestor);
                                    current_ancestor = (rel_parent != nullptr) ? rel_parent->parent : ecs::tombstone;
                                    if (current_ancestor == dropped_entity)
                                    {
                                        is_cycle = true;
                                        break;
                                    }
                                }

                                if (!is_cycle)
                                {
                                    if (old_scene.is_valid())
                                    {
                                        _scene_mgr->remove_root_entity(old_scene, dropped_entity);
                                    }

                                    _entity_registry->reparent(dropped_entity, row.entity);

                                    if (row.scene.is_valid())
                                    {
                                        auto sync_scene = [&](auto&& self, ecs::entity current_entity) -> void {
                                            _entity_registry->assign_or_replace(
                                                current_entity, scene::scene_entity_component{.scene = row.scene});
                                            const auto* const rel_c =
                                                _entity_registry
                                                    ->try_get<ecs::relationship_component<ecs::entity>>(current_entity);
                                            if (rel_c != nullptr)
                                            {
                                                auto child = rel_c->first_child;
                                                while (child != ecs::tombstone && _entity_registry->is_valid(child))
                                                {
                                                    self(self, child);
                                                    const auto* const child_rel =
                                                        _entity_registry
                                                            ->try_get<ecs::relationship_component<ecs::entity>>(child);
                                                    child = (child_rel != nullptr) ? child_rel->next_sibling
                                                                                   : ecs::tombstone;
                                                }
                                            }
                                        };
                                        sync_scene(sync_scene, dropped_entity);
                                    }
                                    _cache.mark_dirty();
                                }
                            }
                            else if (row.type == row_type::scene_header)
                            {
                                if (old_scene.is_valid())
                                {
                                    _scene_mgr->remove_root_entity(old_scene, dropped_entity);
                                }

                                _entity_registry->reparent(dropped_entity, ecs::tombstone);
                                if (_scene_mgr->is_valid(row.scene))
                                {
                                    _scene_mgr->add_root_entity(row.scene, dropped_entity);
                                }

                                auto sync_scene = [&](auto&& self, ecs::entity current_entity) -> void {
                                    _entity_registry->assign_or_replace(
                                        current_entity, scene::scene_entity_component{.scene = row.scene});
                                    const auto* const rel_c =
                                        _entity_registry->try_get<ecs::relationship_component<ecs::entity>>(current_entity);
                                    if (rel_c != nullptr)
                                    {
                                        auto child = rel_c->first_child;
                                        while (child != ecs::tombstone && _entity_registry->is_valid(child))
                                        {
                                            self(self, child);
                                            const auto* const child_rel =
                                                _entity_registry
                                                    ->try_get<ecs::relationship_component<ecs::entity>>(child);
                                            child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                                        }
                                    }
                                };
                                sync_scene(sync_scene, dropped_entity);
                                _cache.mark_dirty();
                            }
                            else if (row.type == row_type::dynamic_header)
                            {
                                if (old_scene.is_valid())
                                {
                                    _scene_mgr->remove_root_entity(old_scene, dropped_entity);
                                }

                                _entity_registry->reparent(dropped_entity, ecs::tombstone);

                                auto remove_scene = [&](auto&& self, ecs::entity current_entity) -> void {
                                    _entity_registry->remove<scene::scene_entity_component>(current_entity);
                                    const auto* const rel_c =
                                        _entity_registry->try_get<ecs::relationship_component<ecs::entity>>(current_entity);
                                    if (rel_c != nullptr)
                                    {
                                        auto child = rel_c->first_child;
                                        while (child != ecs::tombstone && _entity_registry->is_valid(child))
                                        {
                                            self(self, child);
                                            const auto* const child_rel =
                                                _entity_registry
                                                    ->try_get<ecs::relationship_component<ecs::entity>>(child);
                                            child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                                        }
                                    }
                                };
                                remove_scene(remove_scene, dropped_entity);
                                _cache.mark_dirty();
                            }
                        }
                    }
                }

                if (const auto* const payload = ImGui::AcceptDragDropPayload(asset_payload_type))
                {
                    const auto asset =
                        asset_payload::from_raw_payload(payload->Data, static_cast<size_t>(payload->DataSize));

                    if (row.type == row_type::entity_node)
                    {
                        handle_asset_drop_on_entity(row.entity, asset);
                    }
                    else if (row.type == row_type::scene_header)
                    {
                        handle_asset_drop_on_scene(row.scene, asset);
                    }
                    else if (row.type == row_type::dynamic_header)
                    {
                        handle_asset_drop_on_scene(scene::scene_handle{}, asset);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        if (indent_px > 0.0F)
        {
            ImGui::Unindent(indent_px);
        }

        ImGui::PopID();
        ImGui::PopID();
        ImGui::PopID();
    }

    auto hierarchy_window::_handle_selection(size_t index, const hierarchy_row& row) -> void
    {
        if (row.type != row_type::entity_node)
        {
            return;
        }

        _selected_entity = row.entity;

        const auto ctrl = ImGui::GetIO().KeyCtrl;
        const auto shift = ImGui::GetIO().KeyShift;
        const auto addr = compute_entity_address(*_entity_registry, row.entity);

        if (ctrl)
        {
            _selection->toggle_selection(addr, row.entity);
            _anchor_entity = row.entity;
            _anchor_index = static_cast<int>(index);
        }
        else if (shift && _anchor_index >= 0)
        {
            const auto rows = _cache.get_visible_rows();
            if (!rows.empty())
            {
                auto effective_anchor = _anchor_index;
                if (_anchor_entity != ecs::tombstone && _anchor_entity != ecs::null)
                {
                    for (size_t search_idx = 0; search_idx < rows.size(); ++search_idx)
                    {
                        if (rows[search_idx].entity == _anchor_entity)
                        {
                            effective_anchor = static_cast<int>(search_idx);
                            break;
                        }
                    }
                }

                const auto clamped_anchor = tempest::clamp(effective_anchor, 0, static_cast<int>(rows.size()) - 1);
                const auto clamped_curr = tempest::clamp(static_cast<int>(index), 0, static_cast<int>(rows.size()) - 1);
                const auto start_idx = tempest::min(clamped_anchor, clamped_curr);
                const auto end_idx = tempest::max(clamped_anchor, clamped_curr);

                auto addrs = vector<entity_address>{};
                auto ents = vector<ecs::entity>{};

                for (auto row_index = start_idx; row_index <= end_idx; ++row_index)
                {
                    const auto& candidate = rows[static_cast<size_t>(row_index)];
                    if (candidate.type == row_type::entity_node)
                    {
                        addrs.push_back(compute_entity_address(*_entity_registry, candidate.entity));
                        ents.push_back(candidate.entity);
                    }
                }
                _selection->select_range(addrs, ents);
            }
        }
        else
        {
            _selection->set_selection(addr, row.entity);
            _anchor_entity = row.entity;
            _anchor_index = static_cast<int>(index);
        }
    }

    auto hierarchy_window::_handle_shortcuts() -> void
    {
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        {
            return;
        }

        auto active_ent = _selected_entity;
        if (_selection->get_primary_entity() != ecs::tombstone)
        {
            active_ent = _selection->get_primary_entity();
        }

        if (active_ent == ecs::null || active_ent == ecs::tombstone || !_entity_registry->is_valid(active_ent))
        {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
        {
            _renaming_entity = active_ent;
            _focus_rename = true;
            const auto name_opt = _entity_registry->name(active_ent);
            if (name_opt.has_value())
            {
                const auto copy_len = tempest::min(sizeof(_rename_buffer) - 1, name_opt->size());
                tempest::memcpy(_rename_buffer, name_opt->data(), copy_len);
                _rename_buffer[copy_len] = '\0';
            }
            else
            {
                _rename_buffer[0] = '\0';
            }
        }

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
        {
            const auto dup = _entity_registry->duplicate(active_ent);
            if (dup != ecs::tombstone)
            {
                _selected_entity = dup;
                _selection->set_selection(compute_entity_address(*_entity_registry, dup), dup);
                _cache.mark_dirty();
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            if (!_selection->is_empty())
            {
                const auto selected_ents = _selection->get_selected_entities();
                auto ents_to_delete = vector<ecs::entity>{};
                for (const auto ent : selected_ents)
                {
                    if (ent != ecs::tombstone && _entity_registry->is_valid(ent))
                    {
                        ents_to_delete.push_back(ent);
                    }
                }
                for (const auto ent : ents_to_delete)
                {
                    const auto* const sec = _entity_registry->try_get<scene::scene_entity_component>(ent);
                    if (sec != nullptr && sec->scene.is_valid())
                    {
                        _scene_mgr->remove_root_entity(sec->scene, ent);
                    }
                    _entity_registry->destroy_recursive(ent);
                }
                _selection->clear_selection();
                _selected_entity = ecs::null;
                _cache.mark_dirty();
            }
            else if (active_ent != ecs::null && active_ent != ecs::tombstone && _entity_registry->is_valid(active_ent))
            {
                const auto* const sec = _entity_registry->try_get<scene::scene_entity_component>(active_ent);
                if (sec != nullptr && sec->scene.is_valid())
                {
                    _scene_mgr->remove_root_entity(sec->scene, active_ent);
                }
                _entity_registry->destroy_recursive(active_ent);
                if (_selected_entity == active_ent)
                {
                    _selected_entity = ecs::null;
                }
                _cache.mark_dirty();
            }
        }
    }

    auto hierarchy_window::_draw_context_menu(const hierarchy_row& row) -> void
    {
        if (row.type == row_type::scene_header)
        {
            if (ImGui::MenuItem("Set Active Scene"))
            {
                _scene_mgr->set_active_scene(row.scene);
            }
            if (ImGui::MenuItem("Save Scene"))
            {
                const auto path_opt = _scene_mgr->get_scene_path(row.scene);
                if (path_opt.has_value())
                {
                    [[maybe_unused]] const auto save_res =
                        _scene_mgr->save_sync(row.scene, filesystem::path(string(*path_opt)));
                }
            }
            if (ImGui::MenuItem("Create Entity"))
            {
                const auto created = _scene_mgr->create_entity(row.scene, "New Entity");
                _selected_entity = created;
                _selection->set_selection(compute_entity_address(*_entity_registry, created), created);
                _cache.mark_dirty();
            }
            if (ImGui::MenuItem("Unload Scene"))
            {
                _scene_mgr->unload(row.scene);
                _cache.mark_dirty();
            }
        }
        else if (row.type == row_type::entity_node)
        {
            if (ImGui::MenuItem("Create Empty"))
            {
                auto created = static_cast<ecs::entity>(ecs::tombstone);
                if (row.scene.is_valid())
                {
                    created = _scene_mgr->create_entity(row.scene, "Entity");
                }
                else
                {
                    created = _entity_registry->create();
                    _entity_registry->name(created, "Entity");
                }
                _selected_entity = created;
                _selection->set_selection(compute_entity_address(*_entity_registry, created), created);
                _cache.mark_dirty();
            }
            if (ImGui::MenuItem("Create Child"))
            {
                auto child = static_cast<ecs::entity>(ecs::tombstone);
                if (row.scene.is_valid())
                {
                    child = _scene_mgr->create_entity(row.scene, "Child");
                    _scene_mgr->remove_root_entity(row.scene, child);
                }
                else
                {
                    child = _entity_registry->create();
                    _entity_registry->name(child, "Child");
                }
                _entity_registry->reparent(child, row.entity);
                _cache.set_expanded(row.entity, true);
                _selected_entity = child;
                _selection->set_selection(compute_entity_address(*_entity_registry, child), child);
                _cache.mark_dirty();
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
            {
                const auto dup = _entity_registry->duplicate(row.entity);
                if (dup != ecs::tombstone)
                {
                    _selected_entity = dup;
                    _selection->set_selection(compute_entity_address(*_entity_registry, dup), dup);
                    _cache.mark_dirty();
                }
            }
            if (ImGui::MenuItem("Rename", "F2"))
            {
                _renaming_entity = row.entity;
                _focus_rename = true;
                const auto name_opt = _entity_registry->name(row.entity);
                if (name_opt.has_value())
                {
                    const auto copy_len = tempest::min(sizeof(_rename_buffer) - 1, name_opt->size());
                    tempest::memcpy(_rename_buffer, name_opt->data(), copy_len);
                    _rename_buffer[copy_len] = '\0';
                }
                else
                {
                    _rename_buffer[0] = '\0';
                }
            }
            if (ImGui::MenuItem("Delete", "Delete"))
            {
                if (row.scene.is_valid())
                {
                    _scene_mgr->remove_root_entity(row.scene, row.entity);
                }
                _entity_registry->destroy_recursive(row.entity);
                _selection->remove_from_selection(compute_entity_address(*_entity_registry, row.entity));
                if (_selected_entity == row.entity)
                {
                    _selected_entity = ecs::null;
                }
                _cache.mark_dirty();
            }
        }
    }
} // namespace tempest::editor
