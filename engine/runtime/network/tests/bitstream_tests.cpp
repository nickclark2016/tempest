#include <gtest/gtest.h>

#include <tempest/network/bit_stream.hpp>
#include <tempest/network/packet.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/bit.hpp>
#include <tempest/limits.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/quat.hpp>
#include <tempest/span.hpp>
#include <tempest/vec3.hpp>

namespace tempest::network::tests
{
    // =========================================================================
    // SECTION: Packet Header Framing Tests
    // =========================================================================

    /// @brief Verifies wire format layout, packing size of exactly 27 bytes, and default magic.
    TEST(bitstream_tests, packet_header_layout_and_magic)
    {
        // 1. Setup: Instantiate default packet header
        const auto default_header = packet_header{};

        // 2. Act: Inspect byte sizes and default fields
        const auto header_size = sizeof(packet_header);

        // 3. Assert: Wire size matches exactly 27 bytes and defaults match specification
        EXPECT_EQ(header_size, 27);
        EXPECT_EQ(default_header.protocol_magic, packet_header::default_protocol_magic);
        EXPECT_EQ(default_header.protocol_magic, 0x544D5053U); // "TMPS"
        EXPECT_EQ(default_header.protocol_version, packet_header::default_protocol_version);
        EXPECT_EQ(default_header.protocol_version, 1);
        EXPECT_EQ(default_header.session_id, 0ULL);
        EXPECT_EQ(default_header.sequence_id, 0U);
        EXPECT_EQ(default_header.ack_sequence_id, 0U);
        EXPECT_EQ(default_header.ack_bitfield, 0U);
        EXPECT_EQ(default_header.type, static_cast<uint8_t>(packet_type::connect_request));

        // Test custom populated header
        auto custom_header = packet_header{};
        custom_header.session_id = 0x0123456789ABCDEFULL;
        custom_header.sequence_id = 1001U;
        custom_header.ack_sequence_id = 1000U;
        custom_header.ack_bitfield = 0xFFFFFFFFU;
        custom_header.type = static_cast<uint8_t>(packet_type::snapshot);

        EXPECT_EQ(custom_header.session_id, 0x0123456789ABCDEFULL);
        EXPECT_EQ(custom_header.sequence_id, 1001U);
        EXPECT_EQ(custom_header.ack_sequence_id, 1000U);
        EXPECT_EQ(custom_header.ack_bitfield, 0xFFFFFFFFU);
        EXPECT_EQ(custom_header.type, static_cast<uint8_t>(packet_type::snapshot));
    }

    // =========================================================================
    // SECTION: Bitstream Primitive Reading and Writing Tests
    // =========================================================================

    /// @brief Verifies bit-level packing from 1 to 32 bits, booleans, u64, and buffer overflow detection.
    TEST(bitstream_tests, bitstream_read_write_primitive_bits)
    {
        // 1. Setup: Prepare bit_writer with initial allocation
        auto writer = bit_writer{64};

        // 2. Act: Write diverse bit widths, boolean values, u64, and raw byte spans
        writer.write_bits(1U, 1);
        writer.write_bits(5U, 3);
        writer.write_bits(120U, 7);
        writer.write_bits(5432U, 13);
        writer.write_bits(16777215U, 25);
        writer.write_bits(0xDEADBEEFU, 32);

        writer.write_bool(true);
        writer.write_bool(false);
        writer.write_bool(true);

        writer.write_u64(0xFEDCBA9876543210ULL);

        const auto payload_bytes = array<byte, 4>{byte{0xAA}, byte{0xBB}, byte{0xCC}, byte{0xDD}};
        writer.write_bytes(span<const byte>{payload_bytes.data(), payload_bytes.size()});

        // 3. Assert: Read all values back and verify exact bitstream integrity
        auto reader = bit_reader{writer.data()};
        EXPECT_FALSE(reader.has_overflowed());

        EXPECT_EQ(reader.read_bits(1), 1U);
        EXPECT_EQ(reader.read_bits(3), 5U);
        EXPECT_EQ(reader.read_bits(7), 120U);
        EXPECT_EQ(reader.read_bits(13), 5432U);
        EXPECT_EQ(reader.read_bits(25), 16777215U);
        EXPECT_EQ(reader.read_bits(32), 0xDEADBEEFU);

        EXPECT_TRUE(reader.read_bool());
        EXPECT_FALSE(reader.read_bool());
        EXPECT_TRUE(reader.read_bool());

        EXPECT_EQ(reader.read_u64(), 0xFEDCBA9876543210ULL);

        auto received_bytes = array<byte, 4>{};
        EXPECT_TRUE(reader.read_bytes(span<byte>{received_bytes.data(), received_bytes.size()}));
        for (auto byte_index = size_t{0}; byte_index < payload_bytes.size(); ++byte_index)
        {
            EXPECT_EQ(received_bytes[byte_index], payload_bytes[byte_index]);
        }

        EXPECT_FALSE(reader.has_overflowed());

        // Attempting to read past available bits triggers overflow flag
        const auto overflow_val = reader.read_bits(32);
        EXPECT_EQ(overflow_val, 0U);
        EXPECT_TRUE(reader.has_overflowed());
    }

