#include <tempest/bitset.hpp>

#include <gtest/gtest.h>

namespace tempest::tests
{
    // =========================================================================
    // SECTION: Single-Word Bitset Operations (< 64 and == 64 bits)
    // =========================================================================

    /// @brief Verifies default construction, initial bit states, and capacity observers
    ///        on small and single-word bitsets.
    TEST(bitset_test, default_construction_and_capacity)
    {
        // Setup & Act: Construct default bitsets of various sizes
        const auto small_set = bitset<10>{};
        const auto exact_word_set = bitset<64>{};

        // Assert: Size matches template argument, count is 0, and none() is true
        EXPECT_EQ(small_set.size(), 10u);
        EXPECT_EQ(small_set.count(), 0u);
        EXPECT_TRUE(small_set.none());
        EXPECT_FALSE(small_set.any());
        EXPECT_FALSE(small_set.all());

        EXPECT_EQ(exact_word_set.size(), 64u);
        EXPECT_EQ(exact_word_set.count(), 0u);
        EXPECT_TRUE(exact_word_set.none());
    }

    /// @brief Verifies setting, testing, resetting, and flipping individual bits within a single word.
    TEST(bitset_test, single_word_mutation_and_testing)
    {
        // Setup: Create empty 64-bit set
        auto bits = bitset<64>{};

        // Act: Set bits 0, 15, and 63
        bits.set(0);
        bits.set(15);
        bits.set(63);

        // Assert: Specified bits are set and count reflects accurate popcount
        EXPECT_TRUE(bits.test(0));
        EXPECT_TRUE(bits.test(15));
        EXPECT_TRUE(bits.test(63));
        EXPECT_FALSE(bits.test(1));
        EXPECT_FALSE(bits.test(62));
        EXPECT_EQ(bits.count(), 3u);
        EXPECT_TRUE(bits.any());

        // Act: Reset bit 15 and flip bit 62
        bits.reset(15);
        bits.flip(62);

        // Assert: Invariant holds after reset and flip
        EXPECT_FALSE(bits.test(15));
        EXPECT_TRUE(bits.test(62));
        EXPECT_EQ(bits.count(), 3u);
    }

    /// @brief Verifies that setting all bits on a non-multiple-of-64 bitset strictly masks out
    ///        bits beyond the specified capacity N.
    TEST(bitset_test, partial_word_tail_masking)
    {
        // Setup: Bitset with 10 bits
        auto bits = bitset<10>{};

        // Act: Set all bits
        bits.set();

        // Assert: Exactly 10 bits are set, and all() evaluates to true
        EXPECT_EQ(bits.count(), 10u);
        EXPECT_TRUE(bits.all());
        for (auto index = 0u; index < 10u; ++index)
        {
            EXPECT_TRUE(bits.test(index));
        }

        // Out-of-bounds queries return false
        EXPECT_FALSE(bits.test(10));
        EXPECT_FALSE(bits.test(63));

        // Act: Invert bits
        bits.flip();

        // Assert: After flipping all 10 bits, set is completely empty
        EXPECT_EQ(bits.count(), 0u);
        EXPECT_TRUE(bits.none());
    }

    // =========================================================================
    // SECTION: Multi-Word Bitset Operations (128 and 1024 bits)
    // =========================================================================

    /// @brief Verifies bit manipulation, count, and queries across 64-bit word boundaries.
    TEST(bitset_test, multi_word_cross_boundary_operations)
    {
        // Setup: 128-bit bitset spanning exactly two 64-bit words
        auto bits = bitset<128>{};
        EXPECT_EQ(bits.num_words(), 2u);

        // Act: Set boundary bits (last bit of word 0 and first bit of word 1)
        bits.set(63);
        bits.set(64);
        bits.set(127);

        // Assert: Boundary bits are preserved without bleeding or truncation
        EXPECT_TRUE(bits.test(63));
        EXPECT_TRUE(bits.test(64));
        EXPECT_TRUE(bits.test(127));
        EXPECT_FALSE(bits.test(0));
        EXPECT_FALSE(bits.test(65));
        EXPECT_EQ(bits.count(), 3u);

        EXPECT_EQ(bits.word(0), 1ULL << 63);
        EXPECT_EQ(bits.word(1), (1ULL << 0) | (1ULL << 63));
    }

