#include <tempest/network/bit_stream.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/bit.hpp>
#include <tempest/half.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/utility.hpp>

namespace tempest::network
{
    namespace
    {
        inline auto sign_not_zero(float val) noexcept -> float
        {
            return (val >= 0.0F) ? 1.0F : -1.0F;
        }
    } // namespace

    // =========================================================================
    // bit_writer Implementation
    // =========================================================================

    bit_writer::bit_writer(size_t initial_byte_capacity)
    {
        _buffer.reserve(initial_byte_capacity);
    }

    auto bit_writer::write_bits(uint32_t value, size_t bit_count) -> void
    {
        if (bit_count == 0)
        {
            return;
        }

        bit_count = tempest::min<size_t>(bit_count, 32U); // NOLINT

        if (bit_count < 32) // NOLINT
        {
            value &= ((1U << bit_count) - 1U); // Mask out any bits above the specified bit count
        }

        auto remaining_bits = bit_count;
        auto bits_written = static_cast<size_t>(0);

        while (remaining_bits > 0)
        {
            const auto byte_index = _bit_count / 8;
            const auto bit_offset = _bit_count % 8;
            const auto bits_free_in_byte = 8 - bit_offset;
            const auto bits_to_write = min(remaining_bits, bits_free_in_byte);

            const auto mask = (1U << bits_to_write) - 1U;
            const auto bits = (value >> bits_written) & mask;

            if (byte_index >= _buffer.size())
            {
                _buffer.push_back(byte{0});
            }

            _buffer[byte_index] |= static_cast<byte>(bits << bit_offset);

            _bit_count += bits_to_write;
            bits_written += bits_to_write;
            remaining_bits -= bits_to_write;
        }
    }

    auto bit_writer::write_bool(bool value) -> void
    {
        write_bits(value ? 1U : 0U, 1);
    }

    auto bit_writer::write_u64(uint64_t value) -> void
    {
        write_bits(static_cast<uint32_t>(value & 0xFFFFFFFFULL), 32);         // NOLINT
        write_bits(static_cast<uint32_t>((value >> 32) & 0xFFFFFFFFULL), 32); // NOLINT
    }

    auto bit_writer::write_bytes(span<const byte> src) -> void
    {
        if (src.empty())
        {
            return;
        }

        if ((_bit_count % 8) == 0)
        {
            const auto current_byte_count = _bit_count / 8;
            _buffer.resize(current_byte_count + src.size());
            tempest::memcpy(_buffer.data() + current_byte_count, src.data(), src.size());
            _bit_count += src.size() * 8;
        }
        else
        {
            for (const auto byte_val : src)
            {
                write_bits(to_integer<uint32_t>(byte_val), 8);
            }
        }
    }

    auto bit_writer::write_quantized_float(float value, float min_val, float max_val, size_t bits) -> void
    {
        if (bits == 0)
        {
            return;
        }

        bits = tempest::min<size_t>(bits, 32U); // NOLINT

        const auto clamped_value = clamp(value, min_val, max_val); // NOLINT
        const auto range = max_val - min_val;
        const auto max_int = (bits == 32) ? 0xFFFFFFFFULL : ((1ULL << bits) - 1ULL);

        const auto normalized = (range > 0.0F)
            ? (static_cast<double>(clamped_value - min_val) / static_cast<double>(range))
            : 0.0;
        const auto scaled = normalized * static_cast<double>(max_int);
        const auto quantized = static_cast<uint32_t>(min(static_cast<double>(max_int), scaled + 0.5));
        write_bits(quantized, bits);
    }

    auto bit_writer::write_chunk_relative_position(const math::float3& world_pos, const math::float3& chunk_origin,
                                                   float chunk_extent_m, size_t bits_per_axis) -> void
    {
        write_quantized_float(world_pos.x - chunk_origin.x, 0.0F, chunk_extent_m, bits_per_axis);
        write_quantized_float(world_pos.y - chunk_origin.y, 0.0F, chunk_extent_m, bits_per_axis);
        write_quantized_float(world_pos.z - chunk_origin.z, 0.0F, chunk_extent_m, bits_per_axis);
    }