    // =========================================================================
    // SECTION: Position and Chunk-Relative Quantization Tests
    // =========================================================================

    /// @brief Verifies fixed-point position quantization in [-100m, +100m] with < 1mm error.
    TEST(bitstream_tests, bitstream_position_quantization_1mm_precision)
    {
        // 1. Setup: Test coordinate positions across [-100m, +100m]
        const auto test_positions = array<float, 7>{-100.0f, -50.1234f, -0.001f, 0.0f, 12.3456f, 99.999f, 100.0f};

        auto writer = bit_writer{64};

        // 2. Act: Quantize using 20 bits (resolution 200m / 1,048,575 = ~0.19mm per step)
        for (const auto original_pos : test_positions)
        {
            writer.write_quantized_float(original_pos, -100.0f, 100.0f, 20);
        }

        // 3. Assert: Dequantized values exhibit position error strictly less than 1mm (0.001m)
        auto reader = bit_reader{writer.data()};
        for (const auto original_pos : test_positions)
        {
            const auto reconstructed_pos = reader.read_quantized_float(-100.0f, 100.0f, 20);
            const auto error_delta = math::abs(reconstructed_pos - original_pos);
            EXPECT_LT(error_delta, 0.001f);
        }
        EXPECT_FALSE(reader.has_overflowed());
    }

    /// @brief Verifies chunk-relative position quantization in a 64m chunk maintains < 1mm error.
    TEST(bitstream_tests, bitstream_chunk_relative_position)
    {
        // 1. Setup: Define chunk origin in world space and chunk extent of 64m
        const auto chunk_origin = math::float3{1024.0f, 2048.0f, -512.0f};
        constexpr auto chunk_extent_m = 64.0f;

        const auto world_positions =
            array<math::float3, 3>{chunk_origin, math::float3{1024.0f + 10.555f, 2048.0f + 32.123f, -512.0f + 63.999f},
                                   math::float3{1024.0f + 64.0f, 2048.0f + 64.0f, -512.0f + 64.0f}};

        auto writer = bit_writer{64};

        // 2. Act: Write chunk-relative positions quantized to 16 bits per axis
        for (const auto& original_world_pos : world_positions)
        {
            writer.write_chunk_relative_position(original_world_pos, chunk_origin, chunk_extent_m, 16);
        }

        // 3. Assert: 3D Euclidean error between original and reconstructed positions is < 1mm (0.001m)
        auto reader = bit_reader{writer.data()};
        for (const auto& original_world_pos : world_positions)
        {
            const auto reconstructed_world_pos = reader.read_chunk_relative_position(chunk_origin, chunk_extent_m, 16);
            const auto delta_vector = reconstructed_world_pos - original_world_pos;
            const auto distance_error = math::norm(delta_vector);
            EXPECT_LT(distance_error, 0.001f);
        }
        EXPECT_FALSE(reader.has_overflowed());
    }

