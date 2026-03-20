#ifndef tempest_assets_asset_database_hpp
#define tempest_assets_asset_database_hpp

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
#include <tempest/vector.hpp>

namespace tempest::assets
{
    struct prefab_tag_t
    {
    };

    inline constexpr prefab_tag_t prefab_tag{};

    struct content_hash
    {
        array<byte, 32> data{};
    };

    struct source_entry
    {
        guid id;
        string source_path;
        content_hash source_hash;
    };

    struct asset_entry
    {
        guid id;
        asset_type_id type;
        uint64_t blob_offset{0};
        uint64_t blob_size{0};
        guid source_id;
        vector<guid> dependencies;
        flat_unordered_map<string, string> user_metadata;
    };

    struct import_result
    {
        ecs::archetype_entity root_entity;
        vector<guid> produced_assets;
    };

    /// Callbacks for serializing/deserializing a single trivial ECS component
    /// to/from the entity hierarchy blob.
    struct component_serializer
    {
        size_t type_hash;
        size_t component_size;

        /// Try to serialize the component from `entity` into `out_bytes`.
        /// Returns true if the entity has this component and serialization succeeded.
        function<bool(const ecs::archetype_registry& registry, ecs::archetype_entity entity,
                       vector<byte>& out_bytes)> serialize;

        /// Assign the component described by `bytes` onto `entity`.
        /// Returns true on success.
        function<bool(ecs::archetype_registry& registry, ecs::archetype_entity entity,
                       span<const byte> bytes)> deserialize;
    };

    class asset_database
    {
      public:
        struct asset_metadata
        {
            string path;
            flat_unordered_map<string, string> metadata;
        };

        explicit asset_database(asset_type_registry* type_reg) noexcept;

        void open(string_view db_path);
        bool save() const;

        [[nodiscard]] ecs::archetype_entity load(string_view source_path,
                                                  ecs::archetype_registry& registry);

        [[nodiscard]] const asset_entry* find_by_guid(const guid& asset_id) const;
        [[nodiscard]] const asset_entry* find_by_path(string_view path) const;

        guid register_asset(asset_type_id type, string_view source_path);
        bool register_asset_with_guid(const guid& id, asset_type_id type, string_view source_path);
        void store_blob(const guid& asset_id, span<const byte> blob_data);
        span<const byte> get_blob(const guid& asset_id) const;

        void register_importer(unique_ptr<asset_importer> importer, string_view extension);

        /// Register a trivial ECS component type for entity hierarchy serialization.
        /// The component is serialized/deserialized via memcpy.
        template <typename T>
            requires is_trivial_v<T>
        void register_component();

        [[nodiscard]] auto register_asset_metadata(asset_metadata meta) -> guid;
        [[nodiscard]] auto get_asset_metadata(guid asset_id) const -> optional<const asset_metadata&>;

        asset_type_registry* type_registry() noexcept
        {
            return _type_reg;
        }

        const asset_type_registry* type_registry() const noexcept
        {
            return _type_reg;
        }

      private:
        asset_type_registry* _type_reg;

        string _db_path;

        vector<unique_ptr<source_entry>> _sources;
        flat_unordered_map<string, size_t> _source_path_to_index;
        flat_unordered_map<guid, size_t> _source_id_to_index;

        vector<unique_ptr<asset_entry>> _assets;
        flat_unordered_map<guid, size_t> _asset_guid_to_index;

        vector<byte> _blob_data;

        flat_unordered_map<string, unique_ptr<asset_importer>> _importers;
        flat_unordered_map<guid, asset_metadata> _metadata;

        vector<component_serializer> _component_serializers;
        flat_unordered_map<size_t, size_t> _component_hash_to_index;

        [[nodiscard]] ecs::archetype_entity _load_from_blobs(string_view source_path,
                                                              ecs::archetype_registry& registry);
        [[nodiscard]] ecs::archetype_entity _load_via_import(string_view source_path,
                                                              ecs::archetype_registry& registry);

        source_entry& _get_or_create_source(string_view source_path);
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

        component_serializer cs;
        cs.type_hash = hash;
        cs.component_size = sizeof(T);

        cs.serialize = [](const ecs::archetype_registry& registry, ecs::archetype_entity entity,
                          vector<byte>& out_bytes) -> bool {
            const auto* comp = registry.try_get<T>(entity);
            if (comp == nullptr)
            {
                return false;
            }
            out_bytes.resize(sizeof(T));
            tempest::memcpy(out_bytes.data(), comp, sizeof(T));
            return true;
        };

        cs.deserialize = [](ecs::archetype_registry& registry, ecs::archetype_entity entity,
                            span<const byte> bytes) -> bool {
            if (bytes.size() != sizeof(T))
            {
                return false;
            }
            T comp;
            tempest::memcpy(&comp, bytes.data(), sizeof(T));
            registry.assign(entity, comp);
            return true;
        };

        auto index = _component_serializers.size();
        _component_hash_to_index.insert({hash, index});
        _component_serializers.push_back(tempest::move(cs));
    }

    struct asset_metadata_component
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