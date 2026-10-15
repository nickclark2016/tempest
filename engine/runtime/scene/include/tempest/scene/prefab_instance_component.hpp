#ifndef tempest_scene_prefab_instance_component_hpp
#define tempest_scene_prefab_instance_component_hpp

#include <tempest/api.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/traits.hpp>

namespace tempest::scene
{
    /// @brief Attached to the root entity of an instantiated prefab to identify its source asset and overrides.
    struct TEMPEST_API prefab_instance_component
    {
        static constexpr guid default_prefab_guid = {};
        static constexpr entity_local_id default_instance_local_id = {};

        guid prefab_guid = default_prefab_guid;
        entity_local_id instance_local_id = default_instance_local_id;
        prefab_overrides overrides = {};
    };

    /// @brief Attached to each member entity of an instantiated prefab to record its local ID within the template.
    struct TEMPEST_API prefab_member_component
    {
        static constexpr entity_local_id default_template_local_id = {};

        entity_local_id template_local_id = default_template_local_id;
    };

    static_assert(ecs::component<prefab_member_component>);
} // namespace tempest::scene

namespace tempest::ecs
{
    template <>
    struct is_serializable<tempest::scene::prefab_instance_component> : false_type
    {
    };

    template <>
    struct is_duplicatable<tempest::scene::prefab_instance_component> : false_type
    {
    };

    template <>
    struct is_authorable<tempest::scene::prefab_instance_component> : false_type
    {
    };

    template <>
    struct is_serializable<tempest::scene::prefab_member_component> : false_type
    {
    };

    template <>
    struct is_duplicatable<tempest::scene::prefab_member_component> : false_type
    {
    };

    template <>
    struct is_authorable<tempest::scene::prefab_member_component> : false_type
    {
    };
} // namespace tempest::ecs

#endif // tempest_scene_prefab_instance_component_hpp
