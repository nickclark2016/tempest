#include <tempest/asset_database.hpp>
#include <tempest/asset_serializers.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/files.hpp>
#include <tempest/logger.hpp>
#include <tempest/serial.hpp>

#include <fstream>

namespace tempest::assets
{
    namespace
    {
        auto log = logger::logger_factory::create({
            .prefix = "tempest::asset_database",
        });

        constexpr array<uint8_t, 4> db_magic = {'T', 'E', 'B', 'F'};
        constexpr uint16_t db_version = 2;
    } // namespace

    asset_database::asset_database(asset_type_registry* type_reg) noexcept : _type_reg{type_reg}
    {
    }

    auto asset_database::open(string_view db_path) -> void
    {
        _db_path = string(db_path);

        // Clear existing data
        _sources.clear();
        _source_path_to_index.clear();
        _source_id_to_index.clear();
        _assets.clear();
        _asset_guid_to_index.clear();
        _blob_data.clear();

        // Try to read existing database file
        std::ifstream file(string(db_path).c_str(), std::ios::binary);
        if (!file.is_open())
        {
            log->info("No existing database file at '{}', starting empty.", std::string_view(db_path.data(), db_path.size()));
            return;
        }

        // Read binary header
        serialization::binary_header header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file || header.magic != db_magic || header.version != db_version)
        {
            log->warn("Invalid or incompatible database header at '{}', starting empty.", std::string_view(db_path.data(), db_path.size()));
            return;
        }

