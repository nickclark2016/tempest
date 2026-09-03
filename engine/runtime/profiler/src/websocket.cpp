#include <tempest/profiler/websocket.hpp>

#include <array>
#include <format>
#include <string>

namespace tempest::profiler
{
    namespace
    {
        inline uint32_t rotl32(uint32_t value, unsigned int count) noexcept
        {
            return (value << count) | (value >> (32 - count));
        }

        auto sha1(span<const uint8_t> data) -> std::array<uint8_t, 20>
        {
            auto h0 = uint32_t{0x67452301};
            auto h1 = uint32_t{0xEFCDAB89};
            auto h2 = uint32_t{0x98BADCFE};
            auto h3 = uint32_t{0x10325476};
            auto h4 = uint32_t{0xC3D2E1F0};

            const auto len_bytes = data.size();
            const auto len_bits = static_cast<uint64_t>(len_bytes) * 8;
            const auto padded_len = ((len_bytes + 8) / 64 + 1) * 64;

            auto padded = vector<uint8_t>(padded_len, 0);
            for (auto i = size_t{0}; i < len_bytes; ++i)
            {
                padded[i] = data[i];
            }
            padded[len_bytes] = 0x80;

            for (auto i = size_t{0}; i < 8; ++i)
            {
                padded[padded_len - 8 + i] = static_cast<uint8_t>((len_bits >> ((7 - i) * 8)) & 0xFF);
            }

            for (auto chunk = size_t{0}; chunk < padded_len; chunk += 64)
            {
                auto w = std::array<uint32_t, 80>{};
                for (auto i = size_t{0}; i < 16; ++i)
                {
                    w[i] = (static_cast<uint32_t>(padded[chunk + i * 4]) << 24) |
                           (static_cast<uint32_t>(padded[chunk + i * 4 + 1]) << 16) |
                           (static_cast<uint32_t>(padded[chunk + i * 4 + 2]) << 8) |
                           static_cast<uint32_t>(padded[chunk + i * 4 + 3]);
                }
                for (auto i = size_t{16}; i < 80; ++i)
                {
                    w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
                }

                auto a = h0;
                auto b = h1;
                auto c = h2;
                auto d = h3;
                auto e = h4;

                for (auto i = size_t{0}; i < 80; ++i)
                {
                    auto f = uint32_t{0};
                    auto k = uint32_t{0};

                    if (i < 20)
                    {
                        f = (b & c) | ((~b) & d);
                        k = 0x5A827999;
                    }
                    else if (i < 40)
                    {
                        f = b ^ c ^ d;
                        k = 0x6ED9EBA1;
                    }
                    else if (i < 60)
                    {
                        f = (b & c) | (b & d) | (c & d);
                        k = 0x8F1BBCDC;
                    }
                    else
                    {
                        f = b ^ c ^ d;
                        k = 0xCA62C1D6;
                    }

                    const auto temp = rotl32(a, 5) + f + e + k + w[i];
                    e = d;
                    d = c;
                    c = rotl32(b, 30);
                    b = a;
                    a = temp;
                }

                h0 += a;
                h1 += b;
                h2 += c;
                h3 += d;
                h4 += e;
            }

            auto digest = std::array<uint8_t, 20>{};
            const uint32_t h[5] = {h0, h1, h2, h3, h4};
            for (auto i = size_t{0}; i < 5; ++i)
            {
                digest[i * 4] = static_cast<uint8_t>((h[i] >> 24) & 0xFF);
                digest[i * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xFF);
                digest[i * 4 + 2] = static_cast<uint8_t>((h[i] >> 8) & 0xFF);
                digest[i * 4 + 3] = static_cast<uint8_t>(h[i] & 0xFF);
            }

            return digest;
        }

