#include <tempest/windows/inspector_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/dynamic_entity_id_component.hpp>
#include <tempest/format.hpp>
#include <tempest/iterator.hpp>
#include <tempest/logger.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_operations.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/string.hpp>
#include <tempest/ui.hpp>

#include <imgui.h>

namespace tempest::editor
{
    namespace
    {
        auto find_prefab_instance_root(const ecs::basic_archetype_registry& registry, ecs::entity ent) -> ecs::entity
        {
            auto curr = ent;
            while (curr != ecs::tombstone && curr != ecs::null && registry.is_valid(curr))
            {
                if (registry.has<scene::prefab_instance_component>(curr))
                {
                    return curr;
                }
                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(curr);
                curr = (rel != nullptr) ? rel->parent : ecs::tombstone;
            }
            return ecs::tombstone;
        }

        auto compute_prefab_id_path(const ecs::basic_archetype_registry& registry, ecs::entity root_ent,
                                    ecs::entity target_ent) -> scene::id_path
        {
            if (root_ent == target_ent)
            {
                if (const auto* pmc = registry.try_get<scene::prefab_member_component>(root_ent); pmc != nullptr)
                {
                    auto path = scene::id_path{};
                    path.push_back(pmc->template_local_id);
                    return path;
                }
                if (const auto* sec = registry.try_get<scene::scene_entity_component>(root_ent); sec != nullptr)
                {
                    auto path = scene::id_path{};
                    path.push_back(sec->local_id);
                    return path;
                }
                return scene::id_path{};
            }

            auto chain = vector<scene::entity_local_id>{};
            auto curr = target_ent;
            while (curr != ecs::tombstone && curr != root_ent && registry.is_valid(curr))
            {
                if (const auto* pmc = registry.try_get<scene::prefab_member_component>(curr); pmc != nullptr)
                {
                    chain.push_back(pmc->template_local_id);
                }
                else if (const auto* sec = registry.try_get<scene::scene_entity_component>(curr); sec != nullptr)
                {
                    chain.push_back(sec->local_id);
                }
                else if (const auto* dyn = registry.try_get<dynamic_entity_id_component>(curr); dyn != nullptr)
                {
                    chain.push_back(dyn->local_id);
                }
                else
                {
                    chain.push_back(scene::entity_local_id{static_cast<uint64_t>(curr)});
                }

                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(curr);
                curr = (rel != nullptr) ? rel->parent : ecs::tombstone;
            }

            auto path = scene::id_path{};
            for (auto i = chain.size(); i > 0; --i)
            {
                path.push_back(chain[i - 1]);
            }
            return path;
        }

        auto get_display_component_name(string_view type_name) -> string
        {
            auto last_colons = size_t{0};
            auto found_colons = false;
            if (type_name.size() >= 2)
            {
                for (auto i = type_name.size() - 2;; --i)
                {
                    if (type_name[i] == ':' && type_name[i + 1] == ':')
                    {
                        last_colons = i + 2;
                        found_colons = true;
                        break;
                    }
                    if (i == 0)
                    {
                        break;
                    }
                }
            }

            auto result = string{};
            auto capitalize_next = true;
            for (auto i = (found_colons ? last_colons : 0); i < type_name.size(); ++i)
            {
                const auto ch = type_name[i];
                if (ch == '_')
                {
                    result.push_back(' ');
                    capitalize_next = true;
                }
                else if (capitalize_next)
                {
                    const auto upper = (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 'a' + 'A') : ch;
                    result.push_back(upper);
                    capitalize_next = false;
                }
                else
                {
                    result.push_back(ch);
                }
            }
            return result;
        }
    } // namespace

    inspector_window::inspector_window(ecs::archetype_registry& registry, scene::scene_manager* in_scene_mgr,
                                       editor_selection* in_selection, command_history* in_history,
                                       scene::prefab_cache* in_prefab_cache, event::event_registry* in_events,
                                       logger* in_logger)
        : entity_registry{&registry}, scene_mgr{in_scene_mgr}, selection{in_selection}, history{in_history},
          prefab_cache{in_prefab_cache}, events{in_events}, log{in_logger}
    {
        if (scene_mgr == nullptr)
        {
            _owned_scene_mgr = make_unique<scene::scene_manager>(registry);
            scene_mgr = _owned_scene_mgr.get();
        }
    }

