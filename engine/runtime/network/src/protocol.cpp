#include <tempest/network/protocol.hpp>

namespace tempest::network
{
    auto write_connect_request(bit_writer& writer, const connect_request_payload& payload) -> void
    {
        writer.write_u16(payload.client_version);
        writer.write_u64(payload.client_nonce);
    }

    auto read_connect_request(bit_reader& reader) -> optional<connect_request_payload>
    {
        const auto client_version = reader.read_u16();
        const auto client_nonce = reader.read_u64();

        if (reader.has_overflowed())
        {
            return nullopt;
        }

        return connect_request_payload{
            .client_version = client_version,
            .client_nonce = client_nonce,
        };
    }

    auto write_connect_accepted(bit_writer& writer, const connect_accepted_payload& payload) -> void
    {
        writer.write_u64(payload.session_id);
        writer.write_u32(payload.player_slot);
        writer.write_u32(payload.initial_server_tick);
        writer.write_u16(payload.tick_rate);
    }

    auto read_connect_accepted(bit_reader& reader) -> optional<connect_accepted_payload>
    {
        const auto session_id = reader.read_u64();
        const auto player_slot = reader.read_u32();
        const auto initial_server_tick = reader.read_u32();
        const auto tick_rate = reader.read_u16();

        if (reader.has_overflowed())
        {
            return nullopt;
        }

        return connect_accepted_payload{
            .session_id = session_id,
            .player_slot = player_slot,
            .initial_server_tick = initial_server_tick,
            .tick_rate = tick_rate,
        };
    }

    auto write_disconnect(bit_writer& writer, const disconnect_payload& payload) -> void
    {
        writer.write_u8(static_cast<uint8_t>(payload.reason));
    }

    auto read_disconnect(bit_reader& reader) -> optional<disconnect_payload>
    {
        const auto reason_code = reader.read_u8();

        if (reader.has_overflowed())
        {
            return nullopt;
        }

        return disconnect_payload{
            .reason = static_cast<disconnect_reason>(reason_code),
        };
    }
} // namespace tempest::network
