#include <tempest/physics/character_protocol.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/bit.hpp>

namespace tempest::physics
{
    namespace
    {
        inline auto write_f32(network::bit_writer& writer, float value) -> void
        {
            writer.write_u32(bit_cast<uint32_t>(value));
        }

        inline auto read_f32(network::bit_reader& reader) -> float
        {
            return bit_cast<float>(reader.read_u32());
        }
    } // namespace

    auto write_character_snapshot(network::bit_writer& writer, const character_snapshot& snapshot) -> void
    {
        writer.write_u32(snapshot.tick());

        write_f32(writer, snapshot.position().x);
        write_f32(writer, snapshot.position().y);
        write_f32(writer, snapshot.position().z);

        write_f32(writer, snapshot.rotation().x);
        write_f32(writer, snapshot.rotation().y);
        write_f32(writer, snapshot.rotation().z);
        write_f32(writer, snapshot.rotation().w);

        write_f32(writer, snapshot.linear_velocity().x);
        write_f32(writer, snapshot.linear_velocity().y);
        write_f32(writer, snapshot.linear_velocity().z);

        write_f32(writer, snapshot.motion.angular_velocity.x);
        write_f32(writer, snapshot.motion.angular_velocity.y);
        write_f32(writer, snapshot.motion.angular_velocity.z);

        writer.write_bits(snapshot.is_grounded ? 1U : 0U, 1);
        writer.write_bits(static_cast<uint32_t>(snapshot.current_ground_state), 2);

        write_f32(writer, snapshot.ground_normal.x);
        write_f32(writer, snapshot.ground_normal.y);
        write_f32(writer, snapshot.ground_normal.z);
    }

    auto read_character_snapshot(network::bit_reader& reader) -> optional<character_snapshot>
    {
        const auto tick = reader.read_u32();

        const auto px = read_f32(reader);
        const auto py = read_f32(reader);
        const auto pz = read_f32(reader);

        const auto rx = read_f32(reader);
        const auto ry = read_f32(reader);
        const auto rz = read_f32(reader);
        const auto rw = read_f32(reader);

        const auto vx = read_f32(reader);
        const auto vy = read_f32(reader);
        const auto vz = read_f32(reader);

        const auto wx = read_f32(reader);
        const auto wy = read_f32(reader);
        const auto wz = read_f32(reader);

        const auto is_grounded = reader.read_bits(1) != 0U;
        const auto ground_state_val = reader.read_bits(2);

        const auto nx = read_f32(reader);
        const auto ny = read_f32(reader);
        const auto nz = read_f32(reader);

        if (reader.has_overflowed())
        {
            return nullopt;
        }

        auto result = character_snapshot{};
        result.motion.tick = tick;
        result.motion.position = math::vec3<float>{px, py, pz};
        result.motion.rotation = math::quat<float>{rx, ry, rz, rw};
        result.motion.linear_velocity = math::vec3<float>{vx, vy, vz};
        result.motion.angular_velocity = math::vec3<float>{wx, wy, wz};
        result.is_grounded = is_grounded;
        result.current_ground_state = static_cast<jolt::shim::ground_state>(ground_state_val);
        result.ground_normal = math::vec3<float>{nx, ny, nz};

        return result;
    }

    auto write_world_snapshot(network::bit_writer& writer, const world_snapshot_packet& packet) -> void
    {
        writer.write_u32(packet.server_tick);
        writer.write_u32(packet.ack_client_tick);

        const auto count = static_cast<uint16_t>(min<size_t>(packet.players.size(), world_snapshot_packet::max_players_per_snapshot));
        writer.write_u16(count);

        for (size_t i = 0; i < count; ++i)
        {
            writer.write_u64(packet.players[i].session_id);
            write_character_snapshot(writer, packet.players[i].snapshot);
        }
    }

    auto read_world_snapshot(network::bit_reader& reader, world_snapshot_packet& out_packet) -> bool
    {
        const auto server_tick = reader.read_u32();
        const auto ack_client_tick = reader.read_u32();
        const auto player_count = reader.read_u16();

        if (reader.has_overflowed() || player_count > world_snapshot_packet::max_players_per_snapshot)
        {
            return false;
        }

        out_packet.server_tick = server_tick;
        out_packet.ack_client_tick = ack_client_tick;
        out_packet.players.clear();
        out_packet.players.reserve(player_count);

        for (size_t i = 0; i < player_count; ++i)
        {
            const auto session_id = reader.read_u64();
            auto snapshot_opt = read_character_snapshot(reader);
            if (!snapshot_opt.has_value() || reader.has_overflowed())
            {
                return false;
            }

            out_packet.players.push_back(player_snapshot_entry{
                .session_id = session_id,
                .snapshot = *snapshot_opt,
            });
        }

        return !reader.has_overflowed();
    }
} // namespace tempest::physics
