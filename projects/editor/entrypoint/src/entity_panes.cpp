#include "entity_panes.hpp"

#include <tempest/asset_database.hpp>
#include <tempest/ui.hpp>

namespace tempest::editor
{
    entity_inspector::entity_inspector(ecs::archetype_registry& registry) : _registry(&registry)
    {
    }

    void entity_inspector::render()
    {
        if (ui::ui_context::begin_window({
                .name = "Entity Inspector",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
            if (_selected_entity == ecs::tombstone)
            {
                ui::ui_context::text("No entity selected.");
            }
            else
            {
                const auto name = _registry->name(_selected_entity);
                if (name)
                {
                    ui::ui_context::text("Selected Entity: %s", name->data());
                }
                else
                {
                    ui::ui_context::text("Selected Entity: <Unnamed>");
                }

                // TODO: Enumerate components of the selected entity.
            }
        }

        ui::ui_context::end_window();
    }

    bool entity_inspector::should_render() const noexcept
    {
        return true; // Always render for now.
    }

    bool entity_inspector::should_close() const noexcept
    {
        return false; // Do not close by default.
    }

    namespace
    {
        // Render a tree node for the given entity and its children.
        void render_entity_node(ecs::archetype_entity entity, const ecs::archetype_registry& registry,
                                ecs::archetype_entity& selected_entity)
        {
            // Iterate over descendants in a depth-first manner.
            // Relationships are a tree structure, with implicit links between siblings.
            // Each entity relationship stores the parent, the first child, and the next sibling.

            if (registry.has<assets::prefab_tag_t>(entity))
            {
                return;
            }

            auto rel_comp = registry.try_get<ecs::relationship_component<ecs::archetype_entity>>(entity);
            const bool has_children = rel_comp && rel_comp->first_child != ecs::tombstone;

            auto draw_children = [&]() {
                if (has_children)
                {
                    auto child = rel_comp->first_child;
                    while (child != ecs::tombstone)
                    {
                        render_entity_node(child, registry, selected_entity);
                        auto child_rel = registry.try_get<ecs::relationship_component<ecs::archetype_entity>>(child);
                        if (child_rel)
                        {
                            child = child_rel->next_sibling;
                        }
                        else
                        {
                            break; // No further siblings
                        }
                    }
                }
            };

            void* id = nullptr;
            detail::copy_bytes(&entity, &id, sizeof(entity));

            const auto name = registry.name(entity);
            const auto is_selected = entity == selected_entity;

            const auto node_flags = make_enum_mask(is_selected ? ui::ui_context::tree_node_flags::selected
                                                               : ui::ui_context::tree_node_flags::none);

            if (name)
            {
                if (has_children ? ui::ui_context::tree_node(id, node_flags, "%s", name->data())
                                 : ui::ui_context::tree_leaf(id, node_flags, "%s", name->data()))
                {
                    if (ui::ui_context::is_clicked())
                    {
                        selected_entity = entity;
                    }

                    draw_children();
                    ui::ui_context::tree_pop();
                }
            }
            else
            {
                if (has_children
                        ? ui::ui_context::tree_node(id, node_flags, "<Unnamed:%zu>", static_cast<size_t>(entity))
                        : ui::ui_context::tree_leaf(id, node_flags, "<Unnamed:%zu>", static_cast<size_t>(entity)))
                {
                    if (ui::ui_context::is_clicked())
                    {
                        selected_entity = entity;
                    }

                    draw_children();
                    ui::ui_context::tree_pop();
                }
            }
        }
    } // namespace

    scene_hierarchy::scene_hierarchy(ecs::archetype_registry& registry) : _registry(&registry)
    {
    }

    void scene_hierarchy::render()
    {
        if (ui::ui_context::begin_window({
                .name = "Scene Hierarchy",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
            _registry->each([&](ecs::self_component self) {
                auto rel_comp = _registry->try_get<ecs::relationship_component<ecs::archetype_entity>>(self.entity);
                // If the entity has no parent, render it at the root level.
                if (!rel_comp || rel_comp->parent == ecs::tombstone)
                {
                    render_entity_node(self.entity, *_registry, _selected_entity);
                }
            });
        }

        ui::ui_context::end_window();
    }

    bool scene_hierarchy::should_render() const noexcept
    {
        return true; // Always render for now.
    }

    bool scene_hierarchy::should_close() const noexcept
    {
        return false; // Do not close by default.
    }
} // namespace tempest::editor