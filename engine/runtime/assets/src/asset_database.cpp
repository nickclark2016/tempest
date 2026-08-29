#include <tempest/asset_database.hpp>
#include <tempest/asset_serializers.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/logger.hpp>
#include <tempest/serial.hpp>

#include <fstream>

namespace tempest::assets
{
    namespace
    {
        constexpr array<uint8_t, 4> db_magic = {'T', 'E', 'B', 'F'};
        constexpr uint16_t db_version = 2;

        auto normalize_path_str(string_view input) -> string
        {
            auto s = string(input);
            for (auto& ch : s)
            {
                if (ch == '\\')
                {
                    ch = '/';
                }
            }
            while (s.size() >= 2 && tempest::starts_with(s, "./"))
            {
                auto sub = tempest::substr(s, 2, s.size() - 2);
                s = string(sub);
            }
            while (s.size() > 1 && s[0] == '/')
            {
                auto sub = tempest::substr(s, 1, s.size() - 1);
                s = string(sub);
            }
            while (s.size() > 1 && tempest::ends_with(s, '/'))
            {
                s.pop_back();
            }
            return s;
        }
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
        _blob_chunks.clear();
        _current_chunk_capacity = 0;
        _current_chunk_used = 0;
        _cached_blobs.clear();
        _basename_to_relative_path.clear();
        _relative_path_to_source_path.clear();
        _dirty = false;

        // Try to read existing database file
        auto file = std::ifstream(string(db_path).c_str(), std::ios::binary);
        if (!file.is_open())
        {
            return;
        }

        // Read binary header
        auto header = serialization::binary_header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file || header.magic != db_magic || header.version != db_version)
        {
            return;
        }