    // =========================================================================
    // SECTION: Float, Angle, Vector, and Quaternion Quantization Tests
    // =========================================================================

    /// @brief Verifies IEEE 754 16-bit half-precision floating-point pack and unpack round-trip.
    TEST(bitstream_tests, bitstream_half_precision_float)
    {
        // 1. Setup: Test float values spanning zero, signs, normal ranges, subnormals, and infinities
        constexpr auto min_subnormal = 5.9604644775390625e-8f; // 2^-24
        const auto test_floats = array<float, 12>{
            0.0f, -0.0f, 1.0f, -1.0f, 0.5f, -2.5f, 65504.0f, -65504.0f,
            min_subnormal, -min_subnormal,
            numeric_limits<float>::infinity(), -numeric_limits<float>::infinity()
        };

        auto writer = bit_writer{48};

        // 2. Act: Write floats as 16-bit half precision
        for (const auto original_value : test_floats)
        {
            writer.write_half(original_value);
        }

        // Also test NaN preservation through bitstream
        const auto test_nan = bit_cast<float>(0x7FC00000U);
        writer.write_half(test_nan);

        // 3. Assert: Exact representable half-floats decode with zero error
        auto reader = bit_reader{writer.data()};
        for (const auto original_value : test_floats)
        {
            const auto reconstructed_value = reader.read_half();
            if (original_value == 0.0f)
            {
                EXPECT_FLOAT_EQ(reconstructed_value, 0.0f);
            }
            else if (isinf(original_value))
            {
                EXPECT_TRUE(isinf(reconstructed_value));
                EXPECT_EQ(signbit(original_value), signbit(reconstructed_value));
            }
            else
            {
                const auto relative_error = math::abs(reconstructed_value - original_value) / math::abs(original_value);
                EXPECT_LT(relative_error, 0.001f);
            }
        }

        const auto reconstructed_nan = reader.read_half();
        EXPECT_TRUE(isnan(reconstructed_nan));
        EXPECT_FALSE(reader.has_overflowed());
    }

    /// @brief Verifies angle quantization for 8-bit and 16-bit precision across primary radians.
    TEST(bitstream_tests, bitstream_angle_quantization)
    {
        // 1. Setup: Test cardinal angles 0, pi/4, pi/2, pi, 3*pi/2
        constexpr auto pi = math::constants::pi<float>;
        const auto test_angles = array<float, 5>{0.0f, pi * 0.25f, pi * 0.5f, pi, pi * 1.5f};

        // 2. Act: Test both 8-bit and 16-bit quantizations
        auto writer_8bit = bit_writer{16};
        for (const auto angle_rad : test_angles)
        {
            writer_8bit.write_angle(angle_rad, 8);
        }

        auto writer_16bit = bit_writer{16};
        for (const auto angle_rad : test_angles)
        {
            writer_16bit.write_angle(angle_rad, 16);
        }

        // 3. Assert: 8-bit angles round-trip within < 0.03 rad, 16-bit within < 0.001 rad
        auto reader_8bit = bit_reader{writer_8bit.data()};
        for (const auto angle_rad : test_angles)
        {
            const auto reconstructed_angle = reader_8bit.read_angle(8);
            const auto error = math::abs(reconstructed_angle - angle_rad);
            EXPECT_LT(error, 0.03f);
        }

        auto reader_16bit = bit_reader{writer_16bit.data()};
        for (const auto angle_rad : test_angles)
        {
            const auto reconstructed_angle = reader_16bit.read_angle(16);
            const auto error = math::abs(reconstructed_angle - angle_rad);
            EXPECT_LT(error, 0.001f);
        }
    }