        constexpr char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        auto base64_encode(span<const uint8_t> data) -> std::string
        {
            auto out = std::string{};
            out.reserve(((data.size() + 2) / 3) * 4);
            auto i = size_t{0};
            while (i < data.size())
            {
                const auto rem = data.size() - i;
                if (rem >= 3)
                {
                    const auto octet_a = static_cast<uint32_t>(data[i++]);
                    const auto octet_b = static_cast<uint32_t>(data[i++]);
                    const auto octet_c = static_cast<uint32_t>(data[i++]);
                    const auto triple = (octet_a << 16) | (octet_b << 8) | octet_c;
                    out.push_back(b64_table[(triple >> 18) & 0x3F]);
                    out.push_back(b64_table[(triple >> 12) & 0x3F]);
                    out.push_back(b64_table[(triple >> 6) & 0x3F]);
                    out.push_back(b64_table[triple & 0x3F]);
                }
                else if (rem == 2)
                {
                    const auto octet_a = static_cast<uint32_t>(data[i++]);
                    const auto octet_b = static_cast<uint32_t>(data[i++]);
                    const auto triple = (octet_a << 16) | (octet_b << 8);
                    out.push_back(b64_table[(triple >> 18) & 0x3F]);
                    out.push_back(b64_table[(triple >> 12) & 0x3F]);
                    out.push_back(b64_table[(triple >> 6) & 0x3F]);
                    out.push_back('=');
                }
                else if (rem == 1)
                {
                    const auto octet_a = static_cast<uint32_t>(data[i++]);
                    const auto triple = octet_a << 16;
                    out.push_back(b64_table[(triple >> 18) & 0x3F]);
                    out.push_back(b64_table[(triple >> 12) & 0x3F]);
                    out.push_back('=');
                    out.push_back('=');
                }
            }
            return out;
        }

