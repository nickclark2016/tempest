#ifndef tempest_scene_scene_entity_component_hpp
#define tempest_scene_scene_entity_component_hpp

#include <tempest/int.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/traits.hpp>

namespace tempest::scene
{
    /// @brief Structural component attaching an entity to a scene with a persistent 64-bit local identifier.
    struct scene_entity_component
    {
        static constexpr entity_local_id default_local_id = {};

        scene_handle scene = {};
        entity_local_id local_id = default_local_id;
    };

    static_assert(ecs::component<scene_entity_component>);
} // namespace tempest::scene

namespace tempest::ecs
{
    template <>
    struct is_serializable<tempest::scene::scene_entity_component> : false_type
    {
    };

    template <>
    struct is_duplicatable<tempest::scene::scene_entity_component> : false_type
    {
    };

    template <>
    struct is_authorable<tempest::scene::scene_entity_component> : false_type
    {
    };
} // namespace tempest::ecs

#endif // tempest_scene_scene_entity_component_hpp