        // Read the rest of the file into a buffer
        const auto data_size = static_cast<size_t>(header.data_length);
        auto buffer = vector<byte>{};
        unsafe::resize_no_init(buffer, data_size);
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(data_size));
        file.close();

        serialization::binary_archive archive;
        archive.write(tempest::move(buffer));

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
            auto path_copy = entry->source_path;
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
            auto meta = serialization::serializer<serialization::binary_archive,
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

        // Read blob section into initial chunk
        auto blob_size = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);
        if (blob_size > 0)
        {
            auto chunk0 = make_unique<vector<byte>>();
            unsafe::resize_no_init(*chunk0, static_cast<size_t>(blob_size));
            auto blob_span = archive.read(static_cast<size_t>(blob_size));
            tempest::memcpy(chunk0->data(), blob_span.data(), static_cast<size_t>(blob_size));

            for (const auto& asset : _assets)
            {
                if (asset->blob_size > 0 && asset->blob_offset + asset->blob_size <= blob_size)
                {
                    _cached_blobs[asset->id] = span<const byte>{chunk0->data() + asset->blob_offset,
                                                                 static_cast<size_t>(asset->blob_size)};
                }
            }
            _blob_chunks.push_back(tempest::move(chunk0));
        }
    }

    auto asset_database::save() const -> bool
    {
        if (_db_path.empty())
        {
            return false;
        }

        if (!_dirty && filesystem::exists(_db_path))
        {
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
            serialization::serializer<serialization::binary_archive, string>::serialize(archive, entry.canonical_name);
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

        // Compute total blob size and contiguous compacted offsets
        uint64_t total_blob_size = 0;
        for (const auto& asset : _assets)
        {
            asset->blob_offset = total_blob_size;
            auto it = _cached_blobs.find(asset->id);
            if (it != _cached_blobs.end() && !it->second.empty())
            {
                asset->blob_size = it->second.size();
            }
            total_blob_size += asset->blob_size;
        }

        // Write asset table
        serialization::serializer<serialization::binary_archive, uint64_t>::serialize(
            archive, static_cast<uint64_t>(_assets.size()));
        for (const auto& asset : _assets)
        {
            serialization::serializer<serialization::binary_archive, guid>::serialize(archive, asset->id);
            serialization::serializer<serialization::binary_archive, uint64_t>::serialize(
                archive, static_cast<uint64_t>(asset->type.hash()));
            serialization::serializer<serialization::binary_archive, uint64_t>::serialize(archive, asset->blob_offset);
            serialization::serializer<serialization::binary_archive, uint64_t>::serialize(archive, asset->blob_size);
            serialization::serializer<serialization::binary_archive, guid>::serialize(archive, asset->source_id);
            serialization::serializer<serialization::binary_archive, vector<guid>>::serialize(archive,
                                                                                              asset->dependencies);
            serialization::serializer<serialization::binary_archive, flat_unordered_map<string, string>>::serialize(
                archive, asset->user_metadata);
        }

        // Write blob section (coalesced into contiguous stream)
        serialization::serializer<serialization::binary_archive, uint64_t>::serialize(
            archive, total_blob_size);
        for (const auto& asset : _assets)
        {
            auto it = _cached_blobs.find(asset->id);
            if (it != _cached_blobs.end() && !it->second.empty())
            {
                archive.write(it->second);
            }
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
            return false;
        }

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        auto all_data = archive.read(total_size);
        file.write(reinterpret_cast<const char*>(all_data.data()), static_cast<std::streamsize>(all_data.size()));
        file.close();

        return true;
    }

    auto asset_database::load(string_view source_path, ecs::archetype_registry& registry) -> ecs::entity
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
        auto normalized = normalize_path_str(path);

        // Find the source entry for this path
        auto src_it = _source_path_to_index.find(normalized);
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

    auto asset_database::mount_root(string_view root_path, int32_t priority) -> void
    {
        auto normalized = normalize_path_str(root_path);
        if (normalized.empty())
        {
            normalized = ".";
        }

        for (auto& mp : _mount_roots)
        {
            if (mp.path == normalized)
            {
                mp.priority = priority;
                for (size_t i = 1; i < _mount_roots.size(); ++i)
                {
                    auto key = tempest::move(_mount_roots[i]);
                    size_t j = i;
                    while (j > 0 && _mount_roots[j - 1].priority < key.priority)
                    {
                        _mount_roots[j] = tempest::move(_mount_roots[j - 1]);
                        --j;
                    }
                    _mount_roots[j] = tempest::move(key);
                }
                return;
            }
        }

        _mount_roots.push_back(mount_point{
            .path = tempest::move(normalized),
            .priority = priority,
        });

        for (size_t i = 1; i < _mount_roots.size(); ++i)
        {
            auto key = tempest::move(_mount_roots[i]);
            size_t j = i;
            while (j > 0 && _mount_roots[j - 1].priority < key.priority)
            {
                _mount_roots[j] = tempest::move(_mount_roots[j - 1]);
                --j;
            }
            _mount_roots[j] = tempest::move(key);
        }
    }

    auto asset_database::unmount_root(string_view root_path) -> void
    {
        auto normalized = normalize_path_str(root_path);
        for (auto it = _mount_roots.begin(); it != _mount_roots.end(); ++it)
        {
            if (it->path == normalized)
            {
                _mount_roots.erase(it);
                break;
            }
        }
    }

    auto asset_database::clear_mount_roots() -> void
    {
        _mount_roots.clear();
    }

    auto asset_database::get_mount_roots() const -> span<const mount_point>
    {
        return span<const mount_point>{_mount_roots.data(), _mount_roots.size()};
    }

    auto asset_database::scan_and_index() -> void
    {
        for (const auto& mp : _mount_roots)
        {
            auto root_path = filesystem::path(mp.path.c_str());
            if (!filesystem::exists(root_path) || !filesystem::is_directory(root_path))
            {
                continue;
            }

            auto scan_dir = [&](auto& self, const filesystem::path& dir) -> void {
                for (auto it = filesystem::directory_iterator(dir); it != filesystem::directory_iterator(); ++it)
                {
                    if (it->is_directory())
                    {
                        self(self, it->path());
                    }
                    else if (it->is_regular_file())
                    {
                        auto rel = filesystem::relative(it->path(), root_path);
                        auto rel_str = normalize_path_str(rel.generic_string());
                        if (rel_str.empty() || rel_str == ".")
                        {
                            continue;
                        }

                        auto filename_str = string(it->path().filename().string());

                        // If not registered in _sources yet, register it
                        auto src_it = _source_path_to_index.find(rel_str);
                        if (src_it == _source_path_to_index.end())
                        {
                            _get_or_create_source(rel_str);

                            // If shader file (.spv or .slang)
                            if (tempest::ends_with(rel_str, ".spv") || tempest::ends_with(rel_str, ".slang"))
                            {
                                register_asset(asset_type_id::of<shader_asset>(), rel_str);
                            }
                        }

                        // Populate basename index if not already present (higher priority mount root wins)
                        if (!_basename_to_relative_path.contains(filename_str))
                        {
                            _basename_to_relative_path.insert({filename_str, rel_str});
                        }

                        // Map .spv back to .slang if .slang is encountered or discoverable
                        if (tempest::ends_with(rel_str, ".vert.spv") || tempest::ends_with(rel_str, ".frag.spv") ||
                            tempest::ends_with(rel_str, ".comp.spv"))
                        {
                            auto dot_idx = string::npos;
                            for (size_t i = 0; i < filename_str.size(); ++i)
                            {
                                if (filename_str[i] == '.')
                                {
                                    dot_idx = i;
                                    break;
                                }
                            }
                            if (dot_idx != string::npos)
                            {
                                auto base_stem = string(tempest::substr(filename_str, 0, dot_idx));
                                auto slang_name = base_stem;
                                slang_name.append(".slang");
                                _relative_path_to_source_path[rel_str] = slang_name;
                            }
                        }
                    }
                }
            };

            scan_dir(scan_dir, root_path);
        }
    }

    auto asset_database::resolve_disk_path(string_view relative_path) const -> optional<string>
    {
        auto normalized = normalize_path_str(relative_path);
        if (normalized.empty())
        {
            return nullopt;
        }

        auto direct_path = filesystem::path(normalized.c_str());
        if (direct_path.is_absolute() && filesystem::exists(direct_path))
        {
            return direct_path.string();
        }

        for (const auto& mp : _mount_roots)
        {
            auto candidate = filesystem::path(mp.path.c_str()) / direct_path;
            if (filesystem::exists(candidate))
            {
                return candidate.string();
            }
        }

        if (filesystem::exists(direct_path))
        {
            return direct_path.string();
        }

        return nullopt;
    }

    auto asset_database::find_asset(string_view path_or_name) const -> const asset_entry*
    {
        auto normalized = normalize_path_str(path_or_name);
        if (normalized.empty())
        {
            return nullptr;
        }

        // 1. Direct path lookup
        const auto* entry = find_by_path(normalized);
        if (entry != nullptr)
        {
            return entry;
        }

        // 2. Basename index lookup
        auto base_it = _basename_to_relative_path.find(normalized);
        if (base_it != _basename_to_relative_path.end())
        {
            entry = find_by_path(base_it->second);
            if (entry != nullptr)
            {
                return entry;
            }
        }

        // 3. Fallback: try prepending or stripping "shaders/"
        if (!tempest::starts_with(normalized, "shaders/"))
        {
            auto with_shaders = string("shaders/");
            with_shaders.append(normalized.data(), normalized.size());
            entry = find_by_path(with_shaders);
            if (entry != nullptr)
            {
                return entry;
            }
        }
        else if (normalized.size() > 8)
        {
            auto without_shaders = string(tempest::substr(normalized, 8, normalized.size() - 8));
            entry = find_by_path(without_shaders);
            if (entry != nullptr)
            {
                return entry;
            }
        }

        // 4. On-demand search across mounted roots
        auto disk_path = resolve_disk_path(normalized);
        auto asset_key = normalized;
        if (!disk_path.has_value() && !tempest::starts_with(normalized, "shaders/"))
        {
            auto with_shaders = string("shaders/");
            with_shaders.append(normalized.data(), normalized.size());
            disk_path = resolve_disk_path(with_shaders);
            if (disk_path.has_value())
            {
                asset_key = with_shaders;
            }
        }

        if (disk_path.has_value())
        {
            auto* mutable_this = const_cast<asset_database*>(this);
            auto type_id = (tempest::ends_with(asset_key, ".spv") || tempest::ends_with(asset_key, ".slang"))
                               ? asset_type_id::of<shader_asset>()
                               : asset_type_id::from_hash(0);
            mutable_this->register_asset(type_id, asset_key);

            auto fs_path = filesystem::path(asset_key.c_str());
            auto filename_str = string(fs_path.filename().string());
            if (!mutable_this->_basename_to_relative_path.contains(filename_str))
            {
                mutable_this->_basename_to_relative_path.insert({filename_str, asset_key});
            }

            return find_by_path(asset_key);
        }

        return nullptr;
    }

    auto asset_database::find_source_by_id(const guid& source_id) const -> const source_entry*
    {
        auto it = _source_id_to_index.find(source_id);
        if (it != _source_id_to_index.end())
        {
            return _sources[it->second].get();
        }
        return nullptr;
    }

    auto asset_database::resolve_source_path(string_view path_or_name) const -> optional<string>
    {
        auto normalized = normalize_path_str(path_or_name);
        if (normalized.empty())
        {
            return nullopt;
        }

        auto it = _relative_path_to_source_path.find(normalized);
        if (it != _relative_path_to_source_path.end())
        {
            return it->second;
        }

        auto base_it = _basename_to_relative_path.find(normalized);
        if (base_it != _basename_to_relative_path.end())
        {
            auto rel_it = _relative_path_to_source_path.find(base_it->second);
            if (rel_it != _relative_path_to_source_path.end())
            {
                return rel_it->second;
            }
        }

        if (tempest::ends_with(normalized, ".slang"))
        {
            return normalized;
        }

        if (tempest::ends_with(normalized, ".spv"))
        {
            auto fs_path = filesystem::path(normalized.c_str());
            auto filename = fs_path.filename().string();
            auto dot_pos = string::npos;
            for (size_t i = 0; i < filename.size(); ++i)
            {
                if (filename[i] == '.')
                {
                    dot_pos = i;
                    break;
                }
            }
            if (dot_pos != string::npos)
            {
                auto slang_name = string(tempest::substr(filename, 0, dot_pos));
                slang_name.append(".slang");
                if (_source_path_to_index.contains(slang_name) || _basename_to_relative_path.contains(slang_name) ||
                    resolve_disk_path(slang_name).has_value())
                {
                    return slang_name;
                }
            }
        }

        return nullopt;
    }

    auto asset_database::notify_file_changed(string_view disk_path) -> bool
    {
        auto normalized = normalize_path_str(disk_path);
        if (normalized.empty())
        {
            return false;
        }

        auto fs_path = filesystem::path(normalized.c_str());
        auto filename = fs_path.filename().string();

        bool any_updated = false;

        for (auto& src : _sources)
        {
            bool match = false;
            if (src->source_path == normalized || tempest::ends_with(src->source_path, normalized))
            {
                match = true;
            }
            else
            {
                auto src_filename = filesystem::path(src->source_path.c_str()).filename().string();
                if (src_filename == filename)
                {
                    match = true;
                }
            }

            if (match)
            {
                for (auto& asset : _assets)
                {
                    if (asset->source_id == src->id)
                    {
                        _cached_blobs.erase(asset->id);
                        asset->blob_size = 0;
                        asset->blob_offset = 0;
                        any_updated = true;
                    }
                }
            }
        }

        if (any_updated)
        {
            _dirty = true;
        }

        return any_updated;
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

        _dirty = true;

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
            return;
        }

        auto& entry = _assets[iter->second];
        entry->blob_size = data.size();

        constexpr size_t default_chunk_size = 64 * 1024;
        const byte* dest_ptr = nullptr;

        if (data.size() > default_chunk_size)
        {
            // Large asset: dedicated chunk
            auto dedicated = make_unique<vector<byte>>();
            unsafe::resize_no_init(*dedicated, data.size());
            tempest::memcpy(dedicated->data(), data.data(), data.size());
            dest_ptr = dedicated->data();
            _blob_chunks.push_back(tempest::move(dedicated));
        }
        else
        {
            // Small asset: pack into current chunk
            if (_blob_chunks.empty() || _current_chunk_capacity - _current_chunk_used < data.size())
            {
                auto new_chunk = make_unique<vector<byte>>();
                unsafe::resize_no_init(*new_chunk, default_chunk_size);
                _current_chunk_capacity = default_chunk_size;
                _current_chunk_used = 0;
                _blob_chunks.push_back(tempest::move(new_chunk));
            }

            auto& active_chunk = _blob_chunks.back();
            dest_ptr = active_chunk->data() + _current_chunk_used;
            tempest::memcpy(const_cast<byte*>(dest_ptr), data.data(), data.size());
            _current_chunk_used += data.size();
        }

        _cached_blobs[asset_id] = span<const byte>{dest_ptr, data.size()};
        _dirty = true;
    }

    auto asset_database::get_blob(const guid& asset_id) const -> span<const byte>
    {
        auto it = _cached_blobs.find(asset_id);
        if (it != _cached_blobs.end() && !it->second.empty())
        {
            return it->second;
        }

        auto iter = _asset_guid_to_index.find(asset_id);
        if (iter == _asset_guid_to_index.end())
        {
            return {};
        }

        const auto& entry = _assets[iter->second];
        auto src_it = _source_id_to_index.find(entry->source_id);
        if (src_it != _source_id_to_index.end())
        {
            const auto& src = _sources[src_it->second];
            auto disk_path = resolve_disk_path(src->source_path);
            if (disk_path.has_value())
            {
                auto bytes = core::read_bytes(string_view{disk_path->c_str(), disk_path->size()});
                if (!bytes.empty())
                {
                    auto* mutable_this = const_cast<asset_database*>(this);
                    mutable_this->store_blob(asset_id, span<const byte>{bytes.data(), bytes.size()});
                    return mutable_this->_cached_blobs[asset_id];
                }
            }
        }

        return {};
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

    auto asset_database::_load_from_blobs(string_view source_path, ecs::archetype_registry& registry) -> ecs::entity
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

        auto resolve_asset = [&](const asset_entry& entry, const auto& resolver) -> bool {
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
                    if (!resolver(*dep_entry, resolver))
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
            resolve_asset(*asset, resolve_asset);
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
                        serialization::serializer<serialization::binary_archive, entity_hierarchy>::deserialize(
                            blob_archive);

                    // Create an entity for each record
                    vector<ecs::entity> entities(hierarchy.records.size());
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

    ecs::entity asset_database::_load_via_import(string_view source_path, ecs::archetype_registry& registry)
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
            vector<ecs::entity> all_entities;
            flat_unordered_map<ecs::entity, size_t> entity_to_index;

            function<void(ecs::entity)> collect;
            collect = [&](ecs::entity entity) {
                auto idx = all_entities.size();
                all_entities.push_back(entity);
                entity_to_index.insert({entity, idx});

                auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(entity);
                if (rel != nullptr && rel->first_child != ecs::tombstone)
                {
                    auto child = rel->first_child;
                    while (child != ecs::tombstone)
                    {
                        collect(child);
                        auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
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
                auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(entity);
                if (rel != nullptr && rel->first_child != ecs::tombstone)
                {
                    auto child = rel->first_child;
                    while (child != ecs::tombstone)
                    {
                        record.child_indices.push_back(entity_to_index[child]);
                        auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
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
        auto normalized = normalize_path_str(source_path);
        auto iter = _source_path_to_index.find(normalized);
        if (iter != _source_path_to_index.end())
        {
            return *_sources[iter->second];
        }

        auto new_id = guid::generate_random_guid();
        auto entry = make_unique<source_entry>(source_entry{
            .id = new_id,
            .source_path = normalized,
            .source_hash = {},
        });

        auto index = _sources.size();
        string path_copy = entry->source_path;
        _source_path_to_index.insert({tempest::move(path_copy), index});
        _source_id_to_index.insert({new_id, index});
        auto& ref = *entry;
        _sources.push_back(tempest::move(entry));

        _dirty = true;

        return ref;
    }
} // namespace tempest::assets