    inspector_window::~inspector_window()
    {
        if (history != nullptr && history->is_in_gesture())
        {
            history->end_gesture();
        }
    }

    auto inspector_window::desired_initial_dock() const -> editor_window::dock_location
    {
        return editor_window::dock_location::right;
    }

    auto inspector_window::window_name() const -> string_view
    {
        return "Inspector";
    }

    auto inspector_window::draw() -> void
    {
        const auto active_ent = (selection != nullptr) ? selection->get_primary_entity() : target;
        if (active_ent != _last_inspected_entity || active_ent == ecs::tombstone || active_ent == ecs::null)
        {
            if (history != nullptr && history->is_in_gesture())
            {
                history->end_gesture();
            }
            _last_inspected_entity = active_ent;
        }

        if (!open)
        {
            return;
        }

        const auto name = window_name();
        if (ImGui::Begin(name.data(), &open))
        {
            if (active_ent == ecs::tombstone || active_ent == ecs::null || entity_registry == nullptr ||
                !entity_registry->is_valid(active_ent))
            {
                ImGui::TextDisabled("No entity selected");
                ImGui::End();
                return;
            }

            auto cmd_ctx = command_context{
                .registry = *entity_registry,
                .scene_manager = *scene_mgr,
                .prefab_cache = prefab_cache,
                .events = events,
                .log = log,
            };

            const auto addr = compute_entity_address(*entity_registry, active_ent);

            _draw_entity_header(active_ent, addr, cmd_ctx);
            _draw_component_cards(active_ent, addr, cmd_ctx);
            _draw_unknown_components(active_ent, addr, cmd_ctx);
            _draw_add_component_button(active_ent, addr, cmd_ctx);
        }
        ImGui::End();
    }

    auto inspector_window::_draw_entity_header(ecs::entity ent, const entity_address& addr, command_context& cmd_ctx)
        -> void
    {
        const auto name_opt = entity_registry->name(ent);
        char name_buf[256]{};
        if (name_opt.has_value())
        {
            const auto copy_len = tempest::min(name_opt->size(), sizeof(name_buf) - 1);
            tempest::memcpy(name_buf, name_opt->data(), copy_len);
            name_buf[copy_len] = '\0';
        }

        ImGui::PushItemWidth(-1.0F);
        ImGui::InputTextWithHint("##EntityName", "Unnamed Entity", name_buf, sizeof(name_buf));
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            const auto new_name = string_view{static_cast<const char*>(name_buf)};
            if (history != nullptr)
            {
                history->execute(make_unique<rename_entity_command>(addr, new_name), cmd_ctx);
            }
            else
            {
                entity_registry->name(ent, new_name);
            }
        }
        ImGui::PopItemWidth();

