#ifndef tempest_scene_scene_staging_hpp
#define tempest_scene_scene_staging_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    /// @brief Staged representation of a recognized component instance prior to ECS entity instantiation.
    struct TEMPEST_API staged_component
    {
        static constexpr const ecs::component_type_info* default_type_info = nullptr;

        const ecs::component_type_info* type_info = default_type_info;
        vector<byte> data;
    };

    /// @brief Staged representation of an entity decoded from a scene document.
    struct TEMPEST_API staged_entity
    {
        static constexpr entity_local_id default_local_id = {};

        entity_local_id local_id = default_local_id;
        string name;
        optional<size_t> parent_index = nullopt;
        vector<staged_component> components;
    };

    /// @brief Complete intermediate staging state for a decoded scene document prior to time-sliced commit.
    struct TEMPEST_API scene_staging
    {
        static constexpr bool default_success = false;

        vector<staged_entity> entities;
        vector<size_t> root_indices;
        scene_unrecognized_data unrecognized_data;
        bool success = default_success;
        string error_message;
    };
} // namespace tempest::scene

#endif // tempest_scene_scene_staging_hpp
