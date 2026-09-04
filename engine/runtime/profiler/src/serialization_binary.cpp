#include <tempest/flat_unordered_map.hpp>
#include <tempest/profiler/serialization.hpp>

#include <miniz/miniz.h>

#include <cstring>
#include <fstream>
#include <string>

namespace tempest::profiler
{
    namespace
    {
#pragma pack(push, 1)
        struct tprof_header
        {
            uint8_t magic[4];       // 'T', 'P', 'R', 'F'
            uint16_t version_major; // 1
            uint16_t version_minor; // 0
            uint64_t uncompressed_size;
            uint64_t compressed_size;
            uint32_t flags; // 0
        };
#pragma pack(pop)

        constexpr uint8_t magic_bytes[4] = {'T', 'P', 'R', 'F'};

        struct binary_writer
        {
            vector<byte> buffer;

            template <typename T>
                requires is_trivially_copyable_v<T>
            auto write(const T& val) -> void
            {
                const auto* ptr = reinterpret_cast<const byte*>(&val);
                buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
            }

            auto write_bytes(const void* data, size_t size) -> void
            {
                const auto* ptr = static_cast<const byte*>(data);
                buffer.insert(buffer.end(), ptr, ptr + size);
            }
        };

        struct binary_reader
        {
            span<const byte> data;
            size_t cursor{0};

            [[nodiscard]] auto has_remaining(size_t bytes) const noexcept -> bool
            {
                return cursor + bytes <= data.size();
            }

            template <typename T>
                requires is_trivially_copyable_v<T>
            auto read(T& val) noexcept -> bool
            {
                if (!has_remaining(sizeof(T)))
                {
                    return false;
                }
                std::memcpy(&val, data.data() + cursor, sizeof(T));
                cursor += sizeof(T);
                return true;
            }

            auto read_bytes(void* dest, size_t size) noexcept -> bool
            {
                if (!has_remaining(size))
                {
                    return false;
                }
                std::memcpy(dest, data.data() + cursor, size);
                cursor += size;
                return true;
            }
        };
    } // namespace