    /// @brief Verifies that 1024-bit bitset (matching cpu_mask capacity) accurately handles
    ///        high indices up to 1023 and computes total popcount across all 16 words.
    TEST(bitset_test, cpu_mask_1024_bit_capacity)
    {
        // Setup: 1024-bit bitset
        auto mask = bitset<1024>{};
        EXPECT_EQ(mask.size(), 1024u);
        EXPECT_EQ(mask.num_words(), 16u);

        // Act: Set bits distributed across words (e.g. core 0, core 64, core 128, core 1023)
        mask.set(0);
        mask.set(64);
        mask.set(128);
        mask.set(511);
        mask.set(1023);

        // Assert: Every set bit is individually testable and popcount is exact
        EXPECT_TRUE(mask.test(0));
        EXPECT_TRUE(mask.test(64));
        EXPECT_TRUE(mask.test(128));
        EXPECT_TRUE(mask.test(511));
        EXPECT_TRUE(mask.test(1023));

        EXPECT_FALSE(mask.test(1));
        EXPECT_FALSE(mask.test(65));
        EXPECT_FALSE(mask.test(1022));

        EXPECT_EQ(mask.count(), 5u);
        EXPECT_TRUE(mask.any());
        EXPECT_FALSE(mask.none());
        EXPECT_FALSE(mask.all());

        // Act: Reset all bits
        mask.reset();
        EXPECT_EQ(mask.count(), 0u);
        EXPECT_TRUE(mask.none());
    }

    // =========================================================================
    // SECTION: Bitwise Operators
    // =========================================================================

    /// @brief Verifies bitwise AND, OR, XOR, NOT, and equality operators across multi-word sets.
    TEST(bitset_test, bitwise_operations_and_equality)
    {
        // Setup: Two 128-bit sets with overlapping bits
        auto first = bitset<128>{};
        auto second = bitset<128>{};

        first.set(10);
        first.set(70);

        second.set(70);
        second.set(100);

        // Act & Assert: Bitwise AND
        const auto and_result = first & second;
        EXPECT_EQ(and_result.count(), 1u);
        EXPECT_TRUE(and_result.test(70));
        EXPECT_FALSE(and_result.test(10));
        EXPECT_FALSE(and_result.test(100));

        // Act & Assert: Bitwise OR
        const auto or_result = first | second;
        EXPECT_EQ(or_result.count(), 3u);
        EXPECT_TRUE(or_result.test(10));
        EXPECT_TRUE(or_result.test(70));
        EXPECT_TRUE(or_result.test(100));

        // Act & Assert: Bitwise XOR
        const auto xor_result = first ^ second;
        EXPECT_EQ(xor_result.count(), 2u);
        EXPECT_TRUE(xor_result.test(10));
        EXPECT_FALSE(xor_result.test(70));
        EXPECT_TRUE(xor_result.test(100));

        // Act & Assert: Equality and Inequality
        EXPECT_NE(first, second);
        EXPECT_EQ(first, first);

        auto copy = first;
        EXPECT_EQ(copy, first);
        copy.set(0);
        EXPECT_NE(copy, first);
    }

    // =========================================================================
    // SECTION: Bit Search & Scanning Operations
    // =========================================================================

    /// @brief Verifies find_first() and find_next() correctly locate indices of set bits
    ///        across multiple word boundaries.
    TEST(bitset_test, find_first_and_find_next)
    {
        // Setup: 256-bit set
        auto bits = bitset<256>{};
        EXPECT_FALSE(bits.find_first().has_value());

        // Act: Set bits at arbitrary distributed positions
        bits.set(4);
        bits.set(64);
        bits.set(65);
        bits.set(200);

        // Assert: Sequential scanning visits exactly the set bits in ascending order
        const auto first = bits.find_first();
        ASSERT_TRUE(first.has_value());
        EXPECT_EQ(*first, 4u);

        const auto second = bits.find_next(*first);
        ASSERT_TRUE(second.has_value());
        EXPECT_EQ(*second, 64u);

        const auto third = bits.find_next(*second);
        ASSERT_TRUE(third.has_value());
        EXPECT_EQ(*third, 65u);

        const auto fourth = bits.find_next(*third);
        ASSERT_TRUE(fourth.has_value());
        EXPECT_EQ(*fourth, 200u);

        const auto end_pos = bits.find_next(*fourth);
        EXPECT_FALSE(end_pos.has_value());
    }

    // =========================================================================
    // SECTION: Direct Memory Interoperability
    // =========================================================================

    /// @brief Verifies raw word accessors and data pointer interoperability for OS API integration.
    TEST(bitset_test, raw_word_and_data_pointer_access)
    {
        // Setup: 1024-bit bitset
        auto mask = bitset<1024>{};

        // Act: Direct word mutation
        mask.set_word(0, 0x00000000FFFFFFFFULL);
        mask.set_word(1, 0xFFFFFFFF00000000ULL);

        // Assert: Individual bits match raw word assignments
        EXPECT_EQ(mask.word(0), 0x00000000FFFFFFFFULL);
        EXPECT_EQ(mask.word(1), 0xFFFFFFFF00000000ULL);
        EXPECT_EQ(mask.count(), 64u);
        EXPECT_TRUE(mask.test(0));
        EXPECT_TRUE(mask.test(31));
        EXPECT_FALSE(mask.test(32));
        EXPECT_TRUE(mask.test(96));

        // Data pointer points to underlying contiguous words
        const auto* raw_data = mask.data();
        EXPECT_EQ(raw_data[0], 0x00000000FFFFFFFFULL);
        EXPECT_EQ(raw_data[1], 0xFFFFFFFF00000000ULL);
    }
} // namespace tempest::tests