    /// @brief Verifies octahedral unit vector compression maintains angular error < 0.5 degrees.
    TEST(bitstream_tests, bitstream_octahedral_unit_vector)
    {
        // 1. Setup: Test vectors on coordinate axes and arbitrary directions
        const auto inv_sqrt3 = 1.0f / math::sqrt(3.0f);
        const auto test_vectors = array<math::float3, 6>{math::float3{1.0f, 0.0f, 0.0f},
                                                         math::float3{0.0f, 1.0f, 0.0f},
                                                         math::float3{0.0f, 0.0f, 1.0f},
                                                         math::float3{0.0f, 0.0f, -1.0f},
                                                         math::float3{inv_sqrt3, inv_sqrt3, inv_sqrt3},
                                                         math::normalize(math::float3{-0.5f, 0.5f, -0.7071f})};

        auto writer = bit_writer{32};

        // 2. Act: Quantize octahedral vectors using 12 bits per axis
        for (const auto& original_vector : test_vectors)
        {
            writer.write_octahedral_unit_vector(original_vector, 12);
        }

        // 3. Assert: Angular error between original and reconstructed vectors is strictly < 0.5 degrees
        auto reader = bit_reader{writer.data()};
        for (const auto& original_vector : test_vectors)
        {
            const auto reconstructed_vector = reader.read_octahedral_unit_vector(12);
            const auto dot_product = clamp(math::dot(original_vector, reconstructed_vector), -1.0f, 1.0f);
            const auto angular_error_rad = math::acos(dot_product);
            const auto angular_error_deg = angular_error_rad * (180.0f / math::constants::pi<float>);
            EXPECT_LT(angular_error_deg, 0.5f);
        }
        EXPECT_FALSE(reader.has_overflowed());
    }

    /// @brief Verifies smallest-three unit quaternion compression across different dominant axes.
    TEST(bitstream_tests, bitstream_smallest_three_quaternion)
    {
        // 1. Setup: Prepare quaternions with different dominant axes (w, x, y, z)
        constexpr auto pi = math::constants::pi<float>;
        const auto test_quaternions = array<math::fquat, 5>{
            math::fquat{0.0f, 0.0f, 0.0f, 1.0f},                                   // Dominant w (identity)
            math::fquat{math::sin(pi * 0.25f), 0.0f, 0.0f, math::cos(pi * 0.25f)}, // Dominant x
            math::fquat{0.0f, math::sin(pi * 0.25f), 0.0f, math::cos(pi * 0.25f)}, // Dominant y
            math::fquat{0.0f, 0.0f, math::sin(pi * 0.25f), math::cos(pi * 0.25f)}, // Dominant z
            math::fquat{math::vec3<float>{0.3f, -0.5f, 1.2f}}                      // Arbitrary Euler rotation
        };

        auto writer = bit_writer{32};

        // 2. Act: Compress quaternions using smallest-three representation
        for (const auto& original_quat : test_quaternions)
        {
            writer.write_smallest_three_quaternion(original_quat);
        }

        // 3. Assert: Reconstructed quaternion aligns with original (|dot(q1, q2)| > 0.999)
        auto reader = bit_reader{writer.data()};
        for (const auto& original_quat : test_quaternions)
        {
            const auto reconstructed_quat = reader.read_smallest_three_quaternion();
            const auto dot_product = math::abs(math::dot(original_quat, reconstructed_quat));
            EXPECT_GT(dot_product, 0.999f);
        }
        EXPECT_FALSE(reader.has_overflowed());
    }

    /// @brief Verifies 32-bit quantized float encoding and decoding near boundary conditions (min, zero, mid, max)
    /// does not suffer from float-to-integer overflow wrapping or precision corruption.
    TEST(bitstream_tests, bitstream_quantized_float_32bit_boundary)
    {
        // 1. Setup: Test boundary values across [-100.0f, +100.0f] range
        constexpr auto min_range = -100.0f;
        constexpr auto max_range = 100.0f;
        const auto test_values = array<float, 4>{-100.0f, 0.0f, 50.0f, 100.0f};

        auto writer = bit_writer{16};

        // 2. Act: Quantize using maximum 32-bit integer resolution
        for (const auto original_value : test_values)
        {
            writer.write_quantized_float(original_value, min_range, max_range, 32);
        }

        // 3. Assert: Read back each value and verify accurate reconstruction within float epsilon
        auto reader = bit_reader{writer.data()};
        for (const auto original_value : test_values)
        {
            const auto reconstructed_value = reader.read_quantized_float(min_range, max_range, 32);
            EXPECT_NEAR(reconstructed_value, original_value, 1e-4f);
        }
        EXPECT_FALSE(reader.has_overflowed());
    }

