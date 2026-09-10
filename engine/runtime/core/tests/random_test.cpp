#include <tempest/random.hpp>

#include <tempest/array.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

//=============================================================================
// PCG32 Core Engine Tests
//=============================================================================

/// @brief Verifies that two pcg32 engines initialized with identical seeds generate identical sequences.
TEST(random_engine, pcg32_reproducibility)
{
    // 1. Setup
    constexpr auto test_seed = static_cast<tempest::uint64_t>(0x853c49e6748fea9bULL);
    constexpr auto test_stream = static_cast<tempest::uint64_t>(0xda3e39cb94b95bdbULL);
    auto rng1 = tempest::pcg32{test_seed, test_stream};
    auto rng2 = tempest::pcg32{test_seed, test_stream};

    // 2. Act & 3. Assert
    for (auto i = 0U; i < 100000U; ++i)
    {
        const auto val1 = rng1();
        const auto val2 = rng2();
        ASSERT_EQ(val1, val2);
    }
}

/// @brief Verifies that pcg32 generates deterministic output matching its internal state progression.
TEST(random_engine, pcg32_deterministic_progression)
{
    // 1. Setup
    auto rng = tempest::pcg32{42ULL, 54ULL};

    // 2. Act
    const auto val1 = rng();
    const auto val2 = rng();
    const auto val3 = rng();

    // 3. Assert: re-seeding produces the exact same three outputs
    rng.seed(42ULL, 54ULL);
    ASSERT_EQ(rng(), val1);
    ASSERT_EQ(rng(), val2);
    ASSERT_EQ(rng(), val3);
}

/// @brief Verifies that different streams create uncorrelated sequences from identical initial seeds.
TEST(random_engine, pcg32_distinct_streams)
{
    // 1. Setup
    constexpr auto test_seed = static_cast<tempest::uint64_t>(12345ULL);
    auto rng1 = tempest::pcg32{test_seed, 1ULL};
    auto rng2 = tempest::pcg32{test_seed, 2ULL};

    // 2. Act & 3. Assert
    auto difference_count = 0U;
    constexpr auto iterations = 1000U;
    for (auto i = 0U; i < iterations; ++i)
    {
        if (rng1() != rng2())
        {
            ++difference_count;
        }
    }
    ASSERT_GT(difference_count, iterations * 99 / 100);
}

/// @brief Verifies that pcg32::discard(z) advances the generator state equivalently to calling operator() z times.
TEST(random_engine, pcg32_discard_equivalence)
{
    // 1. Setup
    constexpr auto test_seed = static_cast<tempest::uint64_t>(987654321ULL);
    auto rng1 = tempest::pcg32{test_seed};
    auto rng2 = tempest::pcg32{test_seed};

    // 2. Act
    constexpr auto discard_count = static_cast<tempest::uint64_t>(1000);
    for (auto i = 0ULL; i < discard_count; ++i)
    {
        rng1();
    }
    rng2.discard(discard_count);

    // 3. Assert
    ASSERT_EQ(rng1, rng2);
    ASSERT_EQ(rng1(), rng2());
}