        ImGui::TextDisabled("Entity ID: %u", static_cast<uint32_t>(ent));
        if (addr.scene.is_valid())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("| Scene: #%u", addr.scene.index);
        }
        ImGui::Separator();
        ImGui::Spacing();
    }

    auto inspector_window::_draw_component_cards(ecs::entity ent, const entity_address& addr, command_context& cmd_ctx)
        -> void
    {
        const auto root_ent = find_prefab_instance_root(*entity_registry, ent);
        const auto is_prefab_member = (root_ent != ecs::tombstone && entity_registry->is_valid(root_ent));
        const auto* prefab_inst =
            is_prefab_member ? entity_registry->try_get<scene::prefab_instance_component>(root_ent) : nullptr;
        const auto prefab_target_path =
            is_prefab_member ? compute_prefab_id_path(*entity_registry, root_ent, ent) : scene::id_path{};

        _cached_comp_types = ecs::raw_registry_access::get_component_types(*entity_registry, ent);

        for (const auto& comp_info : _cached_comp_types)
        {
            if (comp_info.name == "tempest::ecs::self_component" ||
                tempest::starts_with(comp_info.name, "tempest::ecs::relationship_component") ||
                comp_info.name == "tempest::scene::scene_entity_component" ||
                comp_info.name == "tempest::editor::dynamic_entity_id_component" ||
                comp_info.name == "tempest::scene::prefab_member_component" ||
                comp_info.name == "tempest::scene::prefab_instance_component")
            {
                continue;
            }

            const auto display_title = get_display_component_name(comp_info.name);
            char header_id[256]{};
            format_to_buffer(header_id, "{}##card_{}", display_title, comp_info.index);

            auto comp_has_override = false;
            if (prefab_inst != nullptr)
            {
                for (const auto& ov : prefab_inst->overrides.field_overrides)
                {
                    if (ov.target == prefab_target_path && ov.component_name == comp_info.name)
                    {
                        comp_has_override = true;
                        break;
                    }
                }
            }

            ImGui::PushID(static_cast<int>(comp_info.index));
            const auto is_open =
                ImGui::CollapsingHeader(header_id, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);

            char menu_id[64]{};
            format_to_buffer(menu_id, "##comp_menu_{}", comp_info.index);
            if (ImGui::BeginPopupContextItem(menu_id))
            {
                if (ImGui::MenuItem("Reset to Default"))
                {
                    if (history != nullptr)
                    {
                        history->execute(make_unique<reset_component_command>(addr, comp_info.index), cmd_ctx);
                    }
                    else
                    {
                        auto cmd = reset_component_command{addr, comp_info.index};
                        cmd.apply(cmd_ctx);
                    }
                }
                if (ImGui::MenuItem("Remove Component"))
                {
                    if (history != nullptr)
                    {
                        history->execute(make_unique<remove_component_command>(addr, comp_info.index), cmd_ctx);
                    }
                    else
                    {
                        ecs::raw_registry_access::remove_raw(*entity_registry, ent, comp_info.index);
                    }
                }
                if (is_prefab_member && comp_has_override)
                {
                    ImGui::Separator();
                    if (ImGui::MenuItem("Revert Override"))
                    {
                        const auto root_addr = compute_entity_address(*entity_registry, root_ent);
                        auto matching_fields = vector<string>{};
                        for (const auto& ov : prefab_inst->overrides.field_overrides)
                        {
                            if (ov.target == prefab_target_path && ov.component_name == comp_info.name)
                            {
                                matching_fields.push_back(ov.field_name);
                            }
                        }
                        for (const auto& field_name : matching_fields)
                        {
                            if (history != nullptr)
                            {
                                history->execute(make_unique<revert_override_command>(
                                                     root_addr, prefab_target_path, comp_info.name, field_name),
                                                 cmd_ctx);
                            }
                            else
                            {
                                auto cmd = revert_override_command{root_addr, prefab_target_path, comp_info.name,
                                                                   field_name};
                                cmd.apply(cmd_ctx);
                            }
                        }
                    }
                    if (ImGui::MenuItem("Apply to Prefab"))
                    {
                        const auto root_addr = compute_entity_address(*entity_registry, root_ent);
                        if (history != nullptr && prefab_cache != nullptr)
                        {
                            history->execute(make_unique<apply_override_command>(root_addr, string_view{}),
                                             cmd_ctx);
                        }
                        else if (prefab_cache != nullptr)
                        {
                            auto cmd = apply_override_command{root_addr, string_view{}};
                            cmd.apply(cmd_ctx);
                        }
                    }
                }
                ImGui::EndPopup();
            }

            if (is_open)
            {
                auto* comp_ptr = ecs::raw_registry_access::get_component_raw(*entity_registry, ent, comp_info.index);
                if (comp_info.descriptor.has_value() && comp_ptr != nullptr)
                {
                    auto grid_ctx = property_grid_context{
                        .registry = *entity_registry,
                        .entity = ent,
                        .address = addr,
                        .history = history,
                        .cmd_ctx = &cmd_ctx,
                        .prefab_instance = prefab_inst,
                        .prefab_target_path = prefab_target_path,
                        .asset_db = scene_mgr != nullptr ? scene_mgr->get_asset_database() : nullptr,
                        .search_filter = &_asset_search_filter,
                    };
                    property_grid::draw_component(grid_ctx, *comp_info.descriptor, comp_ptr);
                }
            }

            ImGui::PopID();
            ImGui::Spacing();
        }
    }

    auto inspector_window::_draw_unknown_components(ecs::entity ent, const entity_address& addr,
                                                    command_context& cmd_ctx) -> void
    {
        if (scene_mgr == nullptr || !addr.scene.is_valid())
        {
            return;
        }

        const auto* unrec_data = scene_mgr->get_unrecognized_data(addr.scene);
        if (unrec_data == nullptr)
        {
            return;
        }

        const auto* ent_comp = entity_registry->try_get<scene::scene_entity_component>(ent);
        if (ent_comp == nullptr)
        {
            return;
        }

        const auto iter = unrec_data->entities.find(ent_comp->local_id.value);
        if (iter == unrec_data->entities.end())
        {
            return;
        }

        for (const auto& unk : iter->second.unknown_components)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.7F, 0.15F, 0.15F, 1.0F));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.85F, 0.25F, 0.25F, 1.0F));
            char header_label[256]{};
            format_to_buffer(header_label, "Unknown: {}##unk_{}", unk.name, unk.name);
            const auto header_open =
                ImGui::CollapsingHeader(header_label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);

            char popup_unk_id[256]{};
            format_to_buffer(popup_unk_id, "##unk_menu_{}", unk.name);
            if (ImGui::BeginPopupContextItem(popup_unk_id))
            {
                if (ImGui::MenuItem("Remove Unknown Component"))
                {
                    if (history != nullptr)
                    {
                        history->execute(make_unique<remove_unknown_component_command>(
                                             addr.scene, ent_comp->local_id.value, unk.name),
                                         cmd_ctx);
                    }
                    else
                    {
                        auto cmd = remove_unknown_component_command{addr.scene, ent_comp->local_id.value, unk.name};
                        cmd.apply(cmd_ctx);
                    }
                }
                if (ImGui::BeginMenu("Map to Component..."))
                {
                    for (uint32_t t = 0; t < entity_registry->type_registry().size(); ++t)
                    {
                        const auto& t_info = entity_registry->type_registry().type_info(t);
                        if (!t_info.is_serializable)
                        {
                            continue;
                        }
                        char tname_buf[256]{};
                        const auto copy_len = tempest::min(t_info.name.size(), sizeof(tname_buf) - 1);
                        tempest::memcpy(tname_buf, t_info.name.data(), copy_len);
                        tname_buf[copy_len] = '\0';
                        if (ImGui::MenuItem(tname_buf))
                        {
                            if (history != nullptr)
                            {
                                history->execute(make_unique<map_unknown_component_command>(addr, unk.name, t),
                                                 cmd_ctx);
                            }
                            else
                            {
                                auto cmd = map_unknown_component_command{addr, unk.name, t};
                                cmd.apply(cmd_ctx);
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            if (header_open)
            {
                ImGui::TextWrapped("JSON Payload:\n%s", unk.json_payload.c_str());
            }

            ImGui::PopStyleColor(2);
            ImGui::Spacing();
        }
    }

    auto inspector_window::_draw_add_component_button(ecs::entity ent, const entity_address& addr,
                                                      command_context& cmd_ctx) -> void
    {
        ImGui::Spacing();
        ImGui::Separator();
        if (ui::centered_button("Add Component"))
        {
            ImGui::OpenPopup("AddComponentPopup");
            _component_search_filter.Clear();
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            _component_search_filter.Draw("##Search", -1.0F);
            if (ImGui::BeginChild("##CompList", ImVec2(280.0F, 250.0F)))
            {
                for (uint32_t i = 0; i < entity_registry->type_registry().size(); ++i)
                {
                    const auto& c_info = entity_registry->type_registry().type_info(i);
                    if (!c_info.is_serializable)
                    {
                        continue;
                    }
                    if (ecs::raw_registry_access::has(*entity_registry, ent, i))
                    {
                        continue;
                    }
                    if (c_info.name == "tempest::ecs::self_component" ||
                        tempest::starts_with(c_info.name, "tempest::ecs::relationship_component") ||
                        c_info.name == "tempest::scene::scene_entity_component" ||
                        c_info.name == "tempest::editor::dynamic_entity_id_component" ||
                        c_info.name == "tempest::scene::prefab_member_component" ||
                        c_info.name == "tempest::scene::prefab_instance_component")
                    {
                        continue;
                    }

                    const auto display_name = get_display_component_name(c_info.name);
                    if (!_component_search_filter.PassFilter(display_name.c_str()) &&
                        !_component_search_filter.PassFilter(c_info.name.data(), c_info.name.data() + c_info.name.size()))
                    {
                        continue;
                    }

                    if (ImGui::Selectable(display_name.c_str()))
                    {
                        if (history != nullptr)
                        {
                            history->execute(make_unique<add_component_command>(addr, i), cmd_ctx);
                        }
                        else
                        {
                            ecs::raw_registry_access::assign_raw(*entity_registry, ent, c_info);
                        }
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }
} // namespace tempest::editor
