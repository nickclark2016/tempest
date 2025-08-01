#include "entity_panes.hpp"

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