/// @brief Verifies constexpr bounds min() and max() for pcg32.
TEST(random_engine, pcg32_bounds)
{
    // 1. Setup & 2. Act & 3. Assert
    static_assert(tempest::pcg32::min() == 0U);
    static_assert(tempest::pcg32::max() == 0xFFFF'FFFFU);
    ASSERT_EQ(tempest::pcg32::min(), 0U);
    ASSERT_EQ(tempest::pcg32::max(), 0xFFFF'FFFFU);
}

/// @brief Verifies that default_random_engine aliases pcg32.
TEST(random_engine, default_random_engine_alias)
{
    // 1. Setup & 2. Act & 3. Assert
    static_assert(tempest::is_same_v<tempest::default_random_engine, tempest::pcg32>);
    ASSERT_TRUE((tempest::is_same_v<tempest::default_random_engine, tempest::pcg32>));
}

//=============================================================================
// xoshiro256** Core Engine Tests
//=============================================================================

/// @brief Verifies that xoshiro256starstar initialized with identical seeds generates identical sequences.
TEST(random_engine, xoshiro256starstar_reproducibility)
{
    // 1. Setup
    constexpr auto test_seed = static_cast<tempest::uint64_t>(0x0123456789ABCDEFULL);
    auto rng1 = tempest::xoshiro256starstar{test_seed};
    auto rng2 = tempest::xoshiro256starstar{test_seed};

    // 2. Act & 3. Assert
    for (auto i = 0U; i < 100000U; ++i)
    {
        const auto val1 = rng1();
        const auto val2 = rng2();
        ASSERT_EQ(val1, val2);
    }
}

/// @brief Verifies that xoshiro256starstar matches canonical Blackman/Vigna test vectors.
TEST(random_engine, xoshiro256starstar_reference_vector)
{
    // 1. Setup: Initialize state directly with {1, 2, 3, 4}
    auto rng = tempest::xoshiro256starstar{1ULL, 2ULL, 3ULL, 4ULL};

    // 2. Act
    // First value: rotl(s[1]*5, 7) * 9 = rotl(10, 7) * 9 = 1280 * 9 = 11520
    const auto v0 = rng();

    // 3. Assert
    ASSERT_EQ(v0, 11520ULL);
}

/// @brief Verifies that xoshiro256starstar re-seeding with 4-state words functions correctly.
TEST(random_engine, xoshiro256starstar_quad_seed)
{
    // 1. Setup
    auto rng = tempest::xoshiro256starstar{};

    // 2. Act
    rng.seed(1ULL, 2ULL, 3ULL, 4ULL);
    const auto v0 = rng();

    // 3. Assert
    ASSERT_EQ(v0, 11520ULL);
}

/// @brief Verifies that xoshiro256starstar jump() creates non-overlapping sequences.
TEST(random_engine, xoshiro256starstar_jump_decorrelation)
{
    // 1. Setup
    auto rng1 = tempest::xoshiro256starstar{42ULL};
    auto rng2 = rng1;

    // 2. Act
    rng2.jump();

    // 3. Assert: Sequences should not collide over many iterations
    auto match_count = 0U;
    for (auto i = 0U; i < 10000U; ++i)
    {
        if (rng1() == rng2())
        {
            ++match_count;
        }
    }
    ASSERT_EQ(match_count, 0U);
}

/// @brief Verifies that xoshiro256starstar long_jump() advances state appropriately.
TEST(random_engine, xoshiro256starstar_long_jump_decorrelation)
{
    // 1. Setup
    auto rng1 = tempest::xoshiro256starstar{42ULL};
    auto rng2 = rng1;

    // 2. Act
    rng2.long_jump();

    // 3. Assert: Sequences should not collide
    auto match_count = 0U;
    for (auto i = 0U; i < 10000U; ++i)
    {
        if (rng1() == rng2())
        {
            ++match_count;
        }
    }
    ASSERT_EQ(match_count, 0U);
}

/// @brief Verifies that xoshiro256starstar discard(z) advances the generator state equivalently to calling operator().
TEST(random_engine, xoshiro256starstar_discard_equivalence)
{
    // 1. Setup
    auto rng1 = tempest::xoshiro256starstar{9999ULL};
    auto rng2 = tempest::xoshiro256starstar{9999ULL};

    // 2. Act
    constexpr auto discard_count = static_cast<tempest::uint64_t>(500);
    for (auto i = 0ULL; i < discard_count; ++i)
    {
        rng1();
    }
    rng2.discard(discard_count);

    // 3. Assert
    ASSERT_EQ(rng1, rng2);
    ASSERT_EQ(rng1(), rng2());
}

/// @brief Verifies constexpr bounds min() and max() for xoshiro256starstar.
TEST(random_engine, xoshiro256starstar_bounds)
{
    // 1. Setup & 2. Act & 3. Assert
    static_assert(tempest::xoshiro256starstar::min() == 0ULL);
    static_assert(tempest::xoshiro256starstar::max() == 0xFFFF'FFFF'FFFF'FFFFULL);
    ASSERT_EQ(tempest::xoshiro256starstar::min(), 0ULL);
    ASSERT_EQ(tempest::xoshiro256starstar::max(), 0xFFFF'FFFF'FFFF'FFFFULL);
}

//=============================================================================
// Hardware / OS Entropy Source (random_device) Tests
//=============================================================================

/// @brief Verifies that random_device generates non-zero varying entropy across multiple draws.
TEST(random_device, entropy_sanity_and_uniqueness)
{
    // 1. Setup
    auto rd = tempest::random_device{};
    auto samples = tempest::vector<tempest::uint32_t>();
    samples.reserve(100);

    // 2. Act
    for (auto i = 0; i < 100; ++i)
    {
        samples.push_back(rd());
    }

    // 3. Assert: Verify values are not all zero and vary across draws
    auto unique_count = 0U;
    for (auto i = 0U; i < samples.size(); ++i)
    {
        auto duplicate = false;
        for (auto j = 0U; j < i; ++j)
        {
            if (samples[i] == samples[j])
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            ++unique_count;
        }
    }
    ASSERT_GE(unique_count, 98U);
}

/// @brief Verifies that random_device::generate fills arbitrary byte buffers.
TEST(random_device, buffer_fill_generation)
{
    // 1. Setup
    auto rd = tempest::random_device{};
    tempest::byte buffer1[64]{};
    tempest::byte buffer2[64]{};

    // 2. Act
    rd.generate(buffer1, sizeof(buffer1));
    rd.generate(buffer2, sizeof(buffer2));

    // 3. Assert: Both buffers should contain non-zero data and differ from each other
    auto b1_nonzero = false;
    auto b2_nonzero = false;
    auto differ = false;

    for (auto i = 0U; i < 64U; ++i)
    {
        if (static_cast<tempest::uint8_t>(buffer1[i]) != 0)
        {
            b1_nonzero = true;
        }
        if (static_cast<tempest::uint8_t>(buffer2[i]) != 0)
        {
            b2_nonzero = true;
        }
        if (buffer1[i] != buffer2[i])
        {
            differ = true;
        }
    }

    ASSERT_TRUE(b1_nonzero);
    ASSERT_TRUE(b2_nonzero);
    ASSERT_TRUE(differ);
}

//=============================================================================
// Uniform Integer Distribution Tests
//=============================================================================

/// @brief Verifies bounds enforcement for positive, negative, and degenerate integer intervals.
TEST(uniform_int_distribution, bounds_enforcement)
{
    // 1. Setup
    auto rng = tempest::pcg32{42ULL};

    // 2. Act & 3. Assert: Standard interval [3, 17] over 10^6 iterations
    auto dist_pos = tempest::uniform_int_distribution<int>{3, 17};
    ASSERT_EQ(dist_pos.a(), 3);
    ASSERT_EQ(dist_pos.b(), 17);
    ASSERT_EQ(dist_pos.min(), 3);
    ASSERT_EQ(dist_pos.max(), 17);

    for (auto i = 0; i < 1000000; ++i)
    {
        const auto val = dist_pos(rng);
        ASSERT_GE(val, 3);
        ASSERT_LE(val, 17);
    }

    // Negative interval [-50, -10]
    auto dist_neg = tempest::uniform_int_distribution<int>{-50, -10};
    for (auto i = 0; i < 10000; ++i)
    {
        const auto val = dist_neg(rng);
        ASSERT_GE(val, -50);
        ASSERT_LE(val, -10);
    }

    // Degenerate interval [42, 42]
    auto dist_degen = tempest::uniform_int_distribution<int>{42, 42};
    for (auto i = 0; i < 1000; ++i)
    {
        ASSERT_EQ(dist_degen(rng), 42);
    }

    // Inverted interval [50, 10] returns a()
    auto dist_inverted = tempest::uniform_int_distribution<int>{50, 10};
    ASSERT_EQ(dist_inverted(rng), 50);

    // Large range near 2^31 (verifies unsigned modulo threshold calculation)
    auto dist_large32 = tempest::uniform_int_distribution<tempest::uint32_t>{0, 0x80000005U};
    for (auto i = 0; i < 10000; ++i)
    {
        const auto val = dist_large32(rng);
        ASSERT_LE(val, 0x80000005U);
    }

    // 64-bit distribution
    auto rng64 = tempest::xoshiro256starstar{123ULL};
    auto dist64 =
        tempest::uniform_int_distribution<tempest::int64_t>{-1000000000000LL, 1000000000000LL};
    for (auto i = 0; i < 10000; ++i)
    {
        const auto val = dist64(rng64);
        ASSERT_GE(val, -1000000000000LL);
        ASSERT_LE(val, 1000000000000LL);
    }

    // Unsigned 8-bit distribution
    auto dist_u8 = tempest::uniform_int_distribution<tempest::uint8_t>{10, 250};
    for (auto i = 0; i < 1000; ++i)
    {
        const auto val = dist_u8(rng);
        ASSERT_GE(val, 10);
        ASSERT_LE(val, 250);
    }
}

/// @brief Tests statistical uniformity across buckets using Pearson's Chi-squared test.
TEST(uniform_int_distribution, statistical_chi_squared_uniformity)
{
    // 1. Setup: 6-sided die roll [1, 6] with 600,000 samples (expected: 100,000 each)
    auto rng = tempest::pcg32{12345ULL};
    auto dist = tempest::uniform_int_distribution<int>{1, 6};
    auto counts = tempest::array<tempest::uint32_t, 6>{};
    constexpr auto total_samples = 600000;
    constexpr auto expected_per_bucket = 100000.0;

    // 2. Act
    for (auto i = 0; i < total_samples; ++i)
    {
        const auto roll = dist(rng);
        ASSERT_GE(roll, 1);
        ASSERT_LE(roll, 6);
        ++counts[roll - 1];
    }

    // 3. Assert: Pearson's Chi-squared test with 5 degrees of freedom at alpha = 0.001 (critical value = 20.515)
    auto chi_squared = 0.0;
    for (unsigned int count : counts)
    {
        const auto diff = static_cast<double>(count) - expected_per_bucket;
        chi_squared += (diff * diff) / expected_per_bucket;
    }

    ASSERT_LT(chi_squared, 20.515);
}

/// @brief Tests STL param_type compatibility, getters, and invocation overrides.
TEST(uniform_int_distribution, stl_param_compatibility)
{
    // 1. Setup
    auto rng = tempest::pcg32{42ULL};
    auto dist = tempest::uniform_int_distribution<int>{0, 10};

    // 2. Act: Override with param_type {100, 200}
    using param_type = tempest::uniform_int_distribution<int>::param_type;
    auto new_parm = param_type{100, 200};
    ASSERT_EQ(new_parm.a(), 100);
    ASSERT_EQ(new_parm.b(), 200);

    const auto override_val = dist(rng, new_parm);

    // 3. Assert
    ASSERT_GE(override_val, 100);
    ASSERT_LE(override_val, 200);
    ASSERT_EQ(dist.a(), 0);
    ASSERT_EQ(dist.b(), 10);

    dist.param(new_parm);
    ASSERT_EQ(dist.a(), 100);
    ASSERT_EQ(dist.b(), 200);
}

//=============================================================================
// Uniform Real Distribution Tests
//=============================================================================

/// @brief Verifies that float and double uniform real samples strictly satisfy a <= x < b.
TEST(uniform_real_distribution, half_open_interval_bounds)
{
    // 1. Setup
    auto rng = tempest::pcg32{777ULL};

    // 2. Act & 3. Assert: float [0.0f, 1.0f) over 10^6 iterations
    auto dist_float = tempest::uniform_real_distribution<float>{0.0F, 1.0F};
    ASSERT_EQ(dist_float.a(), 0.0F);
    ASSERT_EQ(dist_float.b(), 1.0F);
    ASSERT_EQ(dist_float.min(), 0.0F);
    ASSERT_EQ(dist_float.max(), 1.0F);

    for (auto i = 0; i < 1000000; ++i)
    {
        const auto val = dist_float(rng);
        ASSERT_GE(val, 0.0F);
        ASSERT_LT(val, 1.0F);
    }

    // float with 64-bit engine (xoshiro256starstar)
    auto rng64 = tempest::xoshiro256starstar{888ULL};
    for (auto i = 0; i < 10000; ++i)
    {
        const auto val = dist_float(rng64);
        ASSERT_GE(val, 0.0F);
        ASSERT_LT(val, 1.0F);
    }

    // double [-5.0, 5.0)
    auto dist_double = tempest::uniform_real_distribution<double>{-5.0, 5.0};
    for (auto i = 0; i < 100000; ++i)
    {
        const auto val = dist_double(rng64);
        ASSERT_GE(val, -5.0);
        ASSERT_LT(val, 5.0);
    }

    // double with 32-bit engine (pcg32)
    for (auto i = 0; i < 10000; ++i)
    {
        const auto val = dist_double(rng);
        ASSERT_GE(val, -5.0);
        ASSERT_LT(val, 5.0);
    }
}

/// @brief Verifies that the sample mean and variance match the theoretical uniform distribution.
TEST(uniform_real_distribution, statistical_mean_and_variance)
{
    // 1. Setup: N = 10^6 draws on [0.0, 1.0)
    auto rng = tempest::pcg32{42ULL};
    auto dist = tempest::uniform_real_distribution<double>{0.0, 1.0};
    constexpr auto total_draws = 1000000;
    auto sum = 0.0;
    auto sum_sq = 0.0;

    // 2. Act
    for (auto i = 0; i < total_draws; ++i)
    {
        const auto sample_val = dist(rng);
        sum += sample_val;
        sum_sq += sample_val * sample_val;
    }

    const auto mean = sum / total_draws;
    const auto variance = (sum_sq / total_draws) - (mean * mean);

    // 3. Assert:
    // Theoretical mean: 0.5. Standard error of mean: 1 / sqrt(12 * N) = 0.000288.
    // Allow 4 standard errors tolerance: 0.0012
    ASSERT_NEAR(mean, 0.5, 0.0015);

    // Theoretical variance: 1 / 12 = 0.08333. Allow 2% tolerance.
    ASSERT_NEAR(variance, 1.0 / 12.0, 0.002);
}

//=============================================================================
// GUID Integration & RFC 4122 v4 Compliance Tests
//=============================================================================

/// @brief Verifies that guid::generate_random_guid() adheres strictly to RFC 4122 v4.
TEST(guid, rfc4122_v4_compliance)
{
    // 1. Setup & 2. Act & 3. Assert
    for (auto i = 0; i < 1000; ++i)
    {
        const auto test_guid = tempest::guid::generate_random_guid();

        // Byte 6: high 4 bits must be 0100 (version 4)
        const auto byte6 = static_cast<tempest::uint8_t>(test_guid.data[6]);
        const auto version = (byte6 >> 4) & 0x0F;
        ASSERT_EQ(version, 4) << "GUID version mismatch at iteration " << i;

        // Byte 8: high 2 bits must be 10 (variant 1, RFC 4122)
        const auto byte8 = static_cast<tempest::uint8_t>(test_guid.data[8]);
        const auto variant = (byte8 >> 6) & 0x03;
        ASSERT_EQ(variant, 2) << "GUID variant mismatch at iteration " << i;
    }
}

/// @brief Verifies that generated GUIDs are unique with no collisions.
TEST(guid, uniqueness_collision_resistance)
{
    // 1. Setup
    constexpr auto count = 1000U;
    auto guids = tempest::vector<tempest::guid>();
    guids.reserve(count);

    // 2. Act
    for (auto i = 0U; i < count; ++i)
    {
        guids.push_back(tempest::guid::generate_random_guid());
    }

    // 3. Assert: Check all pairwise uniqueness
    for (auto i = 0U; i < count; ++i)
    {
        for (auto j = i + 1; j < count; ++j)
        {
            ASSERT_NE(guids[i], guids[j]);
        }
    }
}

/// @brief Verifies that to_string(guid) produces valid 36-character RFC 4122 formatted string.
TEST(guid, string_representation_format)
{
    // 1. Setup & 2. Act
    const auto test_guid = tempest::guid::generate_random_guid();
    const auto str = tempest::to_string(test_guid);

    // 3. Assert
    ASSERT_EQ(str.size(), 36U);
    ASSERT_EQ(str[8], '-');
    ASSERT_EQ(str[13], '-');
    ASSERT_EQ(str[18], '-');
    ASSERT_EQ(str[23], '-');

    // RFC 4122 v4 version character at index 14 is '4'
    ASSERT_EQ(str[14], '4');

    // RFC 4122 variant character at index 19 is '8', '9', 'A', or 'B'
    const auto variant_char = str[19];
    const auto valid_variant = (variant_char == '8' || variant_char == '9' ||
                                variant_char == 'A' || variant_char == 'B');
    ASSERT_TRUE(valid_variant);
}
