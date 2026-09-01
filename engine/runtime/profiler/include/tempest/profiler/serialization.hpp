#ifndef tempest_profiler_serialization_hpp
#define tempest_profiler_serialization_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/profiler/capture.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::profiler
{
    enum class capture_error : uint8_t
    {
        invalid_magic,
        unsupported_version,
        decompression_failed,
        corrupted_data,
        io_error,
        json_parse_error,
    };

    TEMPEST_API auto save_binary_capture(const capture_session_data& data, string_view file_path)
        -> expected<void, capture_error>;
    TEMPEST_API auto load_binary_capture(string_view file_path) -> expected<capture_session_data, capture_error>;
    TEMPEST_API auto serialize_binary_to_buffer(const capture_session_data& data) -> vector<byte>;
    TEMPEST_API auto deserialize_binary_from_buffer(span<const byte> buffer)
        -> expected<capture_session_data, capture_error>;

    TEMPEST_API auto export_chrome_trace_json(const capture_session_data& data, string_view file_path)
        -> expected<void, capture_error>;
    TEMPEST_API auto export_chrome_trace_json_string(const capture_session_data& data) -> string;
} // namespace tempest::profiler

#endif // tempest_profiler_serialization_hpp
