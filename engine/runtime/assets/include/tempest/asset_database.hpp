#ifndef tempest_assets_asset_database_hpp
#define tempest_assets_asset_database_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/asset_importer.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/functional.hpp>
#include <tempest/guid.hpp>
#include <tempest/memory.hpp>
#include <tempest/meta.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    struct TEMPEST_API prefab_tag_t
    {
    };

    inline constexpr prefab_tag_t prefab_tag{};

    struct TEMPEST_API content_hash
    {
        static constexpr size_t hash_size = 32;

        array<byte, hash_size> data{};
    };

    struct TEMPEST_API source_entry
    {
        guid id;
        string source_path;
        content_hash source_hash;
    };

    struct TEMPEST_API asset_entry
    {
        guid id;
        asset_type_id type;
        uint64_t blob_offset{0};
        uint64_t blob_size{0};
        guid source_id;
        vector<guid> dependencies;
        flat_unordered_map<string, string> user_metadata;
    };

    struct TEMPEST_API import_result
    {
        ecs::entity root_entity;
        vector<guid> produced_assets;
    };

    /// Callbacks for serializing/deserializing a single trivial ECS component
    /// to/from the entity hierarchy blob.
    struct TEMPEST_API component_serializer
    {
        size_t type_hash;
        size_t component_size;

        /// Try to serialize the component from `entity` into `out_bytes`.
        /// Returns true if the entity has this component and serialization succeeded.
        function<bool(const ecs::archetype_registry& registry, ecs::entity entity, vector<byte>& out_bytes)> serialize;

        /// Assign the component described by `bytes` onto `entity`.
        /// Returns true on success.
        function<bool(ecs::archetype_registry& registry, ecs::entity entity, span<const byte> bytes)> deserialize;
    };

    struct TEMPEST_API shader_asset
    {
    };

    struct TEMPEST_API mount_point
    {
        string path;
        int32_t priority{0};
        string alias;

        bool operator==(const mount_point& rhs) const noexcept
        {
            return priority == rhs.priority && path == rhs.path && alias == rhs.alias;
        }
    };

    struct TEMPEST_API scan_options
    {
        vector<string> extension_whitelist;
        vector<string> additional_ignored_directories;
        function<bool(string_view relative_path, bool is_directory)> predicate;
    };

    class TEMPEST_API asset_database
    {
      public:
        struct TEMPEST_API asset_metadata
        {
            string path;
            flat_unordered_map<string, string> metadata;
        };

        explicit asset_database(asset_type_registry* type_reg = nullptr) noexcept;

        auto open(string_view db_path) -> void;
        [[nodiscard]] auto save() const -> bool;

        [[nodiscard]] auto load(string_view source_path, ecs::archetype_registry& registry) -> ecs::entity;
        [[nodiscard]] auto find_by_guid(const guid& asset_id) const -> const asset_entry*;
        [[nodiscard]] auto find_by_path(string_view path) const -> const asset_entry*;

        // Mount Root Management
        auto mount_root(string_view root_path, int32_t priority = 0, string_view alias = "") -> void;
        auto unmount_root(string_view root_path) -> void;
        auto clear_mount_roots() -> void;
        [[nodiscard]] auto get_mount_roots() const -> span<const mount_point>;

        // Directory Scanning & Indexing
        auto scan_and_index(const scan_options& options = {}) -> void;

        // Ignore Management
        auto add_ignored_directory(string_view dir_name) -> void;
        auto add_ignored_extension(string_view extension) -> void;
        auto clear_ignores() -> void;
        [[nodiscard]] auto get_ignored_directories() const -> span<const string>;
        [[nodiscard]] auto get_ignored_extensions() const -> span<const string>;

        // Asset & Path Resolution
        [[nodiscard]] auto find_asset(string_view path_or_name) const -> const asset_entry*;
        [[nodiscard]] auto find_source_by_id(const guid& source_id) const -> const source_entry*;
        [[nodiscard]] auto resolve_source_path(string_view path_or_name) const -> optional<string>;
        [[nodiscard]] auto resolve_disk_path(string_view relative_path) const -> optional<string>;

        // Filewatching & Change Notification
        auto notify_file_changed(string_view disk_path) -> bool;

        auto register_asset(asset_type_id type, string_view source_path) -> guid;
        auto register_asset_with_guid(const guid& uid, asset_type_id type, string_view source_path) -> bool;
        auto store_blob(const guid& asset_id, span<const byte> blob_data) -> void;
        [[nodiscard]] auto get_blob(const guid& asset_id) const -> span<const byte>;

        auto register_importer(unique_ptr<asset_importer> importer, string_view extension) -> void;

        /// Register a trivial ECS component type for entity hierarchy serialization.
        /// The component is serialized/deserialized via memcpy.
        template <typename T>
            requires is_trivial_v<T>
        auto register_component() -> void;

        [[nodiscard]] auto register_asset_metadata(asset_metadata meta) -> guid;
        [[nodiscard]] auto get_asset_metadata(guid asset_id) const -> optional<const asset_metadata&>;

        [[nodiscard]] auto type_registry() noexcept -> asset_type_registry*
        {
            return _type_reg;
        }

        [[nodiscard]] auto type_registry() const noexcept -> const asset_type_registry*
        {
            return _type_reg;
        }

      private:
        asset_type_registry* _type_reg;

        string _db_path;

        vector<mount_point> _mount_roots;
        vector<string> _ignored_directories;
        vector<string> _ignored_extensions;
        flat_unordered_map<string, string> _basename_to_relative_path;
        flat_unordered_map<string, string> _relative_path_to_source_path;

        vector<unique_ptr<source_entry>> _sources;
        flat_unordered_map<string, size_t> _source_path_to_index;
        flat_unordered_map<guid, size_t> _source_id_to_index;

        vector<unique_ptr<asset_entry>> _assets;
        flat_unordered_map<guid, size_t> _asset_guid_to_index;

        mutable vector<unique_ptr<vector<byte>>> _blob_chunks;
        mutable unique_ptr<vector<byte>> _packing_chunk;
        mutable size_t _current_chunk_capacity{0};
        mutable size_t _current_chunk_used{0};
        mutable flat_unordered_map<guid, span<const byte>> _cached_blobs;

        flat_unordered_map<string, unique_ptr<asset_importer>> _importers;
        flat_unordered_map<guid, asset_metadata> _metadata;

        vector<component_serializer> _component_serializers;
        flat_unordered_map<size_t, size_t> _component_hash_to_index;

        [[nodiscard]] auto _load_from_blobs(string_view source_path, ecs::archetype_registry& registry) -> ecs::entity;
        [[nodiscard]] auto _load_via_import(string_view source_path, ecs::archetype_registry& registry) -> ecs::entity;

        auto _get_or_create_source(string_view source_path) -> source_entry&;

        bool _dirty = false;
    };

    template <typename T>
        requires is_trivial_v<T>
    void asset_database::register_component()
    {
        const auto hash = core::type_hash<T>::value();

        // Idempotent: skip if already registered
        if (_component_hash_to_index.find(hash) != _component_hash_to_index.end())
        {
            return;
        }

        auto serializer = component_serializer{
            .type_hash = hash,
            .component_size = sizeof(T),
            .serialize = [](const ecs::archetype_registry& registry, ecs::entity entity,
                            vector<byte>& out_bytes) -> bool {
                const auto* comp = registry.try_get<T>(entity);
                if (comp == nullptr)
                {
                    return false;
                }
                out_bytes.resize(sizeof(T));
                tempest::memcpy(out_bytes.data(), comp, sizeof(T));
                return true;
            },
            .deserialize = [](ecs::archetype_registry& registry, ecs::entity entity, span<const byte> bytes) -> bool {
                if (bytes.size() != sizeof(T))
                {
                    return false;
                }
                T comp;
                tempest::memcpy(&comp, bytes.data(), sizeof(T));
                registry.assign(entity, comp);
                return true;
            },
        };

        auto index = _component_serializers.size();
        _component_hash_to_index.insert({hash, index});
        _component_serializers.push_back(tempest::move(serializer));
    }

    struct TEMPEST_API asset_metadata_component
    {
        guid metadata_id;
    };
} // namespace tempest::assets

namespace tempest::ecs
{
    template <>
    struct is_duplicatable<assets::prefab_tag_t> : false_type
    {
    };
} // namespace tempest::ecs

#endif // tempest_assets_asset_database_hpp