#ifndef tempest_profiler_websocket_hpp
#define tempest_profiler_websocket_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/int.hpp>
#include <tempest/profiler/capture.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::profiler
{
    enum class ws_opcode : uint8_t
    {
        continuation = 0x0,
        text = 0x1,
        binary = 0x2,
        close = 0x8,
        ping = 0x9,
        pong = 0xA,
    };

    enum class ws_error : uint8_t
    {
        incomplete_frame,
        invalid_frame,
        payload_too_large,
        connection_closed,
        socket_error,
    };

    struct ws_message
    {
        ws_opcode opcode{ws_opcode::text};
        vector<byte> payload{};
    };

    struct telemetry_zone
    {
        string name{};
        uint64_t start_ns{0};
        uint64_t end_ns{0};
        uint32_t depth{0};
        uint64_t frame_index{0};
        inplace_vector<metric_record, 4> metrics{};
    };

    struct telemetry_track
    {
        uint64_t track_id{0};
        string name{};
        vector<telemetry_zone> zones{};
    };

    struct telemetry_frame
    {
        uint64_t frame_index{0};
        vector<telemetry_track> cpu_tracks{};
        vector<telemetry_track> gpu_tracks{};
        vector<marker_record> markers{};
        vector<metric_record> metrics{};
    };

    TEMPEST_API auto compute_websocket_accept_key(string_view client_key) -> string;
    TEMPEST_API auto encode_websocket_frame(ws_opcode opcode, span<const byte> payload) -> vector<byte>;
    TEMPEST_API auto encode_websocket_client_frame(ws_opcode opcode, span<const byte> payload,
                                                   uint32_t mask_key = 0x12345678) -> vector<byte>;
    TEMPEST_API auto decode_websocket_frame(span<const byte> data) -> expected<ws_message, ws_error>;
    TEMPEST_API auto decode_websocket_frame(span<const byte> data, size_t& bytes_consumed)
        -> expected<ws_message, ws_error>;

    TEMPEST_API auto serialize_telemetry_frame_json(const telemetry_frame& frame) -> string;
    TEMPEST_API auto create_telemetry_frame_from_capture(uint64_t frame_index, const capture_session_data& data)
        -> telemetry_frame;
} // namespace tempest::profiler

#endif // tempest_profiler_websocket_hpp
