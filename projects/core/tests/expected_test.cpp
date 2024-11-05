#include <gtest/gtest.h>

#include <tempest/expected.hpp>

TEST(expected, default_constructor)
{
    tempest::expected<int, int> e;

    EXPECT_TRUE(e.has_value());
}

TEST(expected, copy_constructor)
{
    tempest::expected<int, int> e(tempest::unexpect);
    tempest::expected<int, int> e2(e);

    EXPECT_FALSE(e2.has_value());
}

TEST(expected, move_constructor)
{
    tempest::expected<int, int> e(tempest::unexpect);
    tempest::expected<int, int> e2(tempest::move(e));

    EXPECT_FALSE(e2.has_value());
}

TEST(expected, value_construction)
{
    tempest::expected<int, int> e(42);

    EXPECT_TRUE(e.has_value());
    EXPECT_EQ(*e, 42);
}

TEST(expected, error_construction)
{
    tempest::expected<int, int> e(tempest::unexpected(42));

    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), 42);
}

TEST(expected, copy_assignment)
{
    tempest::expected<int, int> e(tempest::unexpect);
    tempest::expected<int, int> e2 = e;

    EXPECT_FALSE(e2.has_value());
}

TEST(expected, move_assignment)
{
    tempest::expected<int, int> e(tempest::unexpect);
    tempest::expected<int, int> e2 = tempest::move(e);

    EXPECT_FALSE(e2.has_value());
}

TEST(expected, result_value_assignment)
{
    tempest::expected<int, int> e(tempest::unexpect);

    e = 42;

    EXPECT_TRUE(e.has_value());
    EXPECT_EQ(*e, 42);
}

TEST(expected, result_error_assignment)
{
    tempest::expected<int, int> e(tempest::unexpect);

    e = tempest::unexpected(42);

    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), 42);
}

TEST(expected, value_or_with_value)
{
    tempest::expected<int, int> e(42);

    EXPECT_EQ(e.value_or(0), 42);
}

TEST(expected, value_or_with_error)
{
    tempest::expected<int, int> e(tempest::unexpected(42));

    EXPECT_EQ(e.value_or(0), 0);
}

TEST(expected, error_or_with_value)
{
    tempest::expected<int, int> e(42);

    EXPECT_EQ(e.error_or(0), 0);
}

TEST(expected, error_or_with_error)
{
    tempest::expected<int, int> e(tempest::unexpected(42));

    EXPECT_EQ(e.error_or(0), 42);
}