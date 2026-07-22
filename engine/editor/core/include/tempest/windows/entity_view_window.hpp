#ifndef tempest_editor_core_entity_view_window_hpp
#define tempest_editor_core_entity_view_window_hpp

#include <tempest/archetype.hpp>
#include <tempest/concepts.hpp>
#include <tempest/vector.hpp>
#include <tempest/windows/component_view.hpp>
#include <tempest/windows/editor_window.hpp>

#include <imgui.h>

namespace tempest::editor
{
    struct TEMPEST_EDITOR_API entity_view_window final : public editor_window
    {
        explicit entity_view_window(ecs::archetype_registry& registry) : entity_registry{&registry}
        {
        }

        auto desired_initial_dock() const -> editor_window::dock_location override;
        auto window_name() const -> string_view override;
        auto draw() -> void override;

        ecs::archetype_registry* entity_registry = nullptr;
        ecs::entity target = ecs::null;
        vector<unique_ptr<component_view_provider>> providers;
        bool open = true;

        struct
        {
            ImGuiTextFilter text_filter;
            vector<component_view_provider*> sorted_providers;
            vector<int> filtered_indices;
            int selected = -1;
            bool focus_search = false;
        } component_search;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_entity_view_window_hpp
