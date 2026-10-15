#ifndef tempest_editor_core_property_grid_hpp
#define tempest_editor_core_property_grid_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/checked.hpp>
#include <tempest/command_history.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/reflection.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/string_view.hpp>

#ifndef TEMPEST_EDITOR_API
#    define TEMPEST_EDITOR_API
#endif

namespace tempest::editor
{
    /// @brief Evaluation context passed to property_grid when rendering reflected properties.
    struct TEMPEST_EDITOR_API property_grid_context
    {
        static constexpr command_history* default_history = nullptr;
        static constexpr command_context* default_cmd_ctx = nullptr;
        static constexpr const scene::prefab_instance_component* default_prefab_instance = nullptr;
        static constexpr ecs::entity default_entity = ecs::tombstone;
        static constexpr scene::id_path default_id_path = {};

        non_null<ecs::archetype_registry> registry;
        ecs::entity entity = default_entity;
        entity_address address = {};
        command_history* history = default_history;
        command_context* cmd_ctx = default_cmd_ctx;
        const scene::prefab_instance_component* prefab_instance = default_prefab_instance;
        scene::id_path prefab_target_path = default_id_path;
    };

    /// @brief Generic reflection-driven property editor widget renderer.
    class TEMPEST_EDITOR_API property_grid
    {
      public:
        /// @brief Draws all visible properties of a component instance using reflection metadata.
        static auto draw_component(property_grid_context& ctx, const ecs::type_descriptor& desc,
                                   void* component_ptr) -> bool;

        /// @brief Draws a single reflected field with appropriate ImGui controls and command integration.
        static auto draw_field(property_grid_context& ctx, string_view component_name,
                               const ecs::field_descriptor& field, void* component_ptr) -> bool;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_property_grid_hpp
