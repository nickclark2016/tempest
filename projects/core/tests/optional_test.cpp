#include <tempest/optional.hpp>

#include <gtest/gtest.h>

TEST(optional, default_construct)
{
    tempest::optional<int> opt;
    EXPECT_FALSE(opt.has_value());
}

TEST(optional, value_construct)
{
    tempest::optional<int> opt(42);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 42);
}

TEST(optional, copy_construct)
{
    tempest::optional<int> opt(42);
    tempest::optional<int> opt2(opt);
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt2.value(), 42);
}

TEST(optional, copy_construct_empty)
{
    tempest::optional<int> opt;
    tempest::optional<int> opt2(opt);
    EXPECT_FALSE(opt2.has_value());
}

TEST(optional, move_construct)
{
    tempest::optional<int> opt(42);
    tempest::optional<int> opt2(std::move(opt));
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt2.value(), 42);
}

TEST(optional, move_construct_empty)
{
    tempest::optional<int> opt;
    tempest::optional<int> opt2(std::move(opt));
    EXPECT_FALSE(opt2.has_value());
}

TEST(optional, copy_assign)
{
    tempest::optional<int> opt(42);
    tempest::optional<int> opt2;
    opt2 = opt;
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt2.value(), 42);
}

TEST(optional, copy_assign_empty)
{
    tempest::optional<int> opt;
    tempest::optional<int> opt2(42);
    opt2 = opt;
    EXPECT_FALSE(opt2.has_value());
}

TEST(optional, copy_assign_from_empty)
{
    tempest::optional<int> opt(42);
    tempest::optional<int> opt2;
    opt2 = opt;
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt2.value(), 42);
}

TEST(optional, move_assign)
{
    tempest::optional<int> opt(42);
    tempest::optional<int> opt2;
    opt2 = std::move(opt);
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt2.value(), 42);
}

TEST(optional, move_assign_empty)
{
    tempest::optional<int> opt;
    tempest::optional<int> opt2(42);
    opt2 = std::move(opt);
    EXPECT_FALSE(opt2.has_value());
}

TEST(optional, copy_assign_value)
{
    tempest::optional<int> opt(42);
    opt = 43;
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 43);
}

TEST(optional, copy_assign_empty_value)
{
    tempest::optional<int> opt;
    opt = 42;
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 42);
}

TEST(optional, assign_nullopt)
{
    tempest::optional<int> opt(42);
    opt = tempest::nullopt;
    EXPECT_FALSE(opt.has_value());
}

TEST(optional, move_assign_value)
{
    tempest::optional<int> opt(42);
    opt = 43;
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 43);
}

TEST(optional, move_assign_empty_value)
{
    tempest::optional<int> opt;
    opt = 42;
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 42);
}

TEST(optional, make_optional)
{
    auto opt = tempest::make_optional(42);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 42);
}

TEST(optional, none)
{
    tempest::optional<int> opt = tempest::none();
    EXPECT_FALSE(opt.has_value());
}

TEST(optional, some)
{
    tempest::optional<int> opt = tempest::some(42);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 42);
}

TEST(optional, emplace)
{
    tempest::optional<int> opt;
    opt.emplace(42);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 42);
}

TEST(optional, emplace_with_value)
{
    tempest::optional<int> opt(42);
    opt.emplace(43);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 43);
}

TEST(optional, swap)
{
    tempest::optional<int> opt(42);
    tempest::optional<int> opt2(43);
    opt.swap(opt2);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 43);
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt2.value(), 42);
}

TEST(optional, swap_lhs_empty)
{
    tempest::optional<int> opt;
    tempest::optional<int> opt2(42);
    opt.swap(opt2);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 42);
    EXPECT_FALSE(opt2.has_value());
}

TEST(optional, swap_rhs_empty)
{
    tempest::optional<int> opt(42);
    tempest::optional<int> opt2;
    opt.swap(opt2);
    EXPECT_FALSE(opt.has_value());
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt2.value(), 42);
}

TEST(optional, swap_both_empty)
{
    tempest::optional<int> opt;
    tempest::optional<int> opt2;
    opt.swap(opt2);
    EXPECT_FALSE(opt.has_value());
    EXPECT_FALSE(opt2.has_value());
}

TEST(optional, value_or)
{
    tempest::optional<int> opt(42);
    EXPECT_EQ(opt.value_or(43), 42);
}

TEST(optional, value_or_empty)
{
    tempest::optional<int> opt;
    EXPECT_EQ(opt.value_or(43), 43);
}