    auto bit_writer::write_half(float value) -> void
    {
        write_u16(float_to_half(value));
    }

    auto bit_writer::write_angle(float radians, size_t bits) -> void
    {
        if (bits == 0)
        {
            return;
        }

        bits = tempest::min<size_t>(bits, 32U); // NOLINT

        constexpr auto two_pi = 2.0F * math::constants::pi<float>;
        auto normalized = math::fmod(radians, two_pi);
        if (normalized < 0.0F)
        {
            normalized += two_pi;
        }

        const auto max_int = (bits == 32) ? 0xFFFFFFFFULL : (1ULL << bits); // NOLINT
        const auto normalized_ratio = normalized / two_pi;
        const auto mask = (bits == 32) ? 0xFFFFFFFFULL : ((1ULL << bits) - 1ULL);
        const auto quantized = static_cast<uint32_t>(math::round(normalized_ratio * static_cast<float>(max_int))) &
                               static_cast<uint32_t>(mask);
        write_bits(quantized, bits);
    }

    auto bit_writer::write_octahedral_unit_vector(const math::float3& vec, size_t bits_per_axis) -> void
    {
        const auto l1_norm = math::abs(vec.x) + math::abs(vec.y) + math::abs(vec.z);
        auto proj_x = 0.0F;
        auto proj_y = 0.0F;
        if (l1_norm > 1e-7F) // NOLINT -- Avoid division by very small numbers
        {
            proj_x = vec.x / l1_norm;
            proj_y = vec.y / l1_norm;
            if (vec.z < 0.0F)
            {
                const auto previous_x = proj_x;
                proj_x = (1.0F - math::abs(proj_y)) * sign_not_zero(previous_x);
                proj_y = (1.0F - math::abs(previous_x)) * sign_not_zero(proj_y);
            }
        }

        write_quantized_float(proj_x, -1.0F, 1.0F, bits_per_axis);
        write_quantized_float(proj_y, -1.0F, 1.0F, bits_per_axis);
    }

    auto bit_writer::write_smallest_three_quaternion(const math::fquat& quat) -> void
    {
        const auto length = math::sqrt((quat.x * quat.x) + (quat.y * quat.y) + (quat.z * quat.z) + (quat.w * quat.w));
        auto normalized_quat = (length > 1e-7F) // NOLINT -- Avoid division by very small numbers
                                   ? math::fquat{quat.x / length, quat.y / length, quat.z / length, quat.w / length}
                                   : math::fquat{0.0F, 0.0F, 0.0F, 1.0F};

        auto largest_index = 0U;
        auto max_abs = math::abs(normalized_quat.x);
        if (math::abs(normalized_quat.y) > max_abs)
        {
            largest_index = 1;
            max_abs = math::abs(normalized_quat.y);
        }
        if (math::abs(normalized_quat.z) > max_abs)
        {
            largest_index = 2;
            max_abs = math::abs(normalized_quat.z);
        }
        if (math::abs(normalized_quat.w) > max_abs)
        {
            largest_index = 3;
            max_abs = math::abs(normalized_quat.w);
        }

        if (normalized_quat[largest_index] < 0.0F)
        {
            normalized_quat.x = -normalized_quat.x;
            normalized_quat.y = -normalized_quat.y;
            normalized_quat.z = -normalized_quat.z;
            normalized_quat.w = -normalized_quat.w;
        }

        write_bits(largest_index, 2);

        constexpr float inv_sqrt2 = 0.7071067811865475f; // NOLINT -- sqrt(2) / 2
        for (uint32_t component_index = 0; component_index < 4; ++component_index)
        {
            if (component_index != largest_index)
            {
                // NOLINTNEXTLINE -- Quantizing quaternion component in 10 bits
                write_quantized_float(normalized_quat[component_index], -inv_sqrt2, inv_sqrt2, 10);
            }
        }
    }

