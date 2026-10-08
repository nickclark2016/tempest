#ifndef TEMPEST_NETWORK_PROTOCOL_HPP
#define TEMPEST_NETWORK_PROTOCOL_HPP

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/network/bit_stream.hpp>
#include <tempest/network/packet.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>

namespace tempest::network
{
    /// @brief Handshake connection request payload transmitted from client to server.
    struct connect_request_payload
    {
        static constexpr uint16_t default_client_version = packet_header::default_protocol_version;

        uint16_t client_version = default_client_version;
        uint64_t client_nonce = 0;

        auto operator==(const connect_request_payload& other) const noexcept -> bool = default;
    };

    /// @brief Handshake connection response payload transmitted from server to client upon successful session allocation.
    struct connect_accepted_payload
    {
        static constexpr uint16_t default_tick_rate = 60;

        uint64_t session_id = 0;
        uint32_t player_slot = 0;
        uint32_t initial_server_tick = 0;
        uint16_t tick_rate = default_tick_rate;

        auto operator==(const connect_accepted_payload& other) const noexcept -> bool = default;
    };

    /// @brief Reasons for graceful connection termination.
    enum class disconnect_reason : uint8_t
    {
        user_quit = 0,
        timed_out = 1,
        server_shutdown = 2,
        protocol_mismatch = 3,
        kicked = 4,
    };

    /// @brief Disconnect notification payload transmitted when terminating a session.
    struct disconnect_payload
    {
        static constexpr disconnect_reason default_reason = disconnect_reason::user_quit;

        disconnect_reason reason = default_reason;

        auto operator==(const disconnect_payload& other) const noexcept -> bool = default;
    };

    /// @brief Serializes a connection request payload into a bit_writer.
    TEMPEST_API auto write_connect_request(bit_writer& writer, const connect_request_payload& payload) -> void;

    /// @brief Deserializes a connection request payload from a bit_reader.
    TEMPEST_API auto read_connect_request(bit_reader& reader) -> optional<connect_request_payload>;

    /// @brief Serializes a connection accepted payload into a bit_writer.
    TEMPEST_API auto write_connect_accepted(bit_writer& writer, const connect_accepted_payload& payload) -> void;

    /// @brief Deserializes a connection accepted payload from a bit_reader.
    TEMPEST_API auto read_connect_accepted(bit_reader& reader) -> optional<connect_accepted_payload>;

    /// @brief Serializes a disconnect payload into a bit_writer.
    TEMPEST_API auto write_disconnect(bit_writer& writer, const disconnect_payload& payload) -> void;

    /// @brief Deserializes a disconnect payload from a bit_reader.
    TEMPEST_API auto read_disconnect(bit_reader& reader) -> optional<disconnect_payload>;
} // namespace tempest::network

#endif // TEMPEST_NETWORK_PROTOCOL_HPP
