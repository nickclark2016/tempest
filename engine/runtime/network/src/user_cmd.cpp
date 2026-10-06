#include <tempest/network/user_cmd.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/network/bit_stream.hpp>
#include <tempest/network/packet.hpp>
#include <tempest/utility.hpp>

namespace tempest::network
{
    auto write_user_cmd(bit_writer& writer, const user_cmd& cmd) -> void
    {
        writer.write_u32(cmd.tick);
        writer.write_quantized_float(cmd.forward_move, -1.0F, 1.0F, 8);
        writer.write_quantized_float(cmd.right_move, -1.0F, 1.0F, 8);
        writer.write_angle(cmd.view_yaw, 16);
        writer.write_u16(cmd.buttons);
    }

    auto read_user_cmd(bit_reader& reader) -> optional<user_cmd>
    {
        const auto tick = reader.read_u32();
        const auto forward_move = reader.read_quantized_float(-1.0F, 1.0F, 8);
        const auto right_move = reader.read_quantized_float(-1.0F, 1.0F, 8);
        const auto view_yaw = reader.read_angle(16);
        const auto buttons = reader.read_u16();

        if (reader.has_overflowed())
        {
            return nullopt;
        }

        return user_cmd{
            .tick = tick,
            .forward_move = forward_move,
            .right_move = right_move,
            .view_yaw = view_yaw,
            .buttons = buttons,
        };
    }

    auto write_input_packet(bit_writer& writer, uint64_t session_id, uint32_t sequence_id,
                            uint32_t ack_sequence, uint32_t ack_bitfield,
                            span<const user_cmd> commands) -> void
    {
        if (commands.empty())
        {
            return;
        }

        auto header = packet_header{};
        header.protocol_magic = packet_header::default_protocol_magic;
        header.protocol_version = packet_header::default_protocol_version;
        header.session_id = session_id;
        header.sequence_id = sequence_id;
        header.ack_sequence_id = ack_sequence;
        header.ack_bitfield = ack_bitfield;
        header.type = static_cast<uint8_t>(packet_type::input);

        writer.write_bytes(span<const byte>{reinterpret_cast<const byte*>(&header), sizeof(packet_header)});

        const auto count = min(commands.size(), max_commands_per_packet);
        const auto count_code = static_cast<uint32_t>(count - 1U);
        writer.write_bits(count_code, 2);

        for (size_t i = 0; i < count; ++i)
        {
            write_user_cmd(writer, commands[i]);
        }
    }

    auto read_input_packet(bit_reader& reader, packet_header& out_header,
                           span<user_cmd> out_commands) -> size_t
    {
        out_header = packet_header{};

        const auto initial_bit_pos = reader.bit_position();

        // Check if the reader starts with a packet_header
        if (reader.remaining_bits() >= sizeof(packet_header) * 8)
        {
            packet_header candidate_header{};
            if (reader.read_bytes(span<byte>{reinterpret_cast<byte*>(&candidate_header), sizeof(packet_header)}))
            {
                if (candidate_header.protocol_magic == packet_header::default_protocol_magic &&
                    candidate_header.protocol_version == packet_header::default_protocol_version &&
                    candidate_header.type == static_cast<uint8_t>(packet_type::input))
                {
                    out_header = candidate_header;
                }
                else
                {
                    // Not a packet header; seek back to initial position to interpret as payload directly
                    reader.seek_bits(initial_bit_pos);
                }
            }
            else
            {
                reader.seek_bits(initial_bit_pos);
            }
        }

        if (reader.remaining_bits() < 2)
        {
            return 0;
        }

        const auto count_code = reader.read_bits(2);
        if (reader.has_overflowed())
        {
            return 0;
        }

        const auto command_count = static_cast<size_t>(count_code + 1U);
        auto read_count = static_cast<size_t>(0);

        for (size_t i = 0; i < command_count; ++i)
        {
            auto cmd_opt = read_user_cmd(reader);
            if (!cmd_opt.has_value())
            {
                break;
            }

            if (read_count < out_commands.size())
            {
                out_commands[read_count++] = *cmd_opt;
            }
        }

        return read_count;
    }

    auto read_input_packet(bit_reader& reader, span<user_cmd> out_commands) -> size_t
    {
        auto header = packet_header{};
        return read_input_packet(reader, header, out_commands);
    }
} // namespace tempest::network
