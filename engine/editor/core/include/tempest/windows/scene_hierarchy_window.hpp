#ifndef tempest_editor_core_scene_hierarchy_window_hpp
#define tempest_editor_core_scene_hierarchy_window_hpp

#include <tempest/archetype.hpp>
#include <tempest/vector.hpp>
#include <tempest/windows/editor_window.hpp>

namespace tempest::editor
{
    struct TEMPEST_EDITOR_API scene_hierarchy_window final : public editor_window
    {
        explicit scene_hierarchy_window(ecs::archetype_registry& registry) : entity_registry{&registry}
        {
        }

        auto desired_initial_dock() const -> editor_window::dock_location override;
        auto window_name() const -> string_view override;
        auto draw() -> void override;

        ecs::archetype_registry* entity_registry = nullptr;
        ecs::entity selected_entity = ecs::null;
        bool open = true;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_scene_hierarchy_window_hpp
