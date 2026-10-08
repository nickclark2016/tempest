#ifndef TEMPEST_PHYSICS_CHARACTER_PROTOCOL_HPP
#define TEMPEST_PHYSICS_CHARACTER_PROTOCOL_HPP

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/network/bit_stream.hpp>
#include <tempest/optional.hpp>
#include <tempest/physics/character_snapshot.hpp>
#include <tempest/vector.hpp>

namespace tempest::physics
{
    /// @brief Snapshot state for a single connected player character identified by session ID.
    struct player_snapshot_entry
    {
        uint64_t session_id = 0;
        character_snapshot snapshot{};

        auto operator==(const player_snapshot_entry& other) const noexcept -> bool = default;
    };

    /// @brief Authoritative multi-player world state snapshot dispatched from server to client.
    struct world_snapshot_packet
    {
        static constexpr uint16_t max_players_per_snapshot = 64;

        uint32_t server_tick = 0;
        uint32_t ack_client_tick = 0;
        vector<player_snapshot_entry> players{};

        auto operator==(const world_snapshot_packet& other) const noexcept -> bool = default;
    };

    /// @brief Writes a character_snapshot to the bitstream with full 32-bit floating point precision.
    TEMPEST_API auto write_character_snapshot(network::bit_writer& writer, const character_snapshot& snapshot) -> void;

    /// @brief Reads a character_snapshot from the bitstream.
    TEMPEST_API auto read_character_snapshot(network::bit_reader& reader) -> optional<character_snapshot>;

    /// @brief Writes a world_snapshot_packet to the bitstream.
    TEMPEST_API auto write_world_snapshot(network::bit_writer& writer, const world_snapshot_packet& packet) -> void;

    /// @brief Reads a world_snapshot_packet from the bitstream.
    TEMPEST_API auto read_world_snapshot(network::bit_reader& reader, world_snapshot_packet& out_packet) -> bool;
} // namespace tempest::physics

#endif // TEMPEST_PHYSICS_CHARACTER_PROTOCOL_HPP
