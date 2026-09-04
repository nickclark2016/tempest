#include <tempest/asset_database.hpp>
#include <tempest/asset_serializers.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/logger.hpp>
#include <tempest/serial.hpp>

#include <filesystem>
#include <fstream>

namespace tempest::assets
{
    namespace
    {
        constexpr array<uint8_t, 4> db_magic = {'T', 'E', 'B', 'F'};
        constexpr uint16_t db_version = 3;

        auto get_file_last_write_time(string_view disk_path) noexcept -> uint64_t
        {
            if (disk_path.empty())
            {
                return 0;
            }
            std::error_code ec;
            auto ftime = std::filesystem::last_write_time(
                std::filesystem::path(std::string(disk_path.data(), disk_path.size())), ec);
            if (ec)
            {
                return 0;
            }
            return static_cast<uint64_t>(ftime.time_since_epoch().count());
        }

        auto normalize_extension_str(string_view input) -> string
        {
            auto s = string(input);
            if (s.empty())
            {
                return s;
            }
            if (s[0] != '.')
            {
                auto with_dot = string(".");
                with_dot.append(s.data(), s.size());
                return with_dot;
            }
            return s;
        }

        struct parsed_alias
        {
            bool is_aliased{false};
            string_view alias{};
            string_view subpath{};
        };

        auto parse_alias(string_view path) -> parsed_alias
        {
            if (path.size() >= 2 && path[0] == '@')
            {
                auto sep_pos = string::npos;
                for (size_t i = 1; i < path.size(); ++i)
                {
                    if (path[i] == '/' || path[i] == '\\')
                    {
                        sep_pos = i;
                        break;
                    }
                }

                if (sep_pos != string::npos)
                {
                    auto alias = tempest::substr(path, 1, sep_pos - 1);
                    if (alias.empty())
                    {
                        return parsed_alias{.is_aliased = false};
                    }
                    auto subpath = tempest::substr(path, sep_pos + 1, path.size() - (sep_pos + 1));
                    return parsed_alias{
                        .is_aliased = true,
                        .alias = alias,
                        .subpath = subpath,
                    };
                }
                else
                {
                    auto alias = tempest::substr(path, 1, path.size() - 1);
                    if (alias.empty())
                    {
                        return parsed_alias{.is_aliased = false};
                    }
                    return parsed_alias{
                        .is_aliased = true,
                        .alias = alias,
                        .subpath = string_view{},
                    };
                }
            }
            return parsed_alias{.is_aliased = false};
        }

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

        auto is_path_ignored(string_view path, span<const string> ignored_dirs, span<const string> ignored_exts) -> bool
        {
            if (path.empty())
            {
                return false;
            }

            // 1. Check extension
            auto dot_pos = string::npos;
            for (size_t i = path.size(); i > 0; --i)
            {
                if (path[i - 1] == '.')
                {
                    dot_pos = i - 1;
                    break;
                }
            }
            if (dot_pos != string::npos)
            {
                auto ext = tempest::substr(path, dot_pos, path.size() - dot_pos);
                for (const auto& ignored_ext : ignored_exts)
                {
                    if (ext == ignored_ext)
                    {
                        return true;
                    }
                }
            }

            // 2. Check path segments
            size_t start = 0;
            while (start < path.size())
            {
                size_t end = start;
                while (end < path.size() && path[end] != '/' && path[end] != '\\')
                {
                    ++end;
                }
                auto segment = tempest::substr(path, start, end - start);
                if (!segment.empty())
                {
                    if (segment.size() > 1 && segment[0] == '.')
                    {
                        return true;
                    }
                    for (const auto& ignored_dir : ignored_dirs)
                    {
                        if (segment == ignored_dir)
                        {
                            return true;
                        }
                    }
                }
                start = end + 1;
            }

            return false;
        }

        constexpr uint32_t sha256_k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

        inline auto rotr32(uint32_t x, uint32_t n) noexcept -> uint32_t
        {
            return (x >> n) | (x << (32 - n));
        }

        inline auto sha256_ch(uint32_t x, uint32_t y, uint32_t z) noexcept -> uint32_t
        {
            return (x & y) ^ (~x & z);
        }