    // =========================================================================
    // SECTION: Bulk Byte Transfer Tests (Aligned Fast-Path & Unaligned Fallback)
    // =========================================================================

    /// @brief Verifies that write_bytes and read_bytes execute byte-aligned bulk memory copies
    /// with zero corruption, correctly handle unaligned bit-offset transfers, and detect buffer overflow.
    TEST(bitstream_tests, bitstream_bulk_bytes_byte_aligned_and_unaligned)
    {
        // 1. Setup: Prepare 256-byte payload with deterministic pseudo-random bytes
        auto test_payload = array<byte, 256>{};
        for (auto index = 0U; index < test_payload.size(); ++index)
        {
            test_payload[index] = static_cast<byte>((index * 13U + 7U) & 0xFFU);
        }

        // 2. Act & Assert: Byte-aligned bulk transfer (fast path)
        auto writer_aligned = bit_writer{256};
        writer_aligned.write_bytes(span<const byte>{test_payload.data(), test_payload.size()});

        EXPECT_EQ(writer_aligned.bit_count(), 256U * 8U);
        EXPECT_EQ(writer_aligned.byte_count(), 256U);

        auto reader_aligned = bit_reader{writer_aligned.data()};
        auto read_aligned_dest = array<byte, 256>{};
        const auto aligned_read_success = reader_aligned.read_bytes(
            span<byte>{read_aligned_dest.data(), read_aligned_dest.size()});

        EXPECT_TRUE(aligned_read_success);
        EXPECT_FALSE(reader_aligned.has_overflowed());
        EXPECT_EQ(reader_aligned.remaining_bits(), 0U);

        for (auto index = 0U; index < test_payload.size(); ++index)
        {
            EXPECT_EQ(read_aligned_dest[index], test_payload[index]);
        }

        // 3. Act & Assert: Unaligned bulk transfer preceded by 3 bits (slow path)
        auto writer_unaligned = bit_writer{300};
        constexpr auto prefix_bits = 5U;  // 3-bit pattern 101b = 5
        constexpr auto suffix_bits = 19U; // 5-bit pattern 10011b = 19
        writer_unaligned.write_bits(prefix_bits, 3);
        writer_unaligned.write_bytes(span<const byte>{test_payload.data(), test_payload.size()});
        writer_unaligned.write_bits(suffix_bits, 5);

        EXPECT_EQ(writer_unaligned.bit_count(), 3U + (256U * 8U) + 5U);

        auto reader_unaligned = bit_reader{writer_unaligned.data()};
        const auto read_prefix = reader_unaligned.read_bits(3);
        EXPECT_EQ(read_prefix, prefix_bits);

        auto read_unaligned_dest = array<byte, 256>{};
        const auto unaligned_read_success = reader_unaligned.read_bytes(
            span<byte>{read_unaligned_dest.data(), read_unaligned_dest.size()});

        EXPECT_TRUE(unaligned_read_success);
        EXPECT_FALSE(reader_unaligned.has_overflowed());

        const auto read_suffix = reader_unaligned.read_bits(5);
        EXPECT_EQ(read_suffix, suffix_bits);
        EXPECT_EQ(reader_unaligned.remaining_bits(), 0U);

        for (auto index = 0U; index < test_payload.size(); ++index)
        {
            EXPECT_EQ(read_unaligned_dest[index], test_payload[index]);
        }

        // 4. Assert: Reading beyond available bits sets overflow flag and returns false
        auto read_overflow_dest = array<byte, 1>{};
        const auto overflow_read_success = reader_unaligned.read_bytes(
            span<byte>{read_overflow_dest.data(), read_overflow_dest.size()});
        EXPECT_FALSE(overflow_read_success);
        EXPECT_TRUE(reader_unaligned.has_overflowed());
    }
} // namespace tempest::network::tests
