#ifndef tempest_editor_core_dynamic_entity_id_component_hpp
#define tempest_editor_core_dynamic_entity_id_component_hpp

#include <tempest/int.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/traits.hpp>

namespace tempest::editor
{
    /// @brief Ephemeral identifier component assigned to dynamic (non-scene-bound) entities for stable addressing.
    struct dynamic_entity_id_component
    {
        static constexpr scene::entity_local_id default_local_id = {};

        scene::entity_local_id local_id = default_local_id;
    };

    static_assert(ecs::component<dynamic_entity_id_component>);
} // namespace tempest::editor

namespace tempest::ecs
{
    template <>
    struct is_serializable<tempest::editor::dynamic_entity_id_component> : false_type
    {
    };

    template <>
    struct is_duplicatable<tempest::editor::dynamic_entity_id_component> : false_type
    {
    };

    template <>
    struct is_authorable<tempest::editor::dynamic_entity_id_component> : false_type
    {
    };
} // namespace tempest::ecs

#endif // tempest_editor_core_dynamic_entity_id_component_hpp