    auto serialize_binary_to_buffer(const capture_session_data& data) -> vector<byte>
    {
        auto string_table = vector<string_view>{};
        auto str_map = flat_unordered_map<string_view, uint32_t>{};

        auto get_string_id = [&](string_view s) -> uint32_t {
            if (s.empty())
            {
                return 0xFFFFFFFF;
            }
            const auto it = str_map.find(s);
            if (it != str_map.end())
            {
                return it->second;
            }
            const auto id = static_cast<uint32_t>(string_table.size());
            string_table.push_back(s);
            str_map.insert({s, id});
            return id;
        };

        // 1. Scan and index all strings
        for (const auto& track : data.tracks)
        {
            get_string_id(string_view{track.name.data(), track.name.size()});
            for (const auto& z : track.zones)
            {
                get_string_id(z.name);
                for (const auto& met : z.metrics)
                {
                    get_string_id(met.name);
                }
            }
            for (const auto& m : track.markers)
            {
                get_string_id(m.name);
            }
        }

        for (const auto& st : data.metrics)
        {
            get_string_id(string_view{st.name.data(), st.name.size()});
            for (const auto& smp : st.samples)
            {
                get_string_id(smp.name);
            }
        }

        // 2. Encode uncompressed payload
        auto payload = binary_writer{};

        payload.write(data.start_time_ns);
        payload.write(data.end_time_ns);

        // String Dictionary
        const auto str_count = static_cast<uint32_t>(string_table.size());
        payload.write(str_count);
        for (const auto sv : string_table)
        {
            const auto len = static_cast<uint32_t>(sv.size());
            payload.write(len);
            if (len > 0)
            {
                payload.write_bytes(sv.data(), len);
            }
        }

        // Tracks
        const auto track_count = static_cast<uint32_t>(data.tracks.size());
        payload.write(track_count);
        for (const auto& track : data.tracks)
        {
            payload.write(track.track_id);
            payload.write(get_string_id(string_view{track.name.data(), track.name.size()}));
            payload.write(static_cast<uint8_t>(track.type));

            const auto zone_count = static_cast<uint32_t>(track.zones.size());
            payload.write(zone_count);
            for (const auto& z : track.zones)
            {
                payload.write(z.start_ns);
                payload.write(z.end_ns);
                payload.write(z.depth);
                payload.write(get_string_id(z.name));
                payload.write(z.task_id);

                const auto met_count = static_cast<uint8_t>(z.metrics.size());
                payload.write(met_count);
                for (const auto& met : z.metrics)
                {
                    payload.write(met.timestamp_ns);
                    payload.write(get_string_id(met.name));
                    payload.write(met.value);
                    payload.write(static_cast<uint8_t>(met.unit));
                }
            }

            const auto marker_count = static_cast<uint32_t>(track.markers.size());
            payload.write(marker_count);
            for (const auto& m : track.markers)
            {
                payload.write(m.timestamp_ns);
                payload.write(get_string_id(m.name));
            }
        }

        // Metric Streams
        const auto metric_stream_count = static_cast<uint32_t>(data.metrics.size());
        payload.write(metric_stream_count);
        for (const auto& st : data.metrics)
        {
            payload.write(get_string_id(string_view{st.name.data(), st.name.size()}));
            payload.write(static_cast<uint8_t>(st.unit));

            const auto sample_count = static_cast<uint32_t>(st.samples.size());
            payload.write(sample_count);
            for (const auto& smp : st.samples)
            {
                payload.write(smp.timestamp_ns);
                payload.write(get_string_id(smp.name));
                payload.write(smp.value);
                payload.write(static_cast<uint8_t>(smp.unit));
            }
        }

        // 3. Compress with miniz
        const auto uncompressed_size = payload.buffer.size();
        const auto max_compressed = mz_compressBound(static_cast<mz_ulong>(uncompressed_size));
        auto compressed_data = vector<byte>(static_cast<size_t>(max_compressed));
        auto actual_compressed_len = static_cast<mz_ulong>(max_compressed);

        const auto status = mz_compress(
            reinterpret_cast<unsigned char*>(compressed_data.data()), &actual_compressed_len,
            reinterpret_cast<const unsigned char*>(payload.buffer.data()), static_cast<mz_ulong>(uncompressed_size));

        if (status != MZ_OK)
        {
            return {};
        }

        compressed_data.resize(actual_compressed_len);

        // 4. Assemble final file buffer
        auto header = tprof_header{
            .magic = {magic_bytes[0], magic_bytes[1], magic_bytes[2], magic_bytes[3]},
            .version_major = 1,
            .version_minor = 0,
            .uncompressed_size = static_cast<uint64_t>(uncompressed_size),
            .compressed_size = static_cast<uint64_t>(actual_compressed_len),
            .flags = 0,
        };

        auto final_buffer = vector<byte>{};
        final_buffer.reserve(sizeof(tprof_header) + actual_compressed_len);

        const auto* header_bytes = reinterpret_cast<const byte*>(&header);
        final_buffer.insert(final_buffer.end(), header_bytes, header_bytes + sizeof(tprof_header));
        final_buffer.insert(final_buffer.end(), compressed_data.begin(), compressed_data.end());

        return final_buffer;
    }