        inline auto sha256_maj(uint32_t x, uint32_t y, uint32_t z) noexcept -> uint32_t
        {
            return (x & y) ^ (x & z) ^ (y & z);
        }

        inline auto sha256_sig0(uint32_t x) noexcept -> uint32_t
        {
            return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22);
        }

        inline auto sha256_sig1(uint32_t x) noexcept -> uint32_t
        {
            return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25);
        }

        inline auto sha256_gam0(uint32_t x) noexcept -> uint32_t
        {
            return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3);
        }

        inline auto sha256_gam1(uint32_t x) noexcept -> uint32_t
        {
            return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10);
        }

        auto sha256_transform(uint32_t state[8], const uint8_t block[64]) noexcept -> void
        {
            uint32_t w[64];
            for (size_t i = 0; i < 16; ++i)
            {
                w[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 24) |
                       (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | (static_cast<uint32_t>(block[i * 4 + 3]) << 0);
            }
            for (size_t i = 16; i < 64; ++i)
            {
                w[i] = sha256_gam1(w[i - 2]) + w[i - 7] + sha256_gam0(w[i - 15]) + w[i - 16];
            }

            uint32_t a = state[0];
            uint32_t b = state[1];
            uint32_t c = state[2];
            uint32_t d = state[3];
            uint32_t e = state[4];
            uint32_t f = state[5];
            uint32_t g = state[6];
            uint32_t h = state[7];

            for (size_t i = 0; i < 64; ++i)
            {
                uint32_t t1 = h + sha256_sig1(e) + sha256_ch(e, f, g) + sha256_k[i] + w[i];
                uint32_t t2 = sha256_sig0(a) + sha256_maj(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }

            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            state[4] += e;
            state[5] += f;
            state[6] += g;
            state[7] += h;
        }

        auto paths_match(string_view disk_path_norm, string_view target_path_norm) -> bool
        {
            if (disk_path_norm == target_path_norm)
            {
                return true;
            }

            // Case-insensitive exact match
            if (disk_path_norm.size() == target_path_norm.size())
            {
                bool eq = true;
                for (size_t i = 0; i < disk_path_norm.size(); ++i)
                {
                    char c1 = (disk_path_norm[i] >= 'A' && disk_path_norm[i] <= 'Z') ? (disk_path_norm[i] + 32)
                                                                                     : disk_path_norm[i];
                    char c2 = (target_path_norm[i] >= 'A' && target_path_norm[i] <= 'Z') ? (target_path_norm[i] + 32)
                                                                                         : target_path_norm[i];
                    if (c1 != c2)
                    {
                        eq = false;
                        break;
                    }
                }
                if (eq)
                {
                    return true;
                }
            }

            // Check if disk_path_norm ends with target_path_norm
            if (disk_path_norm.size() > target_path_norm.size())
            {
                auto suffix = tempest::substr(disk_path_norm, disk_path_norm.size() - target_path_norm.size(),
                                              target_path_norm.size());
                if (suffix == target_path_norm)
                {
                    size_t prefix_len = disk_path_norm.size() - target_path_norm.size();
                    if (prefix_len == 0 || disk_path_norm[prefix_len - 1] == '/' ||
                        disk_path_norm[prefix_len - 1] == '\\')
                    {
                        return true;
                    }
                }
            }

            // Check if target_path_norm ends with disk_path_norm
            if (target_path_norm.size() > disk_path_norm.size())
            {
                auto suffix = tempest::substr(target_path_norm, target_path_norm.size() - disk_path_norm.size(),
                                              disk_path_norm.size());
                if (suffix == disk_path_norm)
                {
                    size_t prefix_len = target_path_norm.size() - disk_path_norm.size();
                    if (prefix_len == 0 || target_path_norm[prefix_len - 1] == '/' ||
                        target_path_norm[prefix_len - 1] == '\\')
                    {
                        return true;
                    }
                }
            }

            return false;
        }
    } // namespace

    auto content_hash::compute(span<const byte> bytes) noexcept -> content_hash
    {
        uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                             0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

        const auto* data = reinterpret_cast<const uint8_t*>(bytes.data());
        size_t len = bytes.size();
        size_t offset = 0;

        uint8_t block[64];

        while (len >= 64)
        {
            sha256_transform(state, data + offset);
            offset += 64;
            len -= 64;
        }

        for (size_t b = 0; b < 64; ++b)
        {
            block[b] = 0;
        }
        if (len > 0)
        {
            tempest::memcpy(block, data + offset, len);
        }
        block[len] = 0x80;

        if (len >= 56)
        {
            sha256_transform(state, block);
            for (size_t b = 0; b < 64; ++b)
            {
                block[b] = 0;
            }
        }

        uint64_t total_bits = static_cast<uint64_t>(bytes.size()) * 8ULL;
        for (size_t k = 0; k < 8; ++k)
        {
            block[63 - k] = static_cast<uint8_t>((total_bits >> (k * 8)) & 0xFF);
        }
        sha256_transform(state, block);

        content_hash result;
        for (size_t j = 0; j < 8; ++j)
        {
            result.data[j * 4 + 0] = static_cast<byte>((state[j] >> 24) & 0xFF);
            result.data[j * 4 + 1] = static_cast<byte>((state[j] >> 16) & 0xFF);
            result.data[j * 4 + 2] = static_cast<byte>((state[j] >> 8) & 0xFF);
            result.data[j * 4 + 3] = static_cast<byte>((state[j] >> 0) & 0xFF);
        }
        return result;
    }

    asset_database::asset_database(asset_type_registry* type_reg) noexcept : _type_reg{type_reg}
    {
        _ignored_directories.push_back(".git");
        _ignored_directories.push_back(".cache");
        _ignored_directories.push_back(".vscode");
        _ignored_directories.push_back("bin");
        _ignored_directories.push_back("bin-int");
        _ignored_directories.push_back("build");
        _ignored_directories.push_back("TempestCache");
        _ignored_directories.push_back("vendor");
        _ignored_directories.push_back("dependencies");
        _ignored_directories.push_back(".agents");

        _ignored_extensions.push_back(".tmp");
        _ignored_extensions.push_back(".bak");
        _ignored_extensions.push_back(".pdb");
        _ignored_extensions.push_back(".idb");
        _ignored_extensions.push_back(".ilk");
        _ignored_extensions.push_back(".o");
        _ignored_extensions.push_back(".obj");
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
        _packing_chunk.reset();
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
            auto src_mtime = serialization::serializer<serialization::binary_archive, uint64_t>::deserialize(archive);

            auto entry = make_unique<source_entry>(source_entry{
                .id = src_id,
                .source_path = tempest::move(src_path),
                .source_hash = src_hash,
                .last_modified_time = src_mtime,
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
                    _cached_blobs[asset->id] =
                        span<const byte>{chunk0->data() + asset->blob_offset, static_cast<size_t>(asset->blob_size)};
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

        serialization::binary_archive archive;
        if (total_blob_size > 0)
        {
            archive.reserve(static_cast<size_t>(total_blob_size));
        }

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
            serialization::serializer<serialization::binary_archive, uint64_t>::serialize(archive,
                                                                                          src->last_modified_time);
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
        serialization::serializer<serialization::binary_archive, uint64_t>::serialize(archive, total_blob_size);
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
        auto parsed = parse_alias(source_path);
        if (parsed.is_aliased)
        {
            auto normalized_sub = normalize_path_str(parsed.subpath);
            auto aliased_key = string("@");
            aliased_key.append(parsed.alias.data(), parsed.alias.size());
            if (!normalized_sub.empty())
            {
                aliased_key.append("/");
                aliased_key.append(normalized_sub);
            }

            auto disk_path = resolve_disk_path(aliased_key);
            if (disk_path.has_value())
            {
                auto norm_disk = normalize_path_str(disk_path.value());
                auto disk_mtime = get_file_last_write_time(disk_path.value());

                auto path_it = _source_path_to_index.find(aliased_key);
                if (path_it == _source_path_to_index.end())
                {
                    path_it = _source_path_to_index.find(norm_disk);
                }

                if (path_it != _source_path_to_index.end())
                {
                    auto& src = *_sources[path_it->second];
                    if (src.last_modified_time != 0 && disk_mtime != 0 && src.last_modified_time == disk_mtime)
                    {
                        return _load_from_blobs(src.source_path, registry);
                    }
                }

                auto ent = _load_via_import(disk_path.value(), registry);
                if (ent != ecs::tombstone)
                {
                    auto reg_disk_it = _source_path_to_index.find(norm_disk);
                    if (reg_disk_it != _source_path_to_index.end())
                    {
                        _sources[reg_disk_it->second]->last_modified_time = disk_mtime;
                        _source_path_to_index.insert({aliased_key, reg_disk_it->second});
                    }
                    auto alias_it = _source_path_to_index.find(aliased_key);
                    if (alias_it != _source_path_to_index.end())
                    {
                        _sources[alias_it->second]->last_modified_time = disk_mtime;
                    }
                }
                return ent;
            }

            auto path_it = _source_path_to_index.find(aliased_key);
            if (path_it != _source_path_to_index.end())
            {
                return _load_from_blobs(aliased_key, registry);
            }

            return ecs::tombstone;
        }

        auto norm_path = normalize_path_str(source_path);
        auto disk_path = resolve_disk_path(norm_path);
        if (disk_path.has_value())
        {
            auto disk_mtime = get_file_last_write_time(disk_path.value());

            auto path_it = _source_path_to_index.find(norm_path);
            if (path_it == _source_path_to_index.end())
            {
                path_it = _source_path_to_index.find(normalize_path_str(disk_path.value()));
            }

            if (path_it != _source_path_to_index.end())
            {
                auto& src = *_sources[path_it->second];
                if (src.last_modified_time != 0 && disk_mtime != 0 && src.last_modified_time == disk_mtime)
                {
                    return _load_from_blobs(src.source_path, registry);
                }
            }

            auto ent = _load_via_import(disk_path.value(), registry);
            if (ent != ecs::tombstone)
            {
                auto reg_it = _source_path_to_index.find(norm_path);
                if (reg_it != _source_path_to_index.end())
                {
                    _sources[reg_it->second]->last_modified_time = disk_mtime;
                }
                auto reg_disk_it = _source_path_to_index.find(normalize_path_str(disk_path.value()));
                if (reg_disk_it != _source_path_to_index.end())
                {
                    _sources[reg_disk_it->second]->last_modified_time = disk_mtime;
                }
            }
            return ent;
        }

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

    auto asset_database::mount_root(string_view root_path, int32_t priority, string_view alias) -> void
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
                mp.alias = string(alias);
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
            .alias = string(alias),
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

    auto asset_database::add_ignored_directory(string_view dir_name) -> void
    {
        if (dir_name.empty())
        {
            return;
        }
        auto normalized = normalize_path_str(dir_name);
        for (const auto& existing : _ignored_directories)
        {
            if (existing == normalized)
            {
                return;
            }
        }
        _ignored_directories.push_back(tempest::move(normalized));
    }

    auto asset_database::add_ignored_extension(string_view extension) -> void
    {
        if (extension.empty())
        {
            return;
        }
        auto normalized = normalize_extension_str(extension);
        for (const auto& existing : _ignored_extensions)
        {
            if (existing == normalized)
            {
                return;
            }
        }
        _ignored_extensions.push_back(tempest::move(normalized));
    }

    auto asset_database::clear_ignores() -> void
    {
        _ignored_directories.clear();
        _ignored_extensions.clear();
    }

    auto asset_database::get_ignored_directories() const -> span<const string>
    {
        return span<const string>{_ignored_directories.data(), _ignored_directories.size()};
    }

    auto asset_database::get_ignored_extensions() const -> span<const string>
    {
        return span<const string>{_ignored_extensions.data(), _ignored_extensions.size()};
    }

    auto asset_database::scan_and_index(const scan_options& options) -> void
    {
        auto normalized_whitelist = vector<string>{};
        for (const auto& ext : options.extension_whitelist)
        {
            if (!ext.empty())
            {
                normalized_whitelist.push_back(normalize_extension_str(ext));
            }
        }

        for (const auto& mp : _mount_roots)
        {
            auto root_path = filesystem::path(mp.path.c_str());
            if (!filesystem::exists(root_path) || !filesystem::is_directory(root_path))
            {
                continue;
            }

            auto scan_dir = [&](auto& self, const filesystem::path& dir, string_view rel_accum) -> void {
                for (auto it = filesystem::directory_iterator(dir); it != filesystem::directory_iterator(); ++it)
                {
                    auto filename_str = string(it->path().filename().string());
                    if (filename_str.empty() || filename_str == "." || filename_str == "..")
                    {
                        continue;
                    }

                    auto child_rel = string{};
                    if (rel_accum.empty())
                    {
                        child_rel = filename_str;
                    }
                    else
                    {
                        child_rel.append(rel_accum.data(), rel_accum.size());
                        child_rel.append("/");
                        child_rel.append(filename_str);
                    }

                    if (it->is_directory())
                    {
                        // 1. Skip hidden directories starting with '.' (length > 1)
                        if (filename_str.size() > 1 && filename_str[0] == '.')
                        {
                            continue;
                        }

                        // 2. Check built-in and configured ignored directories
                        bool is_ignored = false;
                        for (const auto& ignored_dir : _ignored_directories)
                        {
                            if (filename_str == ignored_dir || child_rel == ignored_dir)
                            {
                                is_ignored = true;
                                break;
                            }
                        }
                        if (is_ignored)
                        {
                            continue;
                        }

                        for (const auto& add_dir : options.additional_ignored_directories)
                        {
                            auto norm_add = normalize_path_str(add_dir);
                            if (filename_str == norm_add || child_rel == norm_add)
                            {
                                is_ignored = true;
                                break;
                            }
                        }
                        if (is_ignored)
                        {
                            continue;
                        }

                        // 3. Custom predicate check for directory
                        if (options.predicate && !options.predicate(child_rel, true))
                        {
                            continue;
                        }

                        self(self, it->path(), child_rel);
                    }
                    else if (it->is_regular_file())
                    {
                        // 1. Extract extension
                        auto ext_dot = string::npos;
                        for (size_t i = filename_str.size(); i > 0; --i)
                        {
                            if (filename_str[i - 1] == '.')
                            {
                                ext_dot = i - 1;
                                break;
                            }
                        }
                        auto ext_str =
                            (ext_dot != string::npos)
                                ? string(tempest::substr(filename_str, ext_dot, filename_str.size() - ext_dot))
                                : string("");

                        // 2. Check ignored extensions
                        if (!ext_str.empty())
                        {
                            bool is_ext_ignored = false;
                            for (const auto& ignored_ext : _ignored_extensions)
                            {
                                if (ext_str == ignored_ext)
                                {
                                    is_ext_ignored = true;
                                    break;
                                }
                            }
                            if (is_ext_ignored)
                            {
                                continue;
                            }
                        }

                        // 3. Check whitelist (if specified)
                        if (!normalized_whitelist.empty())
                        {
                            bool in_whitelist = false;
                            for (const auto& white_ext : normalized_whitelist)
                            {
                                if (ext_str == white_ext)
                                {
                                    in_whitelist = true;
                                    break;
                                }
                            }
                            if (!in_whitelist)
                            {
                                continue;
                            }
                        }

                        // 4. Custom predicate check for file
                        if (options.predicate && !options.predicate(child_rel, false))
                        {
                            continue;
                        }

                        // 5. Build canonical entry key
                        auto entry_key = string{};
                        if (mp.alias.empty())
                        {
                            entry_key = child_rel;
                        }
                        else
                        {
                            entry_key.append("@");
                            entry_key.append(mp.alias);
                            entry_key.append("/");
                            entry_key.append(child_rel);
                        }

                        // 6. Query timestamp without reading file contents
                        auto disk_p = resolve_disk_path(entry_key);
                        auto disk_mtime = disk_p.has_value() ? get_file_last_write_time(disk_p.value()) : 0;

                        // If not registered in _sources yet, register it
                        auto src_it = _source_path_to_index.find(entry_key);
                        if (src_it == _source_path_to_index.end())
                        {
                            auto& src = _get_or_create_source(entry_key);
                            src.last_modified_time = disk_mtime;

                            // If shader file (.spv or .slang)
                            if (tempest::ends_with(entry_key, ".spv") || tempest::ends_with(entry_key, ".slang"))
                            {
                                register_asset(asset_type_id::of<shader_asset>(), entry_key);
                            }
                        }
                        else
                        {
                            // Existing source - check if timestamp modified on disk
                            auto& src = *_sources[src_it->second];
                            if (src.last_modified_time != 0 && disk_mtime != 0 && src.last_modified_time != disk_mtime)
                            {
                                src.last_modified_time = disk_mtime;
                                for (auto& asset : _assets)
                                {
                                    if (asset->source_id == src.id)
                                    {
                                        _cached_blobs.erase(asset->id);
                                        asset->blob_size = 0;
                                        asset->blob_offset = 0;
                                    }
                                }
                                _dirty = true;
                            }
                        }

                        // Populate basename index if not already present (higher priority mount root wins)
                        if (!_basename_to_relative_path.contains(filename_str))
                        {
                            _basename_to_relative_path.insert({filename_str, entry_key});
                        }

                        // Map .spv back to .slang
                        if (tempest::ends_with(entry_key, ".vert.spv") || tempest::ends_with(entry_key, ".frag.spv") ||
                            tempest::ends_with(entry_key, ".comp.spv"))
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
                                auto slang_name = string{};
                                if (mp.alias.empty())
                                {
                                    slang_name.append(base_stem);
                                    slang_name.append(".slang");
                                }
                                else
                                {
                                    slang_name.append("@");
                                    slang_name.append(mp.alias);
                                    slang_name.append("/");
                                    slang_name.append(base_stem);
                                    slang_name.append(".slang");
                                }
                                _relative_path_to_source_path[entry_key] = slang_name;
                            }
                        }
                    }
                }
            };

            scan_dir(scan_dir, root_path, string_view{});
        }
    }

    auto asset_database::resolve_disk_path(string_view relative_path) const -> optional<string>
    {
        auto parsed = parse_alias(relative_path);
        if (parsed.is_aliased)
        {
            auto normalized_sub = normalize_path_str(parsed.subpath);
            for (const auto& mp : _mount_roots)
            {
                if (mp.alias == parsed.alias)
                {
                    auto candidate = filesystem::path(mp.path.c_str());
                    if (!normalized_sub.empty())
                    {
                        candidate = candidate / filesystem::path(normalized_sub.c_str());
                    }
                    if (filesystem::exists(candidate))
                    {
                        return candidate.string();
                    }
                }
            }
            return nullopt;
        }

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
        auto parsed = parse_alias(path_or_name);
        if (parsed.is_aliased)
        {
            auto normalized_sub = normalize_path_str(parsed.subpath);
            auto aliased_key = string("@");
            aliased_key.append(parsed.alias.data(), parsed.alias.size());
            if (!normalized_sub.empty())
            {
                aliased_key.append("/");
                aliased_key.append(normalized_sub);
            }

            // Direct path lookup under alias
            return find_by_path(aliased_key);
        }

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

        // 4. On-demand search across mounted roots (only for non-ignored shaders)
        if (tempest::ends_with(normalized, ".spv") || tempest::ends_with(normalized, ".slang"))
        {
            if (!is_path_ignored(normalized, _ignored_directories, _ignored_extensions))
            {
                auto disk_path = resolve_disk_path(normalized);
                auto asset_key = normalized;
                if (!disk_path.has_value() && !tempest::starts_with(normalized, "shaders/"))
                {
                    auto with_shaders = string("shaders/");
                    with_shaders.append(normalized.data(), normalized.size());
                    if (!is_path_ignored(with_shaders, _ignored_directories, _ignored_extensions))
                    {
                        disk_path = resolve_disk_path(with_shaders);
                        if (disk_path.has_value())
                        {
                            asset_key = with_shaders;
                        }
                    }
                }

                if (disk_path.has_value())
                {
                    auto* mutable_this = const_cast<asset_database*>(this);
                    mutable_this->register_asset(asset_type_id::of<shader_asset>(), asset_key);

                    auto fs_path = filesystem::path(asset_key.c_str());
                    auto filename_str = string(fs_path.filename().string());
                    if (!mutable_this->_basename_to_relative_path.contains(filename_str))
                    {
                        mutable_this->_basename_to_relative_path.insert({filename_str, asset_key});
                    }

                    return find_by_path(asset_key);
                }
            }
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
        auto parsed = parse_alias(path_or_name);
        if (parsed.is_aliased)
        {
            auto normalized_sub = normalize_path_str(parsed.subpath);
            auto aliased_key = string("@");
            aliased_key.append(parsed.alias.data(), parsed.alias.size());
            if (!normalized_sub.empty())
            {
                aliased_key.append("/");
                aliased_key.append(normalized_sub);
            }

            // 1. Direct match in _relative_path_to_source_path
            auto it = _relative_path_to_source_path.find(aliased_key);
            if (it != _relative_path_to_source_path.end())
            {
                return it->second;
            }

            // 2. Direct match in _source_path_to_index (if not .spv)
            if (!tempest::ends_with(aliased_key, ".spv") && _source_path_to_index.contains(aliased_key))
            {
                return aliased_key;
            }

            // 3. If ends with .slang, return aliased_key
            if (tempest::ends_with(aliased_key, ".slang"))
            {
                return aliased_key;
            }

            // 4. If ends with .spv, try mapping to .slang
            if (tempest::ends_with(aliased_key, ".spv"))
            {
                auto fs_path = filesystem::path(normalized_sub.c_str());
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
                    auto base_stem = string(tempest::substr(filename, 0, dot_pos));
                    auto slang_name = string("@");
                    slang_name.append(parsed.alias.data(), parsed.alias.size());
                    slang_name.append("/");
                    slang_name.append(base_stem);
                    slang_name.append(".slang");

                    if (_source_path_to_index.contains(slang_name) || resolve_disk_path(slang_name).has_value())
                    {
                        return slang_name;
                    }
                }
            }

            // 5. Check if resolves on disk under matching alias mount roots
            if (resolve_disk_path(aliased_key).has_value())
            {
                return aliased_key;
            }

            return nullopt;
        }

        auto normalized = normalize_path_str(path_or_name);
        if (normalized.empty())
        {
            return nullopt;
        }

        // 1. Check relative path to source path mapping
        auto it = _relative_path_to_source_path.find(normalized);
        if (it != _relative_path_to_source_path.end())
        {
            return it->second;
        }

        // 2. Check basename map
        auto base_it = _basename_to_relative_path.find(normalized);
        if (base_it != _basename_to_relative_path.end())
        {
            auto rel_it = _relative_path_to_source_path.find(base_it->second);
            if (rel_it != _relative_path_to_source_path.end())
            {
                return rel_it->second;
            }
            return base_it->second;
        }

        // 3. If ends with .slang, return normalized
        if (tempest::ends_with(normalized, ".slang"))
        {
            return normalized;
        }

        // 4. If ends with .spv, try mapping to .slang
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
                auto base_stem = string(tempest::substr(filename, 0, dot_pos));
                auto slang_name = base_stem;
                slang_name.append(".slang");
                if (_source_path_to_index.contains(slang_name) || _basename_to_relative_path.contains(slang_name) ||
                    resolve_disk_path(slang_name).has_value())
                {
                    return slang_name;
                }
            }
        }

        // 5. Check indexed sources (for non-.spv)
        if (!tempest::ends_with(normalized, ".spv") && _source_path_to_index.contains(normalized))
        {
            return normalized;
        }

        // 6. Check if resolves on disk
        if (resolve_disk_path(normalized).has_value())
        {
            return normalized;
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
            if (paths_match(normalized, src->source_path))
            {
                match = true;
            }
            else
            {
                auto resolved_disk = resolve_disk_path(src->source_path);
                if (resolved_disk.has_value())
                {
                    auto norm_resolved = normalize_path_str(resolved_disk.value());
                    if (paths_match(normalized, norm_resolved))
                    {
                        match = true;
                    }
                }
                else
                {
                    auto src_filename = filesystem::path(src->source_path.c_str()).filename().string();
                    if (src_filename == filename)
                    {
                        match = true;
                    }
                }
            }

            if (match)
            {
                auto resolved = resolve_disk_path(src->source_path);
                auto disk_to_read = resolved.has_value() ? resolved.value() : string(normalized);
                src->last_modified_time = get_file_last_write_time(disk_to_read);
                auto bytes = core::read_bytes(string_view{disk_to_read.c_str(), disk_to_read.size()});

                if (!bytes.empty())
                {
                    src->source_hash = content_hash::compute(span<const byte>{bytes.data(), bytes.size()});
                }
                else
                {
                    src->source_hash = {};
                }

                for (auto& asset : _assets)
                {
                    if (asset->source_id == src->id)
                    {
                        if (!bytes.empty() && (asset->type == asset_type_id::of<shader_asset>() ||
                                               asset->type == asset_type_id::from_hash(0)))
                        {
                            store_blob(asset->id, span<const byte>{bytes.data(), bytes.size()});
                        }
                        else
                        {
                            _cached_blobs.erase(asset->id);
                            asset->blob_size = 0;
                            asset->blob_offset = 0;
                        }
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
            // Small asset: pack into packing chunk
            if (!_packing_chunk || _current_chunk_capacity - _current_chunk_used < data.size())
            {
                if (_packing_chunk)
                {
                    _blob_chunks.push_back(tempest::move(_packing_chunk));
                }
                _packing_chunk = make_unique<vector<byte>>();
                unsafe::resize_no_init(*_packing_chunk, default_chunk_size);
                _current_chunk_capacity = default_chunk_size;
                _current_chunk_used = 0;
            }

            dest_ptr = _packing_chunk->data() + _current_chunk_used;
            tempest::memcpy(const_cast<byte*>(dest_ptr), data.data(), data.size());
            _current_chunk_used += data.size();
        }

        _cached_blobs[asset_id] = span<const byte>{dest_ptr, data.size()};
        _dirty = true;
    }

    auto asset_database::get_blob(const guid& asset_id) const -> span<const byte>
    {
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
                auto disk_mtime = get_file_last_write_time(disk_path.value());
                auto cached_it = _cached_blobs.find(asset_id);
                if (cached_it != _cached_blobs.end() && !cached_it->second.empty() && src->last_modified_time != 0 &&
                    disk_mtime != 0 && disk_mtime == src->last_modified_time)
                {
                    return cached_it->second;
                }

                // Modified offline or not yet cached
                auto bytes = core::read_bytes(string_view{disk_path->c_str(), disk_path->size()});
                if (!bytes.empty())
                {
                    auto* mutable_this = const_cast<asset_database*>(this);
                    src->source_hash = content_hash::compute(span<const byte>{bytes.data(), bytes.size()});
                    src->last_modified_time = disk_mtime;
                    mutable_this->store_blob(asset_id, span<const byte>{bytes.data(), bytes.size()});
                    return mutable_this->_cached_blobs[asset_id];
                }
            }
        }

        auto it = _cached_blobs.find(asset_id);
        if (it != _cached_blobs.end() && !it->second.empty())
        {
            return it->second;
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
        auto& src = _get_or_create_source(source_path);

        auto disk_p = resolve_disk_path(source_path);
        if (disk_p.has_value())
        {
            src.last_modified_time = get_file_last_write_time(disk_p.value());
            auto bytes = core::read_bytes(string_view{disk_p->c_str(), disk_p->size()});
            if (!bytes.empty())
            {
                src.source_hash = content_hash::compute(span<const byte>{bytes.data(), bytes.size()});
            }
        }

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

            auto hier_type = asset_type_id::of<entity_hierarchy>();
            guid hier_id{};
            for (const auto& asset : _assets)
            {
                if (asset->source_id == src.id && asset->type == hier_type)
                {
                    hier_id = asset->id;
                    break;
                }
            }
            if (hier_id == guid{})
            {
                hier_id = register_asset(hier_type, source_path);
            }
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
            .last_modified_time = 0,
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
