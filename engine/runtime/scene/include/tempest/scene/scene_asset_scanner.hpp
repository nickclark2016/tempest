#ifndef tempest_scene_scene_asset_scanner_hpp
#define tempest_scene_scene_asset_scanner_hpp

#include <tempest/api.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/reverse_dependency_index.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    /// @brief Discovered asset reference pair linking a referenced asset GUID to its location metadata.
    struct TEMPEST_API scanned_asset_reference
    {
        guid asset_id = {};
        asset_reference_location location = {};

        [[nodiscard]] auto operator==(const scanned_asset_reference& other) const noexcept -> bool = default;
    };

    /// @brief Utility scanner that inspects scene (.tscene) and prefab (.tprefab) JSON documents
    /// for reflected asset references and updates reverse dependency indices.
    class TEMPEST_API scene_asset_scanner
    {
      public:
        scene_asset_scanner() = delete;
        scene_asset_scanner(const scene_asset_scanner&) = delete;
        scene_asset_scanner(scene_asset_scanner&&) noexcept = delete;
        ~scene_asset_scanner() = delete;

        auto operator=(const scene_asset_scanner&) -> scene_asset_scanner& = delete;
        auto operator=(scene_asset_scanner&&) noexcept -> scene_asset_scanner& = delete;

        /// @brief Scans a scene or prefab JSON document content and returns all discovered asset references.
        /// @param json_content Raw JSON text of the .tscene or .tprefab document.
        /// @param registry Component type registry containing reflection descriptors.
        /// @param file_path File path to associate with recorded references (owner_path).
        /// @param owner_guid Optional GUID of the owning scene or prefab asset.
        /// @return Vector of discovered asset reference entries.
        [[nodiscard]] static auto scan_document(string_view json_content,
                                                const ecs::component_type_registry& registry,
                                                string_view file_path = {},
                                                const guid& owner_guid = {}) -> vector<scanned_asset_reference>;

        /// @brief Scans a scene or prefab JSON document content and records discovered references into the index.
        /// @param index Destination reverse dependency index.
        /// @param file_path File path to associate with recorded references (owner_path).
        /// @param json_content Raw JSON text of the .tscene or .tprefab document.
        /// @param registry Component type registry containing reflection descriptors.
        /// @param owner_guid Optional GUID of the owning scene or prefab asset.
        /// @return Number of asset reference locations discovered and added.
        static auto scan_document(reverse_dependency_index& index,
                                  string_view file_path,
                                  string_view json_content,
                                  const ecs::component_type_registry& registry,
                                  const guid& owner_guid = {}) -> size_t;

        /// @brief Removes all reference entries associated with a specific file path from the index.
        /// @param index Reverse dependency index to modify.
        /// @param file_path File path to match against asset_reference_location::owner_path.
        /// @return Number of reference entries removed.
        /// @post Any asset GUID whose reference locations become empty is pruned from the index.
        static auto remove_references_for_file(reverse_dependency_index& index, string_view file_path) -> size_t;

        /// @brief Incrementally updates references for a file: removes existing references for file_path,
        /// parses json_content, and adds newly discovered references into index.
        /// @param index Reverse dependency index to update.
        /// @param file_path File path of the modified scene or prefab.
        /// @param json_content New JSON content of the document.
        /// @param registry Component type registry containing reflection descriptors.
        /// @param owner_guid Optional GUID of the owning scene or prefab asset.
        /// @return Number of new asset reference locations recorded.
        /// @post Existing reference entries matching file_path are replaced by references discovered in json_content.
        /// @post Any asset GUID whose reference locations become empty is pruned from the index.
        static auto record_references_for_file(reverse_dependency_index& index,
                                               string_view file_path,
                                               string_view json_content,
                                               const ecs::component_type_registry& registry,
                                               const guid& owner_guid = {}) -> size_t;
    };

    /// @brief Free function helper removing all reference entries associated with a file path.
    /// @param index Reverse dependency index to modify.
    /// @param file_path File path to match against asset_reference_location::owner_path.
    /// @return Number of reference entries removed.
    /// @post Any asset GUID whose reference locations become empty is pruned from the index.
    inline auto remove_references_for_file(reverse_dependency_index& index, string_view file_path) -> size_t
    {
        return scene_asset_scanner::remove_references_for_file(index, file_path);
    }

    /// @brief Free function helper incrementally recording references for a scene or prefab document file.
    /// @param index Reverse dependency index to update.
    /// @param file_path File path of the modified scene or prefab.
    /// @param json_content New JSON content of the document.
    /// @param registry Component type registry containing reflection descriptors.
    /// @param owner_guid Optional GUID of the owning scene or prefab asset.
    /// @return Number of new asset reference locations recorded.
    /// @post Existing reference entries matching file_path are replaced by references discovered in json_content.
    /// @post Any asset GUID whose reference locations become empty is pruned from the index.
    inline auto record_references_for_file(reverse_dependency_index& index,
                                           string_view file_path,
                                           string_view json_content,
                                           const ecs::component_type_registry& registry,
                                           const guid& owner_guid = {}) -> size_t
    {
        return scene_asset_scanner::record_references_for_file(index, file_path, json_content, registry, owner_guid);
    }
} // namespace tempest::scene

#endif // tempest_scene_scene_asset_scanner_hpp