    auto bit_writer::flush() -> void
    {
        const auto unaligned_bits = _bit_count % 8; // NOLINT -- Calculate unaligned bits in the current byte
        if (unaligned_bits != 0)
        {
            write_bits(0, 8 - unaligned_bits); // NOLINT -- Align to next byte boundary
        }
    }

    auto bit_writer::clear() noexcept -> void
    {
        _buffer.clear();
        _bit_count = 0;
    }

    auto bit_writer::data() const noexcept -> span<const byte>
    {
        return span<const byte>{_buffer.data(), byte_count()};
    }

    // =========================================================================
    // bit_reader Implementation
    // =========================================================================

    bit_reader::bit_reader(span<const byte> data) noexcept : _data{data}
    {
    }

    auto bit_reader::remaining_bits() const noexcept -> size_t
    {
        const auto total_bits = _data.size() * 8;
        return (_bit_position < total_bits) ? (total_bits - _bit_position) : 0;
    }

    auto bit_reader::read_bits(size_t bit_count) -> uint32_t
    {
        if (bit_count == 0)
        {
            return 0;
        }

        bit_count = tempest::min<size_t>(bit_count, 32U); // NOLINT

        if (_bit_position + bit_count > _data.size() * 8) // NOLINT, 8 bits in a byte
        {
            _overflowed = true;
            return 0;
        }

        uint32_t result = 0;
        size_t remaining_bits = bit_count;
        size_t bits_read = 0;

        while (remaining_bits > 0)
        {
            const auto byte_index = _bit_position / 8;
            const auto bit_offset = _bit_position % 8;
            const auto bits_avail_in_byte = 8 - bit_offset;
            const auto bits_to_read = min(remaining_bits, bits_avail_in_byte);

            const auto mask = (1U << bits_to_read) - 1U;
            const auto raw_byte = to_integer<uint32_t>(_data[byte_index]);
            const auto bits = (raw_byte >> bit_offset) & mask;

            result |= (bits << bits_read);

            _bit_position += bits_to_read;
            bits_read += bits_to_read;
            remaining_bits -= bits_to_read;
        }

        return result;
    }

    auto bit_reader::read_bool() -> bool
    {
        return read_bits(1) != 0;
    }

    auto bit_reader::read_u64() -> uint64_t
    {
        const auto low_part = static_cast<uint64_t>(read_bits(32));
        const auto high_part = static_cast<uint64_t>(read_bits(32));
        return low_part | (high_part << 32); // NOLINT Combine low and high parts into a 64-bit value
    }

    auto bit_reader::read_bytes(span<byte> dest) -> bool
    {
        if (dest.empty())
        {
            return true;
        }

        if (remaining_bits() < dest.size() * 8)
        {
            _overflowed = true;
            return false;
        }

        if ((_bit_position % 8) == 0)
        {
            const auto byte_offset = _bit_position / 8;
            tempest::memcpy(dest.data(), _data.data() + byte_offset, dest.size());
            _bit_position += dest.size() * 8;
            return true;
        }

        for (auto& dest_byte : dest)
        {
            dest_byte = static_cast<byte>(read_bits(8));
        }
        return true;
    }

    auto bit_reader::read_quantized_float(float min_val, float max_val, size_t bits) -> float
    {
        if (bits == 0)
        {
            return min_val;
        }

        bits = tempest::min<size_t>(bits, 32U); // NOLINT

        const auto raw_value = read_bits(bits);
        const auto max_int = (bits == 32) ? 0xFFFFFFFFULL : ((1ULL << bits) - 1ULL);
        const auto range = max_val - min_val;
        const auto normalized = static_cast<double>(raw_value) / static_cast<double>(max_int);
        return static_cast<float>(static_cast<double>(min_val) + (normalized * static_cast<double>(range)));
    }

