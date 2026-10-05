#ifndef tempest_network_bit_stream_hpp
#define tempest_network_bit_stream_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/quat.hpp>
#include <tempest/span.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vector.hpp>

namespace tempest::network
{
    class TEMPEST_API bit_writer
    {
      public:
        static constexpr size_t relative_position_bits_per_axis = 16;
        static constexpr size_t octahedral_bits_per_axis = 8;

        bit_writer() = default;
        explicit bit_writer(size_t initial_byte_capacity);

        auto write_bits(uint32_t value, size_t bit_count) -> void;
        auto write_bool(bool value) -> void;

        auto write_u8(uint8_t value) -> void
        {
            write_bits(value, 8); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) 8 bits
        }

        auto write_u16(uint16_t value) -> void
        {
            write_bits(value, 16); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) 16 bits
        }

        auto write_u32(uint32_t value) -> void
        {
            write_bits(value, 32); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) 32 bits
        }

        auto write_u64(uint64_t value) -> void;
        auto write_bytes(span<const byte> src) -> void;

        // Domain Quantization
        auto write_quantized_float(float value, float min_val, float max_val, size_t bits) -> void;
        auto write_chunk_relative_position(const math::float3& world_pos, const math::float3& chunk_origin,
                                           float chunk_extent_m, size_t bits_per_axis = relative_position_bits_per_axis)
            -> void;
        auto write_half(float value) -> void;
        auto write_angle(float radians, size_t bits) -> void;
        auto write_octahedral_unit_vector(const math::float3& vec, size_t bits_per_axis = octahedral_bits_per_axis)
            -> void;
        auto write_smallest_three_quaternion(const math::fquat& quat) -> void;

        auto flush() -> void;
        auto clear() noexcept -> void;

        [[nodiscard]] auto data() const noexcept -> span<const byte>;
        [[nodiscard]] auto bit_count() const noexcept -> size_t
        {
            return _bit_count;
        }
        [[nodiscard]] auto byte_count() const noexcept -> size_t
        {
            // Ceiling division to convert bit count to byte count
            return (_bit_count + 7) >> 3; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        }

      private:
        vector<byte> _buffer;
        size_t _bit_count = 0;
    };

    class TEMPEST_API bit_reader
    {
      public:
        explicit bit_reader(span<const byte> data) noexcept;

        auto read_bits(size_t bit_count) -> uint32_t;
        auto read_bool() -> bool;

        auto read_u8() -> uint8_t
        {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) 8 bits
            return static_cast<uint8_t>(read_bits(8));
        }

        auto read_u16() -> uint16_t
        {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) 16 bits
            return static_cast<uint16_t>(read_bits(16));
        }

        auto read_u32() -> uint32_t
        {
            return read_bits(32); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) 32 bits
        }

        auto read_u64() -> uint64_t;
        auto read_bytes(span<byte> dest) -> bool;

        // Dequantization
        auto read_quantized_float(float min_val, float max_val, size_t bits) -> float;
        auto read_chunk_relative_position(const math::float3& chunk_origin, float chunk_extent_m,
                                          size_t bits_per_axis = 16) -> math::float3;
        auto read_half() -> float;
        auto read_angle(size_t bits) -> float;
        auto read_octahedral_unit_vector(size_t bits_per_axis = 8) -> math::float3;
        auto read_smallest_three_quaternion() -> math::fquat;

        [[nodiscard]] auto has_overflowed() const noexcept -> bool
        {
            return _overflowed;
        }
        [[nodiscard]] auto remaining_bits() const noexcept -> size_t;
        [[nodiscard]] auto bit_position() const noexcept -> size_t
        {
            return _bit_position;
        }

      private:
        span<const byte> _data;
        size_t _bit_position{0};
        bool _overflowed{false};
    };
} // namespace tempest::network

#endif // tempest_network_bit_stream_hpp
