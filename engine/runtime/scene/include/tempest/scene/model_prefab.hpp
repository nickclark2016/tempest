#ifndef tempest_scene_model_prefab_hpp
#define tempest_scene_model_prefab_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string_view.hpp>

namespace tempest::scene
{
    /// @brief Computes a deterministic 64-bit FNV-1a hash combining source asset GUID and hierarchical node path.
    inline auto fnv1a64(const guid& source_guid, string_view node_path) noexcept -> uint64_t
    {
        const auto guid_hash = detail::fnv1a64(source_guid.data.data(), source_guid.data.size());
        return detail::fnv1a64(node_path.data(), node_path.size(), guid_hash);
    }

    /// @brief Computes a deterministic entity_local_id for a member entity within a model prefab.
    inline auto compute_model_prefab_local_id(const guid& source_guid, string_view node_path) noexcept
        -> entity_local_id
    {
        const auto hash_val = fnv1a64(source_guid, node_path);
        return entity_local_id{.value = (hash_val != entity_local_id::invalid_value) ? hash_val : 1};
    }

    /// @brief Synthesizes an immutable prefab_template from an imported model entity hierarchy.
    /// Traverses the hierarchy from root_entity, determines hierarchical node paths (e.g. "/Root/Node_0"),
    /// computes deterministic entity_local_id for each member entity, creates prefab_entity_records,
    /// omits root transform_component per Requirement 7c, and returns the synthesized prefab_template.
    TEMPEST_API auto synthesize_model_prefab(const ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                             const guid& source_guid) -> prefab_template;
} // namespace tempest::scene

#endif // tempest_scene_model_prefab_hpp
