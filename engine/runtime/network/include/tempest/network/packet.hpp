#ifndef tempest_network_packet_hpp
#define tempest_network_packet_hpp

#include <tempest/int.hpp>

namespace tempest::network
{
    enum class packet_type : uint8_t
    {
        connect_request = 0,
        connect_challenge = 1,
        connect_accepted = 2,
        disconnect = 3,
        input = 4,
        snapshot = 5,
        ping = 6,
        pong = 7,
    };

#pragma pack(push, 1)
    struct packet_header
    {
        static constexpr uint32_t default_protocol_magic = 0x544D5053; // "TMPS"
        static constexpr uint16_t default_protocol_version = 1;
        static constexpr uint32_t ack_bitfield_width = 32;

        uint32_t protocol_magic = default_protocol_magic;
        uint16_t protocol_version = default_protocol_version;
        uint64_t session_id = 0;              // 64-bit client/session ID
        uint32_t sequence_id = 0;
        uint32_t ack_sequence_id = 0;
        uint32_t ack_bitfield = 0;            // ACKs for previous 32 packets
        uint8_t type = 0;                     // packet_type
    };
#pragma pack(pop)

    inline constexpr auto packet_header_size = 27; // Size of the packed packet_header structure
    static_assert(sizeof(packet_header) == packet_header_size, "packet_header must be exactly 27 bytes packed");
} // namespace tempest::network

#endif // tempest_network_packet_hpp
