#ifndef tempest_network_user_cmd_hpp
#define tempest_network_user_cmd_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>

namespace tempest::network
{
    class bit_writer;
    class bit_reader;
    struct packet_header;

    enum user_button : uint16_t // NOLINT -- underlying type for bitmask that is 16 bits wide on the wire
    {
        user_button_none = 0,
        user_button_jump = 1U << 0,
        user_button_crouch = 1U << 1,
        user_button_sprint = 1U << 2,
    };

    struct user_cmd
    {
        uint32_t tick = 0;
        float forward_move = 0.0F; // [-1.0, 1.0]
        float right_move = 0.0F;   // [-1.0, 1.0]
        float view_yaw = 0.0F;     // radians
        uint16_t buttons = user_button_none;

        auto operator==(const user_cmd& other) const noexcept -> bool = default;
    };

    inline constexpr size_t max_redundant_commands = 3;
    inline constexpr size_t max_commands_per_packet = 1 + max_redundant_commands;

    TEMPEST_API auto write_user_cmd(bit_writer& writer, const user_cmd& cmd) -> void;
    TEMPEST_API auto read_user_cmd(bit_reader& reader) -> optional<user_cmd>;

    TEMPEST_API auto write_input_packet(bit_writer& writer, uint64_t session_id, uint32_t sequence_id,
                                        uint32_t ack_sequence, uint32_t ack_bitfield,
                                        span<const user_cmd> commands) -> void;

    TEMPEST_API auto read_input_packet(bit_reader& reader, packet_header& out_header,
                                       span<user_cmd> out_commands) -> size_t;

    TEMPEST_API auto read_input_packet(bit_reader& reader, span<user_cmd> out_commands) -> size_t;
} // namespace tempest::network

#endif // tempest_network_user_cmd_hpp
