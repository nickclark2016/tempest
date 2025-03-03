#include <tempest/compare.hpp>

#include <gtest/gtest.h>

TEST(tempest_compare, three_way_compare_integrals)
{
    ASSERT_EQ(tempest::compare_three_way{}(1, 2), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(2, 1), tempest::strong_ordering::greater);
    ASSERT_EQ(tempest::compare_three_way{}(1, 1), tempest::strong_ordering::equal);

    // Mix signed and unsigned
    ASSERT_EQ(tempest::compare_three_way{}(1, 1u), tempest::strong_ordering::equal);
    ASSERT_EQ(tempest::compare_three_way{}(1u, 1), tempest::strong_ordering::equal);
    ASSERT_EQ(tempest::compare_three_way{}(1, 2u), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(2u, 1), tempest::strong_ordering::greater);

    // Mix signed and unsigned with negative values
    ASSERT_EQ(tempest::compare_three_way{}(-1, 1u), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(1u, -1), tempest::strong_ordering::greater);
    ASSERT_EQ(tempest::compare_three_way{}(-1, 1), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(1, -1), tempest::strong_ordering::greater);
    ASSERT_EQ(tempest::compare_three_way{}(-1, -1), tempest::strong_ordering::equal);
    ASSERT_EQ(tempest::compare_three_way{}(1u, 1u), tempest::strong_ordering::equal);

    // Mix integer widths
    ASSERT_EQ(tempest::compare_three_way{}(1, 1ll), tempest::strong_ordering::equal);
    ASSERT_EQ(tempest::compare_three_way{}(1ll, 1), tempest::strong_ordering::equal);
    ASSERT_EQ(tempest::compare_three_way{}(1, 2ll), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(2ll, 1), tempest::strong_ordering::greater);

    // Mix integer widths and signedness
    ASSERT_EQ(tempest::compare_three_way{}(1, 1ull), tempest::strong_ordering::equal);
    ASSERT_EQ(tempest::compare_three_way{}(1ull, 1), tempest::strong_ordering::equal);
    ASSERT_EQ(tempest::compare_three_way{}(1, 2ull), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(2ull, 1), tempest::strong_ordering::greater);

    // Mix integer widths and signedness with negative values
    ASSERT_EQ(tempest::compare_three_way{}(-1, 1ull), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(1ull, -1), tempest::strong_ordering::greater);
    ASSERT_EQ(tempest::compare_three_way{}(-1, 1ll), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(1ll, -1), tempest::strong_ordering::greater);
    ASSERT_EQ(tempest::compare_three_way{}(-1, -1ll), tempest::strong_ordering::equal);
    ASSERT_EQ(tempest::compare_three_way{}(-1ll, -1), tempest::strong_ordering::equal);
}

TEST(test_compare, three_way_compare_floating_points)
{
    ASSERT_EQ(tempest::compare_three_way{}(1.0f, 2.0f), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(2.0f, 1.0f), tempest::strong_ordering::greater);
    ASSERT_EQ(tempest::compare_three_way{}(1.0f, 1.0f), tempest::strong_ordering::equal);

    ASSERT_EQ(tempest::compare_three_way{}(1.0, 2.0), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(2.0, 1.0), tempest::strong_ordering::greater);
    ASSERT_EQ(tempest::compare_three_way{}(1.0, 1.0), tempest::strong_ordering::equal);

    // Mixed width
    ASSERT_EQ(tempest::compare_three_way{}(1.0f, 2.0), tempest::strong_ordering::less);
    ASSERT_EQ(tempest::compare_three_way{}(2.0, 1.0f), tempest::strong_ordering::greater);
    ASSERT_EQ(tempest::compare_three_way{}(1.0f, 1.0), tempest::strong_ordering::equal);
    ASSERT_EQ(tempest::compare_three_way{}(1.0, 1.0f), tempest::strong_ordering::equal);

    // Test against NaN
    ASSERT_EQ(tempest::compare_three_way{}(1.0f, tempest::numeric_limits<float>::quiet_NaN()),
              tempest::strong_ordering::less);

    ASSERT_EQ(tempest::compare_three_way{}(tempest::numeric_limits<float>::quiet_NaN(), 1.0f),
              tempest::strong_ordering::equal);

    ASSERT_EQ(tempest::compare_three_way{}(tempest::numeric_limits<float>::quiet_NaN(),
                                           tempest::numeric_limits<float>::quiet_NaN()),
              tempest::strong_ordering::equal);
}