        auto escape_json_string_to(string_view src, std::string& out) -> void
        {
            for (const auto c : src)
            {
                switch (c)
                {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\b':
                    out += "\\b";
                    break;
                case '\f':
                    out += "\\f";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        std::format_to(std::back_inserter(out), "\\u{:04x}", static_cast<unsigned int>(c));
                    }
                    else
                    {
                        out += c;
                    }
                    break;
                }
            }
        }
    } // namespace

    auto compute_websocket_accept_key(string_view client_key) -> string
    {
        auto start = size_t{0};
        while (start < client_key.size() && (client_key[start] == ' ' || client_key[start] == '\t' ||
                                             client_key[start] == '\r' || client_key[start] == '\n'))
        {
            ++start;
        }
        auto end = client_key.size();
        while (end > start && (client_key[end - 1] == ' ' || client_key[end - 1] == '\t' ||
                               client_key[end - 1] == '\r' || client_key[end - 1] == '\n'))
        {
            --end;
        }

        auto combined = std::string(client_key.data() + start, end - start);
        combined += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

        const auto digest =
            sha1(span<const uint8_t>{reinterpret_cast<const uint8_t*>(combined.data()), combined.size()});
        const auto b64 = base64_encode(span<const uint8_t>{digest.data(), digest.size()});
        return string{b64.data(), b64.size()};
    }

    auto encode_websocket_frame(ws_opcode opcode, span<const byte> payload) -> vector<byte>
    {
        auto frame = vector<byte>{};
        const auto payload_size = payload.size();

        // 1. Byte 0: FIN bit (0x80) | opcode
        frame.push_back(static_cast<byte>(0x80 | (static_cast<uint8_t>(opcode) & 0x0F)));

        // 2. Length (Server to client: mask bit is 0)
        if (payload_size <= 125)
        {
            frame.push_back(static_cast<byte>(payload_size));
        }
        else if (payload_size <= 0xFFFF)
        {
            frame.push_back(static_cast<byte>(126));
            frame.push_back(static_cast<byte>((payload_size >> 8) & 0xFF));
            frame.push_back(static_cast<byte>(payload_size & 0xFF));
        }
        else
        {
            frame.push_back(static_cast<byte>(127));
            for (auto i = int{7}; i >= 0; --i)
            {
                frame.push_back(static_cast<byte>((payload_size >> (i * 8)) & 0xFF));
            }
        }

        // 3. Append payload
        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    auto encode_websocket_client_frame(ws_opcode opcode, span<const byte> payload, uint32_t mask_key) -> vector<byte>
    {
        auto frame = vector<byte>{};
        const auto payload_size = payload.size();

        // 1. Byte 0: FIN bit (0x80) | opcode
        frame.push_back(static_cast<byte>(0x80 | (static_cast<uint8_t>(opcode) & 0x0F)));

        // 2. Length (Client to server: mask bit is 0x80)
        if (payload_size <= 125)
        {
            frame.push_back(static_cast<byte>(0x80 | static_cast<uint8_t>(payload_size)));
        }
        else if (payload_size <= 0xFFFF)
        {
            frame.push_back(static_cast<byte>(0x80 | 126));
            frame.push_back(static_cast<byte>((payload_size >> 8) & 0xFF));
            frame.push_back(static_cast<byte>(payload_size & 0xFF));
        }
        else
        {
            frame.push_back(static_cast<byte>(0x80 | 127));
            for (auto i = int{7}; i >= 0; --i)
            {
                frame.push_back(static_cast<byte>((payload_size >> (i * 8)) & 0xFF));
            }
        }

        // 3. Mask Key
        const uint8_t mask_bytes[4] = {
            static_cast<uint8_t>((mask_key >> 24) & 0xFF),
            static_cast<uint8_t>((mask_key >> 16) & 0xFF),
            static_cast<uint8_t>((mask_key >> 8) & 0xFF),
            static_cast<uint8_t>(mask_key & 0xFF),
        };
        for (auto i = size_t{0}; i < 4; ++i)
        {
            frame.push_back(static_cast<byte>(mask_bytes[i]));
        }

        // 4. Mask and append payload
        frame.reserve(frame.size() + payload_size);
        for (auto i = size_t{0}; i < payload_size; ++i)
        {
            frame.push_back(static_cast<byte>(static_cast<uint8_t>(payload[i]) ^ mask_bytes[i % 4]));
        }

        return frame;
    }

    auto decode_websocket_frame(span<const byte> data, size_t& bytes_consumed) -> expected<ws_message, ws_error>
    {
        bytes_consumed = 0;
        if (data.size() < 2)
        {
            return unexpected(ws_error::incomplete_frame);
        }

        const auto byte0 = static_cast<uint8_t>(data[0]);

        // RFC 6455 Section 5.2: RSV bits MUST be 0 unless an extension is negotiated
        if ((byte0 & 0x70) != 0)
        {
            return unexpected(ws_error::invalid_frame);
        }

        const auto opcode_raw = static_cast<uint8_t>(byte0 & 0x0F);
        const auto is_control = (opcode_raw & 0x08) != 0;
        const auto is_valid_opcode = (opcode_raw <= 0x02) || (opcode_raw >= 0x08 && opcode_raw <= 0x0A);
        if (!is_valid_opcode)
        {
            return unexpected(ws_error::invalid_frame);
        }
        const auto opcode = static_cast<ws_opcode>(opcode_raw);

        const auto byte1 = static_cast<uint8_t>(data[1]);
        const auto is_masked = (byte1 & 0x80) != 0;
        const auto payload_len_field = static_cast<uint8_t>(byte1 & 0x7F);

        if (is_control)
        {
            // RFC 6455 Section 5.5: Control frames MUST NOT be fragmented
            if ((byte0 & 0x80) == 0)
            {
                return unexpected(ws_error::invalid_frame);
            }
            // RFC 6455 Section 5.5: All control frames MUST have a payload length of 125 bytes or less
            if (payload_len_field > 125)
            {
                return unexpected(ws_error::invalid_frame);
            }
        }

        auto cursor = size_t{2};
        auto payload_len = uint64_t{0};

        if (payload_len_field <= 125)
        {
            payload_len = payload_len_field;
        }
        else if (payload_len_field == 126)
        {
            if (data.size() < cursor + 2)
            {
                return unexpected(ws_error::incomplete_frame);
            }
            payload_len = (static_cast<uint64_t>(static_cast<uint8_t>(data[cursor])) << 8) |
                          static_cast<uint64_t>(static_cast<uint8_t>(data[cursor + 1]));
            cursor += 2;
        }
        else if (payload_len_field == 127)
        {
            if (data.size() < cursor + 8)
            {
                return unexpected(ws_error::incomplete_frame);
            }
            for (auto i = size_t{0}; i < 8; ++i)
            {
                payload_len = (payload_len << 8) | static_cast<uint64_t>(static_cast<uint8_t>(data[cursor + i]));
            }
            cursor += 8;
        }

        auto mask_key = std::array<uint8_t, 4>{};
        if (is_masked)
        {
            if (data.size() < cursor + 4)
            {
                return unexpected(ws_error::incomplete_frame);
            }
            for (auto i = size_t{0}; i < 4; ++i)
            {
                mask_key[i] = static_cast<uint8_t>(data[cursor + i]);
            }
            cursor += 4;
        }

        constexpr auto max_ws_payload_bytes = uint64_t{64 * 1024 * 1024}; // 64 MB
        if (payload_len > max_ws_payload_bytes)
        {
            return unexpected(ws_error::payload_too_large);
        }

        if (data.size() < cursor || (data.size() - cursor) < payload_len)
        {
            return unexpected(ws_error::incomplete_frame);
        }

        auto msg = ws_message{};
        msg.opcode = opcode;
        msg.is_masked = is_masked;
        msg.payload.resize(payload_len);

        if (is_masked)
        {
            for (auto i = size_t{0}; i < payload_len; ++i)
            {
                msg.payload[i] = static_cast<byte>(static_cast<uint8_t>(data[cursor + i]) ^ mask_key[i % 4]);
            }
        }
        else
        {
            for (auto i = size_t{0}; i < payload_len; ++i)
            {
                msg.payload[i] = data[cursor + i];
            }
        }

        bytes_consumed = cursor + static_cast<size_t>(payload_len);
        return msg;
    }

    auto decode_websocket_frame(span<const byte> data) -> expected<ws_message, ws_error>
    {
        auto consumed = size_t{0};
        return decode_websocket_frame(data, consumed);
    }

    auto serialize_telemetry_frame_json(const telemetry_frame& frame) -> string
    {
        auto json = std::string{};
        json.reserve(8192);

        std::format_to(std::back_inserter(json), "{{\"type\":\"frame_data\",\"frame_index\":{},\"cpu_tracks\":[",
                       frame.frame_index);

        // CPU Tracks
        for (auto i = size_t{0}; i < frame.cpu_tracks.size(); ++i)
        {
            if (i > 0)
            {
                json += ",";
            }
            const auto& track = frame.cpu_tracks[i];
            auto escaped_tname = std::string{};
            escape_json_string_to(string_view{track.name.data(), track.name.size()}, escaped_tname);

            std::format_to(std::back_inserter(json), "{{\"track_id\":{},\"name\":\"{}\",\"zones\":[", track.track_id,
                           escaped_tname);

            for (auto j = size_t{0}; j < track.zones.size(); ++j)
            {
                if (j > 0)
                {
                    json += ",";
                }
                const auto& z = track.zones[j];
                auto escaped_zname = std::string{};
                escape_json_string_to(string_view{z.name.data(), z.name.size()}, escaped_zname);

                std::format_to(
                    std::back_inserter(json),
                    "{{\"name\":\"{}\",\"start_ns\":{},\"end_ns\":{},\"depth\":{},\"frame_index\":{},\"metrics\":[",
                    escaped_zname, z.start_ns, z.end_ns, z.depth, z.frame_index);

                for (auto k = size_t{0}; k < z.metrics.size(); ++k)
                {
                    if (k > 0)
                    {
                        json += ",";
                    }
                    const auto& m = z.metrics[k];
                    auto escaped_mname = std::string{};
                    escape_json_string_to(m.name, escaped_mname);

                    std::format_to(std::back_inserter(json), "{{\"name\":\"{}\",\"value\":{:.3f},\"unit\":{}}}",
                                   escaped_mname, m.value, static_cast<uint8_t>(m.unit));
                }
                json += "]}";
            }
            json += "]}";
        }
        json += "],\"gpu_tracks\":[";

        // GPU Tracks
        for (auto i = size_t{0}; i < frame.gpu_tracks.size(); ++i)
        {
            if (i > 0)
            {
                json += ",";
            }
            const auto& track = frame.gpu_tracks[i];
            auto escaped_tname = std::string{};
            escape_json_string_to(string_view{track.name.data(), track.name.size()}, escaped_tname);

            std::format_to(std::back_inserter(json), "{{\"track_id\":{},\"name\":\"{}\",\"zones\":[", track.track_id,
                           escaped_tname);

            for (auto j = size_t{0}; j < track.zones.size(); ++j)
            {
                if (j > 0)
                {
                    json += ",";
                }
                const auto& z = track.zones[j];
                auto escaped_zname = std::string{};
                escape_json_string_to(string_view{z.name.data(), z.name.size()}, escaped_zname);

                std::format_to(
                    std::back_inserter(json),
                    "{{\"name\":\"{}\",\"start_ns\":{},\"end_ns\":{},\"depth\":{},\"frame_index\":{},\"metrics\":[",
                    escaped_zname, z.start_ns, z.end_ns, z.depth, z.frame_index);

                for (auto k = size_t{0}; k < z.metrics.size(); ++k)
                {
                    if (k > 0)
                    {
                        json += ",";
                    }
                    const auto& m = z.metrics[k];
                    auto escaped_mname = std::string{};
                    escape_json_string_to(m.name, escaped_mname);

                    std::format_to(std::back_inserter(json), "{{\"name\":\"{}\",\"value\":{:.3f},\"unit\":{}}}",
                                   escaped_mname, m.value, static_cast<uint8_t>(m.unit));
                }
                json += "]}";
            }
            json += "]}";
        }
        json += "],\"markers\":[";

        // Markers
        for (auto i = size_t{0}; i < frame.markers.size(); ++i)
        {
            if (i > 0)
            {
                json += ",";
            }
            const auto& m = frame.markers[i];
            auto escaped_mname = std::string{};
            escape_json_string_to(m.name, escaped_mname);

            std::format_to(std::back_inserter(json), "{{\"name\":\"{}\",\"timestamp_ns\":{}}}", escaped_mname,
                           m.timestamp_ns);
        }
        json += "],\"metrics\":[";

        // Metrics
        for (auto i = size_t{0}; i < frame.metrics.size(); ++i)
        {
            if (i > 0)
            {
                json += ",";
            }
            const auto& m = frame.metrics[i];
            auto escaped_mname = std::string{};
            escape_json_string_to(m.name, escaped_mname);

            std::format_to(std::back_inserter(json), "{{\"name\":\"{}\",\"value\":{:.3f},\"unit\":{}}}", escaped_mname,
                           m.value, static_cast<uint8_t>(m.unit));
        }
        json += "]}";

        return string{json.data(), json.size()};
    }

    auto create_telemetry_frame_from_capture(uint64_t frame_index, const capture_session_data& data) -> telemetry_frame
    {
        auto frame = telemetry_frame{};
        frame.frame_index = frame_index;

        for (const auto& tr : data.tracks)
        {
            auto t = telemetry_track{};
            t.track_id = tr.track_id;
            t.name = tr.name;

            for (const auto& zr : tr.zones)
            {
                auto tz = telemetry_zone{};
                tz.name = string{zr.name.data(), zr.name.size()};
                tz.start_ns = zr.start_ns;
                tz.end_ns = zr.end_ns;
                tz.depth = zr.depth;
                tz.frame_index = zr.task_id > 0 ? zr.task_id : frame_index;
                tz.metrics = zr.metrics;
                t.zones.push_back(move(tz));
            }

            for (const auto& mr : tr.markers)
            {
                frame.markers.push_back(mr);
            }

            if (tr.type == track_type::gpu_queue)
            {
                frame.gpu_tracks.push_back(move(t));
            }
            else
            {
                frame.cpu_tracks.push_back(move(t));
            }
        }

        for (const auto& ms : data.metrics)
        {
            for (const auto& smp : ms.samples)
            {
                frame.metrics.push_back(smp);
            }
        }

        return frame;
    }
} // namespace tempest::profiler