    auto bit_reader::read_chunk_relative_position(const math::float3& chunk_origin, float chunk_extent_m,
                                                  size_t bits_per_axis) -> math::float3
    {
        const auto relative_x = read_quantized_float(0.0F, chunk_extent_m, bits_per_axis);
        const auto relative_y = read_quantized_float(0.0F, chunk_extent_m, bits_per_axis);
        const auto relative_z = read_quantized_float(0.0F, chunk_extent_m, bits_per_axis);
        return math::float3{chunk_origin.x + relative_x, chunk_origin.y + relative_y, chunk_origin.z + relative_z};
    }

    auto bit_reader::read_half() -> float
    {
        return half_to_float(read_u16());
    }

    auto bit_reader::read_angle(size_t bits) -> float
    {
        if (bits == 0)
        {
            return 0.0F;
        }

        bits = tempest::min<size_t>(bits, 32U); // NOLINT

        constexpr auto two_pi = 2.0F * math::constants::pi<float>;
        const auto raw_value = read_bits(bits);
        const auto max_int = (bits == 32) ? 0xFFFFFFFFULL : (1ULL << bits);
        return (static_cast<float>(raw_value) / static_cast<float>(max_int)) * two_pi;
    }

    auto bit_reader::read_octahedral_unit_vector(size_t bits_per_axis) -> math::float3
    {
        auto proj_x = read_quantized_float(-1.0F, 1.0F, bits_per_axis);
        auto proj_y = read_quantized_float(-1.0F, 1.0F, bits_per_axis);

        auto proj_z = 1.0F - math::abs(proj_x) - math::abs(proj_y);
        if (proj_z < 0.0F)
        {
            const auto previous_x = proj_x;
            proj_x = (1.0F - math::abs(proj_y)) * sign_not_zero(previous_x);
            proj_y = (1.0F - math::abs(previous_x)) * sign_not_zero(proj_y);
        }

        const auto length = math::sqrt((proj_x * proj_x) + (proj_y * proj_y) + (proj_z * proj_z));
        if (length > 1e-7F) // NOLINT Prevent division by zero
        {
            return math::float3{proj_x / length, proj_y / length, proj_z / length};
        }
        return math::float3{0.0F, 0.0F, 1.0F};
    }

    auto bit_reader::read_smallest_three_quaternion() -> math::fquat
    {
        const auto largest_index = read_bits(2);
        constexpr float inv_sqrt2 = 0.7071067811865475f; // NOLINT sqrt(2) / 2

        auto components = array<float, 4>{0.0F, 0.0F, 0.0F, 0.0F};
        auto sum_squares = 0.0F;

        for (uint32_t component_index = 0; component_index < 4; ++component_index)
        {
            if (component_index != largest_index)
            {
                // NOLINTNEXTLINE Quantized float read for quaternion component
                components[component_index] = read_quantized_float(-inv_sqrt2, inv_sqrt2, 10);
                sum_squares += components[component_index] * components[component_index];
            }
        }

        const auto reconstructed = math::sqrt(max(0.0F, 1.0F - sum_squares));
        components[largest_index] = reconstructed;

        const auto length = math::sqrt((components[0] * components[0]) + (components[1] * components[1]) +
                                       (components[2] * components[2]) + (components[3] * components[3]));
        if (length > 1e-7F) // NOLINT Prevent division by zero
        {
            return math::fquat{components[0] / length, components[1] / length, components[2] / length,
                               components[3] / length};
        }
        return math::fquat{0.0F, 0.0F, 0.0F, 1.0F};
    }

    auto bit_reader::seek_bits(size_t bit_position) noexcept -> void
    {
        _bit_position = bit_position;
        _overflowed = (_bit_position > (_data.size() * 8));
    }
} // namespace tempest::network