        // Read the rest of the file into a buffer
        auto data_size = static_cast<size_t>(header.data_length);
        vector<byte> buffer;
        buffer.resize(data_size);
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(data_size));
        file.close();

        serialization::binary_archive archive;
        archive.write(span<const byte>{buffer.data(), buffer.size()});

        // Read type registry entries and validate
        auto num_types = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);
        for (uint64_t idx = 0; idx < num_types; ++idx)
        {
            auto type_hash = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);
            auto type_name = serialization::serializer<serialization::binary_archive, string>::deserialize(archive);
            auto type_id = asset_type_id::from_hash(static_cast<size_t>(type_hash));
            auto validation = _type_reg->validate(type_id, type_name);
            if (validation.has_value() && !validation.value())
            {
                log->warn("Type registry mismatch for hash {}: expected '{}', skipping database.",
                          type_hash, std::string_view(type_name.data(), type_name.size()));
                return;
            }
        }

        // Read source table
        auto num_sources = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);
        for (uint64_t idx = 0; idx < num_sources; ++idx)
        {
            auto src_id = serialization::serializer<serialization::binary_archive, guid>::deserialize(archive);
            auto src_path = serialization::serializer<serialization::binary_archive, string>::deserialize(archive);
            auto src_hash =
                serialization::serializer<serialization::binary_archive, content_hash>::deserialize(archive);

            auto entry = make_unique<source_entry>(source_entry{
                .id = src_id,
                .source_path = tempest::move(src_path),
                .source_hash = src_hash,
            });

            auto source_index = _sources.size();
            string path_copy = entry->source_path;
            _source_path_to_index.insert({tempest::move(path_copy), source_index});
            _source_id_to_index.insert({entry->id, source_index});
            _sources.push_back(tempest::move(entry));
        }

        // Read asset table
        auto num_assets = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);
        for (uint64_t idx = 0; idx < num_assets; ++idx)
        {
            auto asset_id = serialization::serializer<serialization::binary_archive, guid>::deserialize(archive);
            auto type_hash = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);
            auto blob_offset = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);
            auto blob_size = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);
            auto source_id = serialization::serializer<serialization::binary_archive, guid>::deserialize(archive);
            auto deps = serialization::serializer<serialization::binary_archive, vector<guid>>::deserialize(archive);
            auto meta =
                serialization::serializer<serialization::binary_archive,
                                          flat_unordered_map<string, string>>::deserialize(archive);

            auto entry = make_unique<asset_entry>(asset_entry{
                .id = asset_id,
                .type = asset_type_id::from_hash(static_cast<size_t>(type_hash)),
                .blob_offset = blob_offset,
                .blob_size = blob_size,
                .source_id = source_id,
                .dependencies = tempest::move(deps),
                .user_metadata = tempest::move(meta),
            });

            auto asset_index = _assets.size();
            _asset_guid_to_index.insert({entry->id, asset_index});
            _assets.push_back(tempest::move(entry));
        }

        // Read blob section
        auto blob_size = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);
        if (blob_size > 0)
        {
            auto blob_span = archive.read(static_cast<size_t>(blob_size));
            _blob_data.insert(_blob_data.end(), blob_span.begin(), blob_span.end());
        }

        log->info("Loaded database with {} sources, {} assets from '{}'.", _sources.size(), _assets.size(), std::string_view(db_path.data(), db_path.size()));
    }

    auto asset_database::save() const -> bool
    {
        if (_db_path.empty())
        {
            log->error("Cannot save: no database path set. Call open() first.");
            return false;
        }

        serialization::binary_archive archive;

        // Write type registry section
        uint64_t num_types = 0;
        _type_reg->for_each([&num_types](const type_entry&) { ++num_types; });
        serialization::serializer<serialization::binary_archive, uint64_t>::serialize(archive, num_types);
        _type_reg->for_each([&archive](const type_entry& entry) {
            serialization::serializer<serialization::binary_archive, uint64_t>::serialize(
                archive, static_cast<uint64_t>(entry.id.hash()));
            serialization::serializer<serialization::binary_archive, string>::serialize(archive,
                                                                                        entry.canonical_name);
        });

        // Write source table
        serialization::serializer<serialization::binary_archive, uint64_t>::serialize(
            archive, static_cast<uint64_t>(_sources.size()));
        for (const auto& src : _sources)
        {
            serialization::serializer<serialization::binary_archive, guid>::serialize(archive, src->id);
            serialization::serializer<serialization::binary_archive, string>::serialize(archive, src->source_path);
            serialization::serializer<serialization::binary_archive, content_hash>::serialize(archive,
                                                                                              src->source_hash);
        }

        // Write asset table
        serialization::serializer<serialization::binary_archive, uint64_t>::serialize(
            archive, static_cast<uint64_t>(_assets.size()));
        for (const auto& asset : _assets)
        {
            serialization::serializer<serialization::binary_archive, guid>::serialize(archive, asset->id);
            serialization::serializer<serialization::binary_archive, uint64_t>::serialize(
                archive, static_cast<uint64_t>(asset->type.hash()));
            serialization::serializer<serialization::binary_archive, uint64_t>::serialize(archive,
                                                                                          asset->blob_offset);
            serialization::serializer<serialization::binary_archive, uint64_t>::serialize(archive, asset->blob_size);
            serialization::serializer<serialization::binary_archive, guid>::serialize(archive, asset->source_id);
            serialization::serializer<serialization::binary_archive, vector<guid>>::serialize(archive,
                                                                                              asset->dependencies);
            serialization::serializer<serialization::binary_archive,
                                      flat_unordered_map<string, string>>::serialize(archive, asset->user_metadata);
        }

        // Write blob section
        serialization::serializer<serialization::binary_archive, uint64_t>::serialize(
            archive, static_cast<uint64_t>(_blob_data.size()));
        if (!_blob_data.empty())
        {
            archive.write(span<const byte>{_blob_data.data(), _blob_data.size()});
        }

        // Now write everything to disk with the binary header
        // Get the serialized data by reading from a fresh archive perspective
        // We need to extract the buffer. Since binary_archive doesn't expose its buffer directly,
        // we use the data_length from what we wrote.

        // Write binary header + data to file
        serialization::binary_header header;
        header.magic = db_magic;
        header.version = db_version;
        header.flags = 0;

        // We need to get the serialized bytes. Read back the entire archive.
        // The archive's internal buffer holds all our written data.
        // We'll compute data_length based on what we know we wrote.
        // For now, re-serialize into a separate buffer to get the bytes.

        // Actually, the binary_archive accumulates writes in its internal buffer.
        // We need access to the raw buffer. Let's read everything back.
        // The archive allows read of all data that was written.
        // We can read all remaining bytes.
        auto total_size = archive.written_size();
        header.data_length = total_size;

        std::ofstream file(_db_path.c_str(), std::ios::binary);
        if (!file.is_open())
        {
            log->error("Failed to open '{}' for writing.", std::string_view(_db_path.data(), _db_path.size()));
            return false;
        }

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        auto all_data = archive.read(total_size);
        file.write(reinterpret_cast<const char*>(all_data.data()), static_cast<std::streamsize>(all_data.size()));
        file.close();

        log->info("Saved database with {} sources, {} assets to '{}'.", _sources.size(), _assets.size(), std::string_view(_db_path.data(), _db_path.size()));
        return true;
    }

    auto asset_database::load(string_view source_path, ecs::archetype_registry& registry) -> ecs::archetype_entity
    {
        // Check if source exists in the database
        auto path_it = _source_path_to_index.find(string(source_path));
        if (path_it != _source_path_to_index.end())
        {
            return _load_from_blobs(source_path, registry);
        }

        // Fall back to importing
        return _load_via_import(source_path, registry);
    }

    auto asset_database::find_by_guid(const guid& asset_id) const -> const asset_entry*
    {
        auto iter = _asset_guid_to_index.find(asset_id);
        if (iter != _asset_guid_to_index.end())
        {
            return _assets[iter->second].get();
        }
        return nullptr;
    }

    auto asset_database::find_by_path(string_view path) const -> const asset_entry*
    {
        // Find the source entry for this path
        auto src_it = _source_path_to_index.find(string(path));
        if (src_it == _source_path_to_index.end())
        {
            return nullptr;
        }

        const auto& src = _sources[src_it->second];

        // Find the first asset entry that references this source
        for (const auto& asset : _assets)
        {
            if (asset->source_id == src->id)
            {
                return asset.get();
            }
        }
        return nullptr;
    }

    auto asset_database::register_asset(asset_type_id type, string_view source_path) -> guid
    {
        auto& src = _get_or_create_source(source_path);

        auto new_id = guid::generate_random_guid();

        auto entry = make_unique<asset_entry>(asset_entry{
            .id = new_id,
            .type = type,
            .blob_offset = 0,
            .blob_size = 0,
            .source_id = src.id,
            .dependencies = {},
            .user_metadata = {},
        });

        auto index = _assets.size();
        _asset_guid_to_index.insert({new_id, index});
        _assets.push_back(tempest::move(entry));

        return new_id;
    }

    auto asset_database::register_asset_with_guid(const guid& uid, asset_type_id type, string_view source_path) -> bool
    {
        if (_asset_guid_to_index.contains(uid))
        {
            return false;
        }

        auto& src = _get_or_create_source(source_path);

        auto entry = make_unique<asset_entry>(asset_entry{
            .id = uid,
            .type = type,
            .blob_offset = 0,
            .blob_size = 0,
            .source_id = src.id,
            .dependencies = {},
            .user_metadata = {},
        });

        auto index = _assets.size();
        _asset_guid_to_index.insert({uid, index});
        _assets.push_back(tempest::move(entry));

        return true;
    }

    auto asset_database::store_blob(const guid& asset_id, span<const byte> data) -> void
    {
        auto iter = _asset_guid_to_index.find(asset_id);
        if (iter == _asset_guid_to_index.end())
        {
            log->error("Cannot store blob: asset GUID not found.");
            return;
        }

        auto& entry = _assets[iter->second];
        entry->blob_offset = _blob_data.size();
        entry->blob_size = data.size();
        _blob_data.insert(_blob_data.end(), data.begin(), data.end());
    }

    auto asset_database::get_blob(const guid& asset_id) const -> span<const byte>
    {
        auto iter = _asset_guid_to_index.find(asset_id);
        if (iter == _asset_guid_to_index.end())
        {
            return {};
        }

        const auto& entry = _assets[iter->second];
        if (entry->blob_size == 0)
        {
            return {};
        }

        return span<const byte>{_blob_data.data() + entry->blob_offset, static_cast<size_t>(entry->blob_size)};
    }

    auto asset_database::register_importer(unique_ptr<asset_importer> importer, string_view extension) -> void
    {
        _importers[string(extension)] = move(importer);
    }

    auto asset_database::register_asset_metadata(asset_metadata meta) -> guid
    {
        auto unique_id = guid{};
        do
        {
            unique_id = guid::generate_random_guid();
        } while (_metadata.contains(unique_id));

        _metadata.insert({move(unique_id), move(meta)});
        return unique_id;
    }

    auto asset_database::get_asset_metadata(guid asset_id) const -> optional<const asset_database::asset_metadata&>
    {
        if (auto iter = _metadata.find(asset_id); iter != _metadata.end())
        {
            return iter->second;
        }
        return none();
    }

    auto asset_database::_load_from_blobs(string_view source_path,
                                                            ecs::archetype_registry& registry) -> ecs::archetype_entity
    {
        auto src_it = _source_path_to_index.find(string(source_path));
        if (src_it == _source_path_to_index.end())
        {
            return ecs::tombstone;
        }

        const auto& src = _sources[src_it->second];

        // Collect all asset entries for this source
        vector<const asset_entry*> source_assets;
        for (const auto& asset : _assets)
        {
            if (asset->source_id == src->id)
            {
                source_assets.push_back(asset.get());
            }
        }

        if (source_assets.empty())
        {
            return ecs::tombstone;
        }

        // Depth-first resolve dependencies with visited set
        flat_unordered_map<guid, bool> visited;

        function<bool(const asset_entry&)> resolve_asset;
        resolve_asset = [&](const asset_entry& entry) -> bool {
            auto visit_it = visited.find(entry.id);
            if (visit_it != visited.end())
            {
                return visit_it->second;
            }

            // Mark as visiting (cycle detection)
            visited.insert({entry.id, false});

            // Resolve dependencies first
            for (const auto& dep_id : entry.dependencies)
            {
                const auto* dep_entry = find_by_guid(dep_id);
                if (dep_entry != nullptr)
                {
                    if (!resolve_asset(*dep_entry))
                    {
                        return false;
                    }
                }
            }

            // Deserialize this asset from its blob
            auto blob = get_blob(entry.id);
            if (blob.empty())
            {
                visited.insert({entry.id, true});
                return true;
            }

            const auto* type_info = _type_reg->find(entry.type);
            if (type_info != nullptr && type_info->deserializer)
            {
                type_info->deserializer(blob, entry.id, *this);
            }

            visited.insert({entry.id, true});
            return true;
        };

        // Resolve all assets for this source
        for (const auto* asset : source_assets)
        {
            resolve_asset(*asset);
        }

        // Find the entity hierarchy blob and reconstruct
        auto hierarchy_type = asset_type_id::of<entity_hierarchy>();
        for (const auto* asset : source_assets)
        {
            if (asset->type == hierarchy_type)
            {
                auto blob = get_blob(asset->id);
                if (!blob.empty())
                {
                    serialization::binary_archive blob_archive;
                    blob_archive.write(blob);
                    auto hierarchy =
                        serialization::serializer<serialization::binary_archive,
                                                  entity_hierarchy>::deserialize(blob_archive);

                    // Create an entity for each record
                    vector<ecs::archetype_entity> entities(hierarchy.records.size());
                    for (size_t i = 0; i < hierarchy.records.size(); ++i)
                    {
                        entities[i] = registry.create<>();
                    }

                    // Assign components and build relationships
                    for (size_t i = 0; i < hierarchy.records.size(); ++i)
                    {
                        auto& record = hierarchy.records[i];
                        auto ent = entities[i];

                        for (const auto& [type_hash, data] : record.components)
                        {
                            auto handler_it = _component_hash_to_index.find(type_hash);
                            if (handler_it != _component_hash_to_index.end())
                            {
                                _component_serializers[handler_it->second].deserialize(registry, ent, data);
                            }
                        }

                        registry.assign(ent, prefab_tag);

                        // Build parent-child relationships
                        for (auto child_idx : record.child_indices)
                        {
                            ecs::create_parent_child_relationship(registry, ent, entities[child_idx]);
                        }
                    }

                    return entities[hierarchy.root_index];
                }
            }
        }

        // If no entity hierarchy blob found, just return a placeholder
        auto root = registry.create<>();
        if (!registry.has<prefab_tag_t>(root))
        {
            registry.assign(root, prefab_tag);
        }
        return root;
    }

    ecs::archetype_entity asset_database::_load_via_import(string_view source_path,
                                                            ecs::archetype_registry& registry)
    {
        const auto* extension_it = search_last_of(source_path, '.');
        if (extension_it == source_path.end())
        {
            return ecs::tombstone;
        }

        auto importer_it = _importers.find(string(extension_it, source_path.end()));
        if (importer_it == _importers.end())
        {
            return ecs::tombstone;
        }

        // Snapshot asset count before importing so we can detect what the importer registered.
        const auto assets_before = _assets.size();

        const auto ent = importer_it->second->import(*this, source_path, registry);
        if (ent == ecs::tombstone)
        {
            return ecs::tombstone;
        }

        // Ensure the source is tracked regardless of whether the importer registered assets.
        _get_or_create_source(source_path);

        // If the importer didn't register any assets, create a placeholder entry so the
        // source is considered "cached" on subsequent runs and load() takes the blob path.
        if (_assets.size() == assets_before)
        {
            auto placeholder_type = asset_type_id::from_hash(0);
            register_asset(placeholder_type, source_path);
        }

        if (!registry.has<prefab_tag_t>(ent))
        {
            registry.assign(ent, prefab_tag);
        }

        // Serialize the entity hierarchy into a blob so it can be reconstructed
        // from the database on subsequent runs.
        {
            // Collect all entities into a flat list via depth-first walk
            vector<ecs::archetype_entity> all_entities;
            flat_unordered_map<ecs::archetype_entity, size_t> entity_to_index;

            function<void(ecs::archetype_entity)> collect;
            collect = [&](ecs::archetype_entity entity) {
                auto idx = all_entities.size();
                all_entities.push_back(entity);
                entity_to_index.insert({entity, idx});

                auto* rel =
                    registry.try_get<ecs::relationship_component<ecs::archetype_entity>>(entity);
                if (rel != nullptr && rel->first_child != ecs::tombstone)
                {
                    auto child = rel->first_child;
                    while (child != ecs::tombstone)
                    {
                        collect(child);
                        auto* child_rel =
                            registry.try_get<ecs::relationship_component<ecs::archetype_entity>>(child);
                        child = child_rel->next_sibling;
                    }
                }
            };
            collect(ent);

            // Build the hierarchy from the collected entities
            entity_hierarchy hierarchy;
            hierarchy.root_index = 0;

            for (size_t i = 0; i < all_entities.size(); ++i)
            {
                auto entity = all_entities[i];
                entity_hierarchy::entity_record record;

                // Serialize components using registered handlers
                for (const auto& handler : _component_serializers)
                {
                    vector<byte> bytes;
                    if (handler.serialize(registry, entity, bytes))
                    {
                        record.components.push_back({handler.type_hash, tempest::move(bytes)});
                    }
                }

                // Record child indices
                auto* rel =
                    registry.try_get<ecs::relationship_component<ecs::archetype_entity>>(entity);
                if (rel != nullptr && rel->first_child != ecs::tombstone)
                {
                    auto child = rel->first_child;
                    while (child != ecs::tombstone)
                    {
                        record.child_indices.push_back(entity_to_index[child]);
                        auto* child_rel =
                            registry.try_get<ecs::relationship_component<ecs::archetype_entity>>(child);
                        child = child_rel->next_sibling;
                    }
                }

                hierarchy.records.push_back(tempest::move(record));
            }

            // Serialize the hierarchy and store it as a blob
            serialization::binary_archive hier_archive;
            serialization::serializer<serialization::binary_archive, entity_hierarchy>::serialize(hier_archive,
                                                                                                  hierarchy);
            auto hier_blob = hier_archive.read(hier_archive.written_size());
            auto hier_id = register_asset(asset_type_id::of<entity_hierarchy>(), source_path);
            store_blob(hier_id, hier_blob);
        }

        return ent;
    }

    source_entry& asset_database::_get_or_create_source(string_view source_path)
    {
        auto iter = _source_path_to_index.find(string(source_path));
        if (iter != _source_path_to_index.end())
        {
            return *_sources[iter->second];
        }

        auto new_id = guid::generate_random_guid();
        auto entry = make_unique<source_entry>(source_entry{
            .id = new_id,
            .source_path = string(source_path),
            .source_hash = {},
        });

        auto index = _sources.size();
        string path_copy = entry->source_path;
        _source_path_to_index.insert({tempest::move(path_copy), index});
        _source_id_to_index.insert({new_id, index});
        auto& ref = *entry;
        _sources.push_back(tempest::move(entry));
        return ref;
    }
} // namespace tempest::assets