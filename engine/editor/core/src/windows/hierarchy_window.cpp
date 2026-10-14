#include <tempest/windows/hierarchy_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/format.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/utility.hpp>

#include <imgui.h>

namespace tempest::editor
{
    hierarchy_window::hierarchy_window(ecs::archetype_registry& registry, scene::scene_manager* in_scene_mgr,
                                       editor_selection* in_selection, command_history* in_history)
        : entity_registry{&registry}, scene_mgr{in_scene_mgr}, selection{in_selection}, history{in_history}
    {
        if (scene_mgr == nullptr)
        {
            _owned_scene_mgr = make_unique<scene::scene_manager>(registry);
            scene_mgr = _owned_scene_mgr.get();
        }

        _cache.attach_event_registry(registry.event_registry());

        _entity_destroyed_sub =
            registry.event_registry().dispatcher<ecs::entity_destroyed_event<ecs::entity>>().subscribe(
                [this](const ecs::entity_destroyed_event<ecs::entity>& evt) {
                    if (selected_entity == evt.entity)
                    {
                        selected_entity = default_selected_entity;
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
        if (entity_registry != nullptr &&
            _entity_destroyed_sub != event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>)
        {
            [[maybe_unused]] const auto unsub =
                entity_registry->event_registry()
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

    auto hierarchy_window::draw() -> void
    {
        const auto name = window_name();
        const auto name_str = string{name};
        if (!ImGui::Begin(name_str.c_str(), &open))
        {
            ImGui::End();
            return;
        }

        if (entity_registry == nullptr || scene_mgr == nullptr)
        {
            ImGui::End();
            return;
        }

        _handle_shortcuts();

        _cache.rebuild(*entity_registry, *scene_mgr);
        const auto visible_rows = _cache.get_visible_rows();

        auto clipper = ImGuiListClipper{};
        clipper.Begin(static_cast<int>(visible_rows.size()));

        while (clipper.Step())
        {
            for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                _draw_row(static_cast<size_t>(i), visible_rows[static_cast<size_t>(i)]);
            }
        }
        clipper.End();

        // Right click in empty window space
        if (ImGui::BeginPopupContextWindow("hierarchy_window_empty_context",
                                           ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty Entity"))
            {
                const auto active_scene = scene_mgr->get_active_scene();
                auto created = static_cast<ecs::entity>(ecs::tombstone);
                if (active_scene.is_valid())
                {
                    created = scene_mgr->create_entity(active_scene, "Entity");
                }
                else
                {
                    created = entity_registry->create();
                    entity_registry->name(created, "Entity");
                }

                selected_entity = created;
                if (selection != nullptr)
                {
                    selection->set_selection(compute_entity_address(*entity_registry, created), created);
                }
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
            ImGui::PushID(static_cast<int>(static_cast<uint32_t>(row.entity)));
        }
        else if (row.type == row_type::scene_header)
        {
            ImGui::PushID(static_cast<int>(row.scene.index));
        }
        else
        {
            ImGui::PushID(-1);
        }

        const auto indent_px = static_cast<float>(row.depth) * 16.0F;
        if (indent_px > 0.0F)
        {
            ImGui::Indent(indent_px);
        }

        auto is_selected = false;
        if (row.type == row_type::entity_node)
        {
            if (selection != nullptr)
            {
                is_selected = selection->is_selected(row.entity);
            }
            else
            {
                is_selected = (selected_entity == row.entity);
            }
        }

        char label_buffer[256]{};
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
                entity_registry->name(row.entity, string_view{static_cast<const char*>(_rename_buffer)});
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

            const auto push_color = (row.is_prefab_instance && row.type == row_type::entity_node);
            if (push_color)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35F, 0.65F, 1.0F, 1.0F));
            }

            const auto node_open = ImGui::TreeNodeEx("##node", node_flags, "%s", label_buffer);

            if (push_color)
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
                        if (dropped_entity != ecs::tombstone && entity_registry->is_valid(dropped_entity))
                        {
                            const auto* const old_sec =
                                entity_registry->try_get<scene::scene_entity_component>(dropped_entity);
                            const auto old_scene = (old_sec != nullptr) ? old_sec->scene : scene::scene_handle{};

                            if (row.type == row_type::entity_node)
                            {
                                auto is_cycle = (row.entity == dropped_entity);
                                auto curr = row.entity;
                                while (!is_cycle && curr != ecs::tombstone && entity_registry->is_valid(curr))
                                {
                                    const auto* const r =
                                        entity_registry->try_get<ecs::relationship_component<ecs::entity>>(curr);
                                    curr = (r != nullptr) ? r->parent : ecs::tombstone;
                                    if (curr == dropped_entity)
                                    {
                                        is_cycle = true;
                                        break;
                                    }
                                }

                                if (!is_cycle)
                                {
                                    if (scene_mgr != nullptr && old_scene.is_valid())
                                    {
                                        scene_mgr->remove_root_entity(old_scene, dropped_entity);
                                    }

                                    entity_registry->reparent(dropped_entity, row.entity);

                                    if (row.scene.is_valid())
                                    {
                                        auto sync_scene = [&](auto&& self, ecs::entity e) -> void {
                                            entity_registry->assign_or_replace(
                                                e, scene::scene_entity_component{.scene = row.scene});
                                            const auto* const rel_c =
                                                entity_registry
                                                    ->try_get<ecs::relationship_component<ecs::entity>>(e);
                                            if (rel_c != nullptr)
                                            {
                                                auto child = rel_c->first_child;
                                                while (child != ecs::tombstone && entity_registry->is_valid(child))
                                                {
                                                    self(self, child);
                                                    const auto* const child_rel =
                                                        entity_registry
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
                                if (scene_mgr != nullptr && old_scene.is_valid())
                                {
                                    scene_mgr->remove_root_entity(old_scene, dropped_entity);
                                }

                                entity_registry->reparent(dropped_entity, ecs::tombstone);
                                if (scene_mgr != nullptr && scene_mgr->is_valid(row.scene))
                                {
                                    scene_mgr->add_root_entity(row.scene, dropped_entity);
                                }

                                auto sync_scene = [&](auto&& self, ecs::entity e) -> void {
                                    entity_registry->assign_or_replace(
                                        e, scene::scene_entity_component{.scene = row.scene});
                                    const auto* const rel_c =
                                        entity_registry->try_get<ecs::relationship_component<ecs::entity>>(e);
                                    if (rel_c != nullptr)
                                    {
                                        auto child = rel_c->first_child;
                                        while (child != ecs::tombstone && entity_registry->is_valid(child))
                                        {
                                            self(self, child);
                                            const auto* const child_rel =
                                                entity_registry
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
                                if (scene_mgr != nullptr && old_scene.is_valid())
                                {
                                    scene_mgr->remove_root_entity(old_scene, dropped_entity);
                                }

                                entity_registry->reparent(dropped_entity, ecs::tombstone);

                                auto remove_scene = [&](auto&& self, ecs::entity e) -> void {
                                    entity_registry->remove<scene::scene_entity_component>(e);
                                    const auto* const rel_c =
                                        entity_registry->try_get<ecs::relationship_component<ecs::entity>>(e);
                                    if (rel_c != nullptr)
                                    {
                                        auto child = rel_c->first_child;
                                        while (child != ecs::tombstone && entity_registry->is_valid(child))
                                        {
                                            self(self, child);
                                            const auto* const child_rel =
                                                entity_registry
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
                ImGui::EndDragDropTarget();
            }
        }

        if (indent_px > 0.0F)
        {
            ImGui::Unindent(indent_px);
        }

        ImGui::PopID();
    }

    auto hierarchy_window::_handle_selection(size_t index, const hierarchy_row& row) -> void
    {
        if (row.type != row_type::entity_node)
        {
            return;
        }

        selected_entity = row.entity;

        if (selection == nullptr)
        {
            return;
        }

        const auto ctrl = ImGui::GetIO().KeyCtrl;
        const auto shift = ImGui::GetIO().KeyShift;
        const auto addr = compute_entity_address(*entity_registry, row.entity);

        if (ctrl)
        {
            selection->toggle_selection(addr, row.entity);
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
                    for (size_t r = 0; r < rows.size(); ++r)
                    {
                        if (rows[r].entity == _anchor_entity)
                        {
                            effective_anchor = static_cast<int>(r);
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

                for (auto r = start_idx; r <= end_idx; ++r)
                {
                    const auto& candidate = rows[static_cast<size_t>(r)];
                    if (candidate.type == row_type::entity_node)
                    {
                        addrs.push_back(compute_entity_address(*entity_registry, candidate.entity));
                        ents.push_back(candidate.entity);
                    }
                }
                selection->select_range(addrs, ents);
            }
        }
        else
        {
            selection->set_selection(addr, row.entity);
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

        auto active_ent = selected_entity;
        if (selection != nullptr && selection->get_primary_entity() != ecs::tombstone)
        {
            active_ent = selection->get_primary_entity();
        }

        if (active_ent == ecs::null || active_ent == ecs::tombstone || !entity_registry->is_valid(active_ent))
        {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
        {
            _renaming_entity = active_ent;
            _focus_rename = true;
            const auto name_opt = entity_registry->name(active_ent);
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
            const auto dup = entity_registry->duplicate(active_ent);
            if (dup != ecs::tombstone)
            {
                selected_entity = dup;
                if (selection != nullptr)
                {
                    selection->set_selection(compute_entity_address(*entity_registry, dup), dup);
                }
                _cache.mark_dirty();
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            if (selection != nullptr && !selection->is_empty())
            {
                const auto selected_ents = selection->get_selected_entities();
                auto ents_to_delete = vector<ecs::entity>{};
                for (const auto ent : selected_ents)
                {
                    if (ent != ecs::tombstone && entity_registry->is_valid(ent))
                    {
                        ents_to_delete.push_back(ent);
                    }
                }
                for (const auto ent : ents_to_delete)
                {
                    const auto* const sec = entity_registry->try_get<scene::scene_entity_component>(ent);
                    if (sec != nullptr && sec->scene.is_valid() && scene_mgr != nullptr)
                    {
                        scene_mgr->remove_root_entity(sec->scene, ent);
                    }
                    entity_registry->destroy_recursive(ent);
                }
                selection->clear_selection();
                selected_entity = ecs::null;
                _cache.mark_dirty();
            }
            else if (active_ent != ecs::null && active_ent != ecs::tombstone && entity_registry->is_valid(active_ent))
            {
                const auto* const sec = entity_registry->try_get<scene::scene_entity_component>(active_ent);
                if (sec != nullptr && sec->scene.is_valid() && scene_mgr != nullptr)
                {
                    scene_mgr->remove_root_entity(sec->scene, active_ent);
                }
                entity_registry->destroy_recursive(active_ent);
                if (selected_entity == active_ent)
                {
                    selected_entity = ecs::null;
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
                scene_mgr->set_active_scene(row.scene);
            }
            if (ImGui::MenuItem("Save Scene"))
            {
                const auto path_opt = scene_mgr->get_scene_path(row.scene);
                if (path_opt.has_value())
                {
                    [[maybe_unused]] const auto save_res =
                        scene_mgr->save_sync(row.scene, filesystem::path(string(*path_opt)));
                }
            }
            if (ImGui::MenuItem("Create Entity"))
            {
                const auto created = scene_mgr->create_entity(row.scene, "New Entity");
                selected_entity = created;
                if (selection != nullptr)
                {
                    selection->set_selection(compute_entity_address(*entity_registry, created), created);
                }
                _cache.mark_dirty();
            }
            if (ImGui::MenuItem("Unload Scene"))
            {
                scene_mgr->unload(row.scene);
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
                    created = scene_mgr->create_entity(row.scene, "Entity");
                }
                else
                {
                    created = entity_registry->create();
                    entity_registry->name(created, "Entity");
                }
                selected_entity = created;
                if (selection != nullptr)
                {
                    selection->set_selection(compute_entity_address(*entity_registry, created), created);
                }
                _cache.mark_dirty();
            }
            if (ImGui::MenuItem("Create Child"))
            {
                auto child = static_cast<ecs::entity>(ecs::tombstone);
                if (row.scene.is_valid())
                {
                    child = scene_mgr->create_entity(row.scene, "Child");
                    scene_mgr->remove_root_entity(row.scene, child);
                }
                else
                {
                    child = entity_registry->create();
                    entity_registry->name(child, "Child");
                }
                entity_registry->reparent(child, row.entity);
                _cache.set_expanded(row.entity, true);
                selected_entity = child;
                if (selection != nullptr)
                {
                    selection->set_selection(compute_entity_address(*entity_registry, child), child);
                }
                _cache.mark_dirty();
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
            {
                const auto dup = entity_registry->duplicate(row.entity);
                if (dup != ecs::tombstone)
                {
                    selected_entity = dup;
                    if (selection != nullptr)
                    {
                        selection->set_selection(compute_entity_address(*entity_registry, dup), dup);
                    }
                    _cache.mark_dirty();
                }
            }
            if (ImGui::MenuItem("Rename", "F2"))
            {
                _renaming_entity = row.entity;
                _focus_rename = true;
                const auto name_opt = entity_registry->name(row.entity);
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
                if (row.scene.is_valid() && scene_mgr != nullptr)
                {
                    scene_mgr->remove_root_entity(row.scene, row.entity);
                }
                entity_registry->destroy_recursive(row.entity);
                if (selection != nullptr)
                {
                    selection->remove_from_selection(compute_entity_address(*entity_registry, row.entity));
                }
                if (selected_entity == row.entity)
                {
                    selected_entity = ecs::null;
                }
                _cache.mark_dirty();
            }
        }
    }
} // namespace tempest::editor
