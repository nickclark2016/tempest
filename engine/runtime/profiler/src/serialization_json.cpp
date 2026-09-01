#include <tempest/profiler/serialization.hpp>

#include <format>
#include <fstream>
#include <string>

namespace tempest::profiler
{
    namespace
    {
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

    auto export_chrome_trace_json_string(const capture_session_data& data) -> string
    {
        auto json = std::string{};
        json.reserve(64 * 1024);

        json += "{\n  \"traceEvents\": [\n";

        auto first_event = true;
        auto append_separator = [&]() {
            if (!first_event)
            {
                json += ",\n";
            }
            first_event = false;
        };

        // 1. Process metadata
        append_separator();
        json += "    {\"name\": \"process_name\", \"ph\": \"M\", \"pid\": 1, \"args\": {\"name\": \"Tempest Engine\"}}";

        // 2. Thread metadata
        for (const auto& track : data.tracks)
        {
            append_separator();
            auto escaped_name = std::string{};
            escape_json_string_to(string_view{track.name.data(), track.name.size()}, escaped_name);

            std::format_to(std::back_inserter(json),
                           "    {{\"name\": \"thread_name\", \"ph\": \"M\", \"pid\": 1, \"tid\": {}, \"args\": "
                           "{{\"name\": \"{}\"}}}}",
                           track.track_id, escaped_name);
        }

        // 3. Zone and Marker Events
        for (const auto& track : data.tracks)
        {
            const auto cat = (track.type == track_type::gpu_queue) ? "gpu" : "cpu";

            for (const auto& z : track.zones)
            {
                append_separator();
                auto escaped_zone_name = std::string{};
                escape_json_string_to(z.name, escaped_zone_name);

                const auto start_us = static_cast<double>(z.start_ns) / 1000.0;
                const auto dur_us = static_cast<double>(z.end_ns >= z.start_ns ? (z.end_ns - z.start_ns) : 0) / 1000.0;

                std::format_to(std::back_inserter(json),
                               "    {{\"name\": \"{}\", \"cat\": \"{}\", \"ph\": \"X\", \"ts\": {:.3f}, \"dur\": "
                               "{:.3f}, \"pid\": 1, \"tid\": {}",
                               escaped_zone_name, cat, start_us, dur_us, track.track_id);

                const auto has_task_id = (z.task_id != 0);
                const auto has_metrics = !z.metrics.empty();

                if (has_task_id || has_metrics)
                {
                    json += ", \"args\": {";
                    auto first_arg = true;

                    if (has_task_id)
                    {
                        std::format_to(std::back_inserter(json), "\"task_id\": {}", z.task_id);
                        first_arg = false;
                    }

                    for (const auto& met : z.metrics)
                    {
                        if (!first_arg)
                        {
                            json += ", ";
                        }
                        first_arg = false;
                        auto escaped_met_name = std::string{};
                        escape_json_string_to(met.name, escaped_met_name);
                        std::format_to(std::back_inserter(json), "\"{}\": {:.3f}", escaped_met_name, met.value);
                    }

                    json += "}";
                }

                json += "}";
            }

            for (const auto& m : track.markers)
            {
                append_separator();
                auto escaped_marker_name = std::string{};
                escape_json_string_to(m.name, escaped_marker_name);

                const auto ts_us = static_cast<double>(m.timestamp_ns) / 1000.0;
                std::format_to(std::back_inserter(json),
                               "    {{\"name\": \"{}\", \"cat\": \"marker\", \"ph\": \"i\", \"ts\": {:.3f}, \"pid\": "
                               "1, \"tid\": {}, \"s\": \"t\"}}",
                               escaped_marker_name, ts_us, track.track_id);
            }
        }

        // 4. Metric Streams
        for (const auto& st : data.metrics)
        {
            auto escaped_stream_name = std::string{};
            escape_json_string_to(string_view{st.name.data(), st.name.size()}, escaped_stream_name);

            for (const auto& smp : st.samples)
            {
                append_separator();
                const auto ts_us = static_cast<double>(smp.timestamp_ns) / 1000.0;
                std::format_to(std::back_inserter(json),
                               "    {{\"name\": \"{}\", \"cat\": \"metric\", \"ph\": \"C\", \"ts\": {:.3f}, \"pid\": "
                               "1, \"tid\": 0, \"args\": {{\"value\": {:.3f}}}}}",
                               escaped_stream_name, ts_us, smp.value);
            }
        }

        json += "\n  ],\n  \"displayTimeUnit\": \"ns\"\n}\n";

        return string{json.data(), json.size()};
    }

    auto export_chrome_trace_json(const capture_session_data& data, string_view file_path)
        -> expected<void, capture_error>
    {
        const auto json_str = export_chrome_trace_json_string(data);
        auto file = std::ofstream(std::string(file_path.data(), file_path.size()));
        if (!file.is_open())
        {
            return unexpected(capture_error::io_error);
        }

        file.write(json_str.data(), static_cast<std::streamsize>(json_str.size()));
        if (!file.good())
        {
            return unexpected(capture_error::io_error);
        }

        return {};
    }
} // namespace tempest::profiler
