#ifndef tempest_scene_scene_reference_stripper_hpp
#define tempest_scene_scene_reference_stripper_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/traits.hpp>
#include <tempest/vector.hpp>

namespace tempest::ecs
{
    class basic_archetype_registry;
    class component_type_registry;
} // namespace tempest::ecs

namespace tempest::scene
{
    class reverse_dependency_index;

    /// @brief Strips references to a target asset from a scene (.tscene) or prefab (.tprefab) JSON file on disk.
    ///
    /// Identifies and removes entities/components referencing target_asset_id:
    /// - An entity whose component has an asset_ref field equal to target_asset_id.
    /// - A prefab instance entity whose prefab_guid is equal to target_asset_id.
    /// - A prefab field override or added component in a prefab instance that references target_asset_id.
    ///
    /// Atomically rewrites the modified file to disk if changes were made.
    ///
    /// @param file_path Filesystem path to the .tscene or .tprefab file on disk.
    /// @param target_asset_id GUID of the asset being stripped.
    /// @param registry Component type registry containing reflection descriptors.
    /// @return Count of removed references/entities on success, or scene_error on failure.
    [[nodiscard]] TEMPEST_API auto strip_references_from_file(
        const filesystem::path& file_path,
        const guid& target_asset_id,
        const ecs::component_type_registry& registry) -> expected<size_t, scene_error>;

    /// @brief Identifies and destroys all live entities in a registry that reference target_asset_id.
    ///
    /// Evaluates reflected components with field_kind::asset_ref as well as
    /// prefab_instance_component::prefab_guid.
    ///
    /// @param registry Archetype registry containing live scene entities.
    /// @param target_asset_id GUID of the asset being stripped.
    /// @return Collection of live entities that were identified and destroyed.
    /// @post No live entities in registry reference target_asset_id.
    TEMPEST_API auto strip_references_from_registry(
        ecs::basic_archetype_registry& registry,
        const guid& target_asset_id) -> vector<ecs::entity>;

    /// @brief High-level coordinator that strips references to target_asset_id across active registries
    /// and disk files, updating the reverse dependency index.
    ///
    /// Queries the index for affected locations, removes in-memory entities from active_registry (if provided),
    /// strips references from affected disk files, and updates the reverse dependency index.
    ///
    /// @param index Reverse dependency index tracking cross-asset references.
    /// @param target_asset_id GUID of the asset being deleted or stripped.
    /// @param registry Component type registry containing reflection descriptors.
    /// @param active_registry Optional pointer to an open archetype registry.
    /// @return Total count of stripped references across active registry and disk files.
    /// @post index.has_references(target_asset_id) is false.
    TEMPEST_API auto strip_asset_references(
        reverse_dependency_index& index,
        const guid& target_asset_id,
        const ecs::component_type_registry& registry,
        ecs::basic_archetype_registry* active_registry = nullptr) -> size_t;
} // namespace tempest::scene

#endif // tempest_scene_scene_reference_stripper_hpp