    auto deserialize_binary_from_buffer(span<const byte> buffer) -> expected<capture_session_data, capture_error>
    {
        if (buffer.size() < sizeof(tprof_header))
        {
            return unexpected(capture_error::corrupted_data);
        }

        auto header = tprof_header{};
        std::memcpy(&header, buffer.data(), sizeof(tprof_header));

        if (header.magic[0] != magic_bytes[0] || header.magic[1] != magic_bytes[1] ||
            header.magic[2] != magic_bytes[2] || header.magic[3] != magic_bytes[3])
        {
            return unexpected(capture_error::invalid_magic);
        }

        if (header.version_major != 1)
        {
            return unexpected(capture_error::unsupported_version);
        }

        constexpr auto max_uncompressed_size = uint64_t{512 * 1024 * 1024}; // 512 MB
        if (header.uncompressed_size > max_uncompressed_size ||
            buffer.size() < sizeof(tprof_header) + header.compressed_size)
        {
            return unexpected(capture_error::corrupted_data);
        }

        auto uncompressed = vector<byte>(static_cast<size_t>(header.uncompressed_size));
        auto actual_uncompressed_len = static_cast<mz_ulong>(header.uncompressed_size);

        const auto status =
            mz_uncompress(reinterpret_cast<unsigned char*>(uncompressed.data()), &actual_uncompressed_len,
                          reinterpret_cast<const unsigned char*>(buffer.data() + sizeof(tprof_header)),
                          static_cast<mz_ulong>(header.compressed_size));

        if (status != MZ_OK || actual_uncompressed_len != header.uncompressed_size)
        {
            return unexpected(capture_error::decompression_failed);
        }

        auto reader = binary_reader{
            .data = span<const byte>{uncompressed.data(), uncompressed.size()},
            .cursor = 0,
        };

        auto result = capture_session_data{};

        if (!reader.read(result.start_time_ns) || !reader.read(result.end_time_ns))
        {
            return unexpected(capture_error::corrupted_data);
        }

        // Read string dictionary
        auto string_count = uint32_t{0};
        if (!reader.read(string_count))
        {
            return unexpected(capture_error::corrupted_data);
        }

        if (string_count > 10'000'000)
        {
            return unexpected(capture_error::corrupted_data);
        }

        result.string_table.reserve(string_count);
        for (auto i = uint32_t{0}; i < string_count; ++i)
        {
            auto len = uint32_t{0};
            if (!reader.read(len))
            {
                return unexpected(capture_error::corrupted_data);
            }
            if (!reader.has_remaining(len))
            {
                return unexpected(capture_error::corrupted_data);
            }
            auto str = string{reinterpret_cast<const char*>(reader.data.data() + reader.cursor), len};
            reader.cursor += len;
            result.string_table.push_back(tempest::move(str));
        }

        auto resolve_str = [&](uint32_t id) -> expected<string_view, capture_error> {
            if (id == 0xFFFFFFFF)
            {
                return string_view{};
            }
            if (id >= result.string_table.size())
            {
                return unexpected(capture_error::corrupted_data);
            }
            return string_view{result.string_table[id].data(), result.string_table[id].size()};
        };

        // Read Tracks
        auto track_count = uint32_t{0};
        if (!reader.read(track_count) || track_count > 1'000'000)
        {
            return unexpected(capture_error::corrupted_data);
        }

        result.tracks.reserve(track_count);
        for (auto t = uint32_t{0}; t < track_count; ++t)
        {
            auto tid = uint64_t{0};
            auto name_id = uint32_t{0};
            auto type_raw = uint8_t{0};
            auto zone_count = uint32_t{0};

            if (!reader.read(tid) || !reader.read(name_id) || !reader.read(type_raw) || !reader.read(zone_count))
            {
                return unexpected(capture_error::corrupted_data);
            }

            const auto name_res = resolve_str(name_id);
            if (!name_res.has_value())
            {
                return unexpected(name_res.error());
            }

            auto track = track_data{
                .track_id = tid,
                .name = string{name_res.value().data(), name_res.value().size()},
                .type = static_cast<track_type>(type_raw),
                .zones = {},
                .markers = {},
            };
            track.zones.reserve(zone_count);

            for (auto z = uint32_t{0}; z < zone_count; ++z)
            {
                auto start_ns = uint64_t{0};
                auto end_ns = uint64_t{0};
                auto depth = uint32_t{0};
                auto z_name_id = uint32_t{0};
                auto task_id = uint64_t{0};
                auto met_count = uint8_t{0};

                if (!reader.read(start_ns) || !reader.read(end_ns) || !reader.read(depth) || !reader.read(z_name_id) ||
                    !reader.read(task_id) || !reader.read(met_count))
                {
                    return unexpected(capture_error::corrupted_data);
                }

                const auto z_name_res = resolve_str(z_name_id);
                if (!z_name_res.has_value())
                {
                    return unexpected(z_name_res.error());
                }

                auto zone = zone_record{
                    .start_ns = start_ns,
                    .end_ns = end_ns,
                    .depth = depth,
                    .name = z_name_res.value(),
                    .location = {},
                    .task_id = task_id,
                    .metrics = {},
                };

                for (auto m = uint8_t{0}; m < met_count; ++m)
                {
                    auto m_ts = uint64_t{0};
                    auto m_name_id = uint32_t{0};
                    auto m_val = double{0.0};
                    auto m_unit_raw = uint8_t{0};

                    if (!reader.read(m_ts) || !reader.read(m_name_id) || !reader.read(m_val) ||
                        !reader.read(m_unit_raw))
                    {
                        return unexpected(capture_error::corrupted_data);
                    }

                    const auto m_name_res = resolve_str(m_name_id);
                    if (!m_name_res.has_value())
                    {
                        return unexpected(m_name_res.error());
                    }

                    zone.metrics.push_back(metric_record{
                        .timestamp_ns = m_ts,
                        .name = m_name_res.value(),
                        .value = m_val,
                        .unit = static_cast<metric_unit>(m_unit_raw),
                    });
                }

                track.zones.push_back(zone);
            }

            auto marker_count = uint32_t{0};
            if (!reader.read(marker_count))
            {
                return unexpected(capture_error::corrupted_data);
            }

            track.markers.reserve(marker_count);
            for (auto mk = uint32_t{0}; mk < marker_count; ++mk)
            {
                auto mark_ts = uint64_t{0};
                auto mark_name_id = uint32_t{0};

                if (!reader.read(mark_ts) || !reader.read(mark_name_id))
                {
                    return unexpected(capture_error::corrupted_data);
                }

                const auto mark_name_res = resolve_str(mark_name_id);
                if (!mark_name_res.has_value())
                {
                    return unexpected(mark_name_res.error());
                }

                track.markers.push_back(marker_record{
                    .timestamp_ns = mark_ts,
                    .name = mark_name_res.value(),
                    .location = {},
                });
            }

            result.tracks.push_back(tempest::move(track));
        }

        // Read Metric Streams
        auto metric_stream_count = uint32_t{0};
        if (!reader.read(metric_stream_count) || metric_stream_count > 100'000)
        {
            return unexpected(capture_error::corrupted_data);
        }

        result.metrics.reserve(metric_stream_count);
        for (auto s = uint32_t{0}; s < metric_stream_count; ++s)
        {
            auto stream_name_id = uint32_t{0};
            auto unit_raw = uint8_t{0};
            auto sample_count = uint32_t{0};

            if (!reader.read(stream_name_id) || !reader.read(unit_raw) || !reader.read(sample_count))
            {
                return unexpected(capture_error::corrupted_data);
            }

            const auto s_name_res = resolve_str(stream_name_id);
            if (!s_name_res.has_value())
            {
                return unexpected(s_name_res.error());
            }

            auto stream = metric_stream{
                .name = string{s_name_res.value().data(), s_name_res.value().size()},
                .unit = static_cast<metric_unit>(unit_raw),
                .samples = {},
            };
            stream.samples.reserve(sample_count);

            for (auto smp_idx = uint32_t{0}; smp_idx < sample_count; ++smp_idx)
            {
                auto s_ts = uint64_t{0};
                auto s_name_id = uint32_t{0};
                auto s_val = double{0.0};
                auto s_unit_raw = uint8_t{0};

                if (!reader.read(s_ts) || !reader.read(s_name_id) || !reader.read(s_val) || !reader.read(s_unit_raw))
                {
                    return unexpected(capture_error::corrupted_data);
                }

                const auto smp_name_res = resolve_str(s_name_id);
                if (!smp_name_res.has_value())
                {
                    return unexpected(smp_name_res.error());
                }

                stream.samples.push_back(metric_record{
                    .timestamp_ns = s_ts,
                    .name = smp_name_res.value(),
                    .value = s_val,
                    .unit = static_cast<metric_unit>(s_unit_raw),
                });
            }

            result.metrics.push_back(tempest::move(stream));
        }

        return result;
    }

    auto save_binary_capture(const capture_session_data& data, string_view file_path) -> expected<void, capture_error>
    {
        const auto buf = serialize_binary_to_buffer(data);
        auto file = std::ofstream(std::string(file_path.data(), file_path.size()), std::ios::binary);
        if (!file.is_open())
        {
            return unexpected(capture_error::io_error);
        }

        file.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        if (!file.good())
        {
            return unexpected(capture_error::io_error);
        }

        return {};
    }

    auto load_binary_capture(string_view file_path) -> expected<capture_session_data, capture_error>
    {
        auto file = std::ifstream(std::string(file_path.data(), file_path.size()), std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return unexpected(capture_error::io_error);
        }

        const auto file_size = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        auto buf = vector<byte>{};
        buf.resize(file_size);
        file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(file_size));

        if (!file.good() && file_size > 0)
        {
            return unexpected(capture_error::io_error);
        }

        return deserialize_binary_from_buffer(span<const byte>{buf.data(), buf.size()});
    }
} // namespace tempest::profiler
