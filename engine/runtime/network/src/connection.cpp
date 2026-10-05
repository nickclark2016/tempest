#include <tempest/network/connection.hpp>

#include <tempest/utility.hpp>

namespace tempest::network
{
    connection::connection(uint64_t session_id, const endpoint& remote_endpoint, ipacket_security* security)
        : _session_id{session_id}, _remote_endpoint{remote_endpoint}, _security{security}
    {
    }

    auto connection::build_packet(packet_type type, span<const byte> payload, span<byte> out_buffer,
                                  chrono::steady_clock::time_point now) -> size_t
    {
        auto final_payload = span<const byte>{payload};

        if (_security != nullptr)
        {
            _encryption_scratch.clear();
            const auto pending_sequence_id = _ack_tracker.next_sequence();
            if (_security->encrypt_payload(_session_id, pending_sequence_id, payload, _encryption_scratch))
            {
                final_payload = span<const byte>(_encryption_scratch.data(), _encryption_scratch.size());
            }
        }

        const auto total_size = sizeof(packet_header) + final_payload.size();
        if (out_buffer.size() < total_size)
        {
            return 0;
        }

        const auto sequence_id = _ack_tracker.register_outgoing_packet(now);

        auto header = packet_header{};
        header.protocol_magic = packet_header::default_protocol_magic;
        header.protocol_version = packet_header::default_protocol_version;
        header.session_id = _session_id;
        header.sequence_id = sequence_id;
        _ack_tracker.populate_header_ack_fields(header);
        header.type = static_cast<uint8_t>(type);

        tempest::memcpy(out_buffer.data(), &header, sizeof(packet_header));
        if (!final_payload.empty())
        {
            tempest::memcpy(out_buffer.data() + sizeof(packet_header), final_payload.data(), final_payload.size());
        }

        return total_size;
    }

    auto connection::build_packet(packet_type type, span<const byte> payload, chrono::steady_clock::time_point now)
        -> vector<byte>
    {
        auto estimated_size = sizeof(packet_header) + payload.size();
        if (_security != nullptr)
        {
            estimated_size += 64; // Additional headroom for auth tags/ciphers
        }
        if (_send_scratch.size() < estimated_size)
        {
            _send_scratch.resize(estimated_size);
        }

        auto written_bytes = build_packet(type, payload, span<byte>(_send_scratch.data(), _send_scratch.size()), now);
        if (written_bytes == 0)
        {
            const auto needed_size = sizeof(packet_header) + _encryption_scratch.size();
            _send_scratch.resize(needed_size);
            written_bytes = build_packet(type, payload, span<byte>(_send_scratch.data(), _send_scratch.size()), now);
        }

        auto output_packet = vector<byte>(written_bytes);
        if (written_bytes > 0)
        {
            tempest::memcpy(output_packet.data(), _send_scratch.data(), written_bytes);
        }

        return output_packet;
    }

    auto connection::process_packet(span<const byte> packet_data, chrono::steady_clock::time_point now)
        -> optional<incoming_packet>
    {
        if (packet_data.size() < sizeof(packet_header))
        {
            return nullopt;
        }

        auto header = packet_header{};
        tempest::memcpy(&header, packet_data.data(), sizeof(packet_header));

        if (header.protocol_magic != packet_header::default_protocol_magic || header.protocol_version != packet_header::default_protocol_version)
        {
            return nullopt;
        }

        if (_session_id != 0U && header.session_id != _session_id)
        {
            return nullopt;
        }

        const auto raw_payload = packet_data.subspan(sizeof(packet_header));

        if (_security != nullptr)
        {
            _decryption_scratch.clear();
            if (!_security->decrypt_payload(_session_id, header.sequence_id, raw_payload, _decryption_scratch))
            {
                return nullopt;
            }

            _ack_tracker.process_incoming_header(header, now);
            return incoming_packet{
                .header = header,
                .payload = span<const byte>(_decryption_scratch.data(), _decryption_scratch.size()),
            };
        }

        _ack_tracker.process_incoming_header(header, now);

        return incoming_packet{
            .header = header,
            .payload = raw_payload,
        };
    }
} // namespace tempest::network
