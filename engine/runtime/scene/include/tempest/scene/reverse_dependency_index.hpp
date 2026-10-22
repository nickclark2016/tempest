#ifndef tempest_scene_reverse_dependency_index_hpp
#define tempest_scene_reverse_dependency_index_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/json.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    class reverse_dependency_codec;

    /// @brief Identifies an entity component field referencing an asset within a scene or prefab.
    struct TEMPEST_API asset_reference_location
    {
        guid owner_guid = {};       ///< GUID of the scene or prefab template containing the reference.
        id_path target_path = {};   ///< Hierarchical entity ID path targeting the referencing entity.
        string component_name = {}; ///< Qualified reflected component type name.
        string field_name = {};     ///< Reflected asset handle / GUID field name.
        string owner_path = {};     ///< Relative project path or disk path to the containing asset file.

        [[nodiscard]] auto operator==(const asset_reference_location& other) const noexcept -> bool = default;
    };

    /// @brief Reverse dependency index mapping referenced asset GUIDs to referencing locations.
    ///
    /// Maintains an inverted lookup table so deletions or renames of assets can instantly identify
    /// all referencing scenes, prefabs, and entity component fields without full project file scans.
    class TEMPEST_API reverse_dependency_index
    {
      public:
        reverse_dependency_index() = default;

        /// @brief Adds a reference location entry for the target asset GUID (lvalue copy).
        /// @param asset_id GUID of the referenced asset.
        /// @param loc Location metadata for the referencing entity and component field.
        /// @post has_references(asset_id) is true.
        auto add_reference(const guid& asset_id, const asset_reference_location& loc) -> void;

        /// @brief Adds a reference location entry for the target asset GUID (rvalue move).
        /// @param asset_id GUID of the referenced asset.
        /// @param loc Location metadata for the referencing entity and component field.
        /// @post has_references(asset_id) is true.
        auto add_reference(const guid& asset_id, asset_reference_location&& loc) -> void;

        /// @brief Removes a previously registered reference location entry.
        /// @param asset_id GUID of the referenced asset.
        /// @param loc Location metadata to remove.
        /// @return True if the reference was found and removed, false otherwise.
        /// @post If true is returned and all references for asset_id are removed, has_references(asset_id) is false.
        [[nodiscard]] auto remove_reference(const guid& asset_id, const asset_reference_location& loc) -> bool;

        /// @brief Removes all reference entries associated with a specific file path.
        /// @param file_path File path to match against asset_reference_location::owner_path.
        /// @return Number of reference entries removed.
        /// @post Any asset GUID whose reference locations become empty is pruned from the index.
        auto remove_references_for_file(string_view file_path) -> size_t;

        /// @brief Clears all recorded dependency relationships.
        /// @post empty() is true.
        /// @post all_dependencies().empty() is true.
        auto clear() noexcept -> void;

        /// @brief Checks whether the dependency index is completely empty.
        /// @return True if no dependency relationships are stored, false otherwise.
        [[nodiscard]] auto empty() const noexcept -> bool;

        /// @brief Returns the total number of distinct referenced asset entries tracked.
        /// @return Number of unique asset GUIDs with registered references.
        [[nodiscard]] auto size() const noexcept -> size_t;

        /// @brief Finds all recorded locations referencing a given asset GUID.
        /// @param asset_id Target asset GUID to query.
        /// @return Non-owning contiguous span of locations; empty if no references are recorded.
        [[nodiscard]] auto find_references(const guid& asset_id) const -> span<const asset_reference_location>;

        /// @brief Checks whether any references are registered for a given asset GUID.
        /// @param asset_id Target asset GUID to query.
        /// @return True if at least one reference exists, false otherwise.
        [[nodiscard]] auto has_references(const guid& asset_id) const noexcept -> bool;

        /// @brief Accesses the full immutable mapping of asset dependencies.
        /// @return Inverted map from asset GUID to vector of reference locations.
        [[nodiscard]] auto all_dependencies() const noexcept
            -> const flat_unordered_map<guid, vector<asset_reference_location>>&;

      private:
        friend class reverse_dependency_codec;

        flat_unordered_map<guid, vector<asset_reference_location>> _dependencies;
    };

    /// @brief Error codes returned by reverse dependency encoding and decoding routines.
    enum class reverse_dependency_codec_error : uint8_t
    {
        ok = 0,
        file_not_found,
        read_failed,
        write_failed,
        invalid_json,
        invalid_root_object,
        unsupported_format,
        version_mismatch,
        missing_dependencies,
    };

    /// @brief Formatting and encoding configuration options for reverse dependency serialization.
    struct TEMPEST_API reverse_dependency_serialize_options
    {
        static constexpr json_format default_format = json_format::pretty;

        json_format format = default_format;
    };

    /// @brief Serialization and deserialization codec for reverse dependency index documents.
    class TEMPEST_API reverse_dependency_codec
    {
      public:
        static constexpr string_view default_format_identifier = "tempest.reverse_dependencies";
        static constexpr int32_t current_schema_version = 1;

        reverse_dependency_codec() = delete;
        reverse_dependency_codec(const reverse_dependency_codec&) = delete;
        reverse_dependency_codec(reverse_dependency_codec&&) noexcept = delete;
        ~reverse_dependency_codec() = delete;

        auto operator=(const reverse_dependency_codec&) -> reverse_dependency_codec& = delete;
        auto operator=(reverse_dependency_codec&&) noexcept -> reverse_dependency_codec& = delete;

        /// @brief Serializes a reverse dependency index to a JSON string payload.
        /// @param index Dependency index to serialize.
        /// @param options Formatting options for JSON output.
        /// @return Serialized JSON text representation.
        [[nodiscard]] static auto serialize(const reverse_dependency_index& index,
                                            const reverse_dependency_serialize_options& options = {}) -> string;

        /// @brief Deserializes a reverse dependency index from a JSON string payload.
        /// @param json_str JSON string containing the serialized dependency cache.
        /// @return Instantiated reverse dependency index on success, or codec error code on failure.
        [[nodiscard]] static auto deserialize(string_view json_str)
            -> expected<reverse_dependency_index, reverse_dependency_codec_error>;

        /// @brief Writes a reverse dependency index to a cache file on disk.
        /// @param index Dependency index to serialize.
        /// @param disk_path Filesystem path to the destination file.
        /// @param options Formatting options for JSON output.
        /// @return Void on success, or codec error code on failure.
        [[nodiscard]] static auto save_to_file(const reverse_dependency_index& index, const filesystem::path& disk_path,
                                               const reverse_dependency_serialize_options& options = {})
            -> expected<void, reverse_dependency_codec_error>;

        /// @brief Reads and deserializes a reverse dependency index from a cache file on disk.
        /// @param disk_path Filesystem path to the source file.
        /// @return Instantiated reverse dependency index on success, or codec error code on failure.
        [[nodiscard]] static auto load_from_file(const filesystem::path& disk_path)
            -> expected<reverse_dependency_index, reverse_dependency_codec_error>;
    };
} // namespace tempest::scene

#endif // tempest_scene_reverse_dependency_index_hpp
