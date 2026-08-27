#include <tempest/windows/scene_hierarchy_window.hpp>

#include <tempest/asset_database.hpp>
#include <tempest/bit.hpp>
#include <tempest/iterator.hpp>
#include <tempest/string.hpp>

#include <imgui.h>

#include <format>

namespace tempest::editor
{
    namespace
    {
        auto render_entity_node(ecs::entity entity, const ecs::archetype_registry& reg, ecs::entity& select) -> void
        {
            if (reg.has<assets::prefab_tag_t>(entity))
            {
                return;
            }

            const auto* rel_comp = reg.try_get<ecs::relationship_component<ecs::entity>>(entity);
            const bool has_children = rel_comp != nullptr && rel_comp->first_child != ecs::tombstone;

            auto draw_children = [&]() {
                if (rel_comp == nullptr)
                {
                    return;
                }

                auto child = rel_comp->first_child;
                while (child != ecs::tombstone)
                {
                    render_entity_node(child, reg, select);
                    const auto* child_rel = reg.try_get<ecs::relationship_component<ecs::entity>>(child);
                    if (child_rel)
                    {
                        child = child_rel->next_sibling;
                    }
                    else
                    {
                        break;
                    }
                }
            };

            const void* eid = bit_cast<void*>(entity);
            const auto name = reg.name(entity);

            const auto is_selected = select == entity;
            const auto node_flags = is_selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;

            auto label = string{};
            if (name.has_value())
            {
                const auto& n = name.value();
                std::format_to(tempest::back_inserter(label), "{:.{}}", n.data(), n.size());
            }
            else
            {
                std::format_to(tempest::back_inserter(label), "<Unnamed:{}>", static_cast<size_t>(entity));
            }

            auto node_open = false;
            if (has_children)
            {
                node_open = ImGui::TreeNodeEx(eid, node_flags, "%s", label.c_str());
            }
            else
            {
                node_open = ImGui::TreeNodeEx(eid, node_flags | ImGuiTreeNodeFlags_Leaf, "%s", label.c_str());
            }

            if (node_open)
            {
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    select = entity;
                }

                draw_children();

                ImGui::TreePop();
            }
        }
    } // namespace

    auto scene_hierarchy_window::desired_initial_dock() const -> editor_window::dock_location
    {
        return dock_location::left;
    }

    auto scene_hierarchy_window::window_name() const -> string_view
    {
        return "Scene Hierarchy";
    }

    auto scene_hierarchy_window::draw() -> void
    {
        const auto name = window_name();

        if (ImGui::Begin(name.data(), &open))
        {
            for (const auto& [self] : entity_registry->with<ecs::self_component>())
            {
                const auto* const rel_component =
                    entity_registry->try_get<ecs::relationship_component<ecs::entity>>(self.entity);
                const auto is_root = rel_component != nullptr ? rel_component->parent == ecs::tombstone : true;

                if (is_root)
                {
                    auto selected = selected_entity;
                    render_entity_node(self.entity, *entity_registry, selected);
                    if (selected != ecs::null)
                    {
                        selected_entity = selected;
                    }
                }
            }
        }
        ImGui::End();
    }
} // namespace tempest::editor