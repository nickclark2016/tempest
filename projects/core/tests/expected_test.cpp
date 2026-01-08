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

TEST(expected, and_then_with_value)
{
    tempest::expected<int, int> e(10);
    auto e2 = e.and_then([](int value) { return tempest::expected<int, int>(value * 2); });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(*e2, 20);
}

TEST(expected, and_then_with_value_return_error)
{
    tempest::expected<int, int> e(10);
    auto e2 = e.and_then([](int) { return tempest::expected<int, int>(tempest::unexpected(42)); });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 42);
}

TEST(expected, and_then_with_error)
{
    tempest::expected<int, int> e(tempest::unexpected(42));
    auto e2 = e.and_then([](int value) { return tempest::expected<int, int>(value * 2); });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 42);
}

TEST(expected, transform_with_value)
{
    tempest::expected<int, int> e(10);
    auto e2 = e.transform([](int value) { return value * 2; });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(*e2, 20);
}

TEST(expected, transform_with_error)
{
    tempest::expected<int, int> e(tempest::unexpected(42));
    auto e2 = e.transform([](int value) { return value * 2; });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 42);
}

TEST(expected, or_else_with_value)
{
    tempest::expected<int, int> e(10);
    auto e2 = e.or_else([](int) { return tempest::expected<int, int>(42); });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(*e2, 10);
}

TEST(expected, or_else_with_error)
{
    tempest::expected<int, int> e(tempest::unexpected(42));
    auto e2 = e.or_else([](int) { return tempest::expected<int, int>(84); });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(*e2, 84);
}

TEST(expected, or_else_with_error_return_error)
{
    tempest::expected<int, int> e(tempest::unexpected(42));
    auto e2 = e.or_else([](int err) { return tempest::expected<int, int>(tempest::unexpected(err + 1)); });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 43);
}

TEST(expected, transform_error_with_value)
{
    tempest::expected<int, int> e(10);
    auto e2 = e.transform_error([](int err) { return err + 1; });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(*e2, 10);
}

TEST(expected, transform_error_with_error)
{
    tempest::expected<int, int> e(tempest::unexpected(42));
    auto e2 = e.transform_error([](int err) { return err + 1; });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 43);
}

TEST(expected, equality_same_value)
{
    tempest::expected<int, int> e1(42);
    tempest::expected<int, int> e2(42);
    EXPECT_TRUE(e1 == e2);
    EXPECT_TRUE(e2 == e1);
}

TEST(expected, equality_same_error)
{
    tempest::expected<int, int> e1(tempest::unexpected(42));
    tempest::expected<int, int> e2(tempest::unexpected(42));
    EXPECT_TRUE(e1 == e2);
    EXPECT_TRUE(e2 == e1);
}

TEST(expected, equality_value_error)
{
    tempest::expected<int, int> e1(42);
    tempest::expected<int, int> e2(tempest::unexpected(42));
    EXPECT_FALSE(e1 == e2);
    EXPECT_FALSE(e2 == e1);
}

TEST(expected, equality_different_value)
{
    tempest::expected<int, int> e1(42);
    tempest::expected<int, int> e2(84);
    EXPECT_FALSE(e1 == e2);
    EXPECT_FALSE(e2 == e1);
}

TEST(expected, equality_different_error)
{
    tempest::expected<int, int> e1(tempest::unexpected(42));
    tempest::expected<int, int> e2(tempest::unexpected(84));
    EXPECT_FALSE(e1 == e2);
    EXPECT_FALSE(e2 == e1);
}

TEST(expected_void, default_constructor)
{
    tempest::expected<void, int> e;
    EXPECT_TRUE(e.has_value());
}

TEST(expected_void, error_construction)
{
    tempest::expected<void, int> e(tempest::unexpected(42));
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), 42);
}

TEST(expected_void, copy_constructor)
{
    tempest::expected<void, int> e(tempest::unexpected(42));
    tempest::expected<void, int> e2(e);
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 42);
}

TEST(expected_void, move_constructor)
{
    tempest::expected<void, int> e(tempest::unexpected(42));
    tempest::expected<void, int> e2(tempest::move(e));
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 42);
}

TEST(expected_void, error_assignment)
{
    tempest::expected<void, int> e;
    e = tempest::unexpected(42);
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error(), 42);
}

TEST(expected_void, error_or_with_value)
{
    tempest::expected<void, int> e;
    EXPECT_EQ(e.error_or(0), 0);
}

TEST(expected_void, error_or_with_error)
{
    tempest::expected<void, int> e(tempest::unexpected(42));
    EXPECT_EQ(e.error_or(0), 42);
}

TEST(expected_void, and_then_with_value)
{
    tempest::expected<void, int> e;
    auto e2 = e.and_then([]() { return tempest::expected<int, int>(42); });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(*e2, 42);
}

TEST(expected_void, and_then_with_value_return_unexpected)
{
    tempest::expected<void, int> e;
    auto e2 = e.and_then([]() { return tempest::expected<int, int>(tempest::unexpected(42)); });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 42);
}

TEST(expected_void, and_then_with_error)
{
    tempest::expected<void, int> e(tempest::unexpected(42));
    auto e2 = e.and_then([]() { return tempest::expected<int, int>(42); });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 42);
}

TEST(expected_void, transform_with_value)
{
    tempest::expected<void, int> e;
    auto e2 = e.transform([]() { return 42; });
    EXPECT_TRUE(e2.has_value());
    EXPECT_EQ(*e2, 42);
}

TEST(expected_void, transform_with_error)
{
    tempest::expected<void, int> e(tempest::unexpected(42));
    auto e2 = e.transform([]() { return 42; });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 42);
}

TEST(expected_void, or_else_with_value)
{
    tempest::expected<void, int> e;
    auto e2 = e.or_else([](int) { return tempest::expected<void, int>(); });
    EXPECT_TRUE(e2.has_value());
}

TEST(expected_void, or_else_with_error)
{
    tempest::expected<void, int> e(tempest::unexpected(42));
    auto e2 = e.or_else([](int) { return tempest::expected<void, int>(tempest::unexpected(84)); });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 84);
}

TEST(expected_void, or_else_with_error_return_error)
{
    tempest::expected<void, int> e(tempest::unexpected(42));
    auto e2 = e.or_else([](int err) { return tempest::expected<void, int>(tempest::unexpected(err + 1)); });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 43);
}

TEST(expected_void, transform_error_with_value)
{
    tempest::expected<void, int> e;
    auto e2 = e.transform_error([](int err) { return err + 1; });
    EXPECT_TRUE(e2.has_value());
}

TEST(expected_void, transform_error_with_error)
{
    tempest::expected<void, int> e(tempest::unexpected(42));
    auto e2 = e.transform_error([](int err) { return err + 1; });
    EXPECT_FALSE(e2.has_value());
    EXPECT_EQ(e2.error(), 43);
}

TEST(expected_void, expected_equal_void_both_value)
{
    tempest::expected<void, int> e1;
    tempest::expected<void, int> e2;
    EXPECT_TRUE(e1 == e2);
    EXPECT_TRUE(e2 == e1);
}

TEST(expected_void, expected_equal_void_both_error)
{
    tempest::expected<void, int> e1(tempest::unexpected(42));
    tempest::expected<void, int> e2(tempest::unexpected(42));
    EXPECT_TRUE(e1 == e2);
    EXPECT_TRUE(e2 == e1);
}

TEST(expected_void, expected_not_equal_void_value_error)
{
    tempest::expected<void, int> e1;
    tempest::expected<void, int> e2(tempest::unexpected(42));
    EXPECT_FALSE(e1 == e2);
    EXPECT_FALSE(e2 == e1);
}

TEST(expected_void, expected_not_equal_void_error_value)
{
    tempest::expected<void, int> e1(tempest::unexpected(42));
    tempest::expected<void, int> e2;
    EXPECT_FALSE(e1 == e2);
    EXPECT_FALSE(e2 == e1);
}

TEST(expected_void, expected_not_equal_void_both_different_error)
{
    tempest::expected<void, int> e1(tempest::unexpected(42));
    tempest::expected<void, int> e2(tempest::unexpected(84));
    EXPECT_FALSE(e1 == e2);
    EXPECT_FALSE(e2 == e1);
}

TEST(expected, visit_value_void_return)
{
    auto e = tempest::expected<char, int>{'c'};
    auto called = false;

    tempest::visit([&](auto c) {
        if constexpr (std::is_same_v<tempest::remove_cvref_t<decltype(c)>, char>)
        {
            called = true;
            EXPECT_EQ(c, 'c');
        }
    }, e);

    EXPECT_TRUE(called);
}

TEST(expected, visit_error_void_return)
{
    auto e = tempest::expected<char, int>{tempest::unexpected(42)};
    auto called = false;

    tempest::visit([&](auto err) {
        if constexpr (std::is_same_v<tempest::remove_cvref_t<decltype(err)>, int>)
        {
            called = true;
            EXPECT_EQ(err, 42);
        }
    }, e);

    EXPECT_TRUE(called);
}

TEST(expected, visit_value_with_return)
{
    auto e = tempest::expected<char, int>{'c'};

    const auto result = tempest::visit([&](auto c) {
        if constexpr (std::is_same_v<tempest::remove_cvref_t<decltype(c)>, char>)
        {
            return true;
        }
        else
        {
            return false;
        }
    }, e);

    EXPECT_TRUE(result);
}

TEST(expected, visit_error_with_return)
{
    auto e = tempest::expected<char, int>{tempest::unexpected(42)};

    const auto result = tempest::visit([&](auto err) {
        if constexpr (std::is_same_v<tempest::remove_cvref_t<decltype(err)>, int>)
        {
            return err;
        }
        else
        {
            return -1;
        }
    }, e);

    EXPECT_EQ(result, 42);
}

TEST(expected, visit_value_with_return_callable_object)
{
    struct Callable
    {
        char operator()(char c) const
        {
            return c;
        }

        char operator()(int) const
        {
            return 'i';
        }
    };

    auto e = tempest::expected<char, int>{'d'};
    const auto result = tempest::visit(Callable{}, e);

    EXPECT_EQ(result, 'd');
}

TEST(expected, visit_error_with_return_callable_object)
{
    struct Callable
    {
        int operator()(char) const
        {
            return -1;
        }
        int operator()(int err) const
        {
            return err;
        }
    };

    auto e = tempest::expected<char, int>{tempest::unexpected(84)};
    const auto result = tempest::visit(Callable{}, e);
    EXPECT_EQ(result, 84);
}

TEST(expected_void, visit_value_void_return)
{
    auto e = tempest::expected<void, int>{};
    auto called = false;

    tempest::visit([&](auto...) { called = true; }, e);

    EXPECT_TRUE(called);
}

TEST(expected_void, visit_error_with_return_callable_object)
{
    struct Callable
    {
        int operator()() const
        {
            return -1;
        }

        int operator()(int err) const
        {
            return err;
        }
    };

    auto e = tempest::expected<void, int>{tempest::unexpected(168)};
    const auto result = tempest::visit(Callable{}, e);

    EXPECT_EQ(result, 168);
}

TEST(expected_void, visit_value_with_return_callable_object)
{
    struct Callable
    {
        int operator()() const
        {
            return 42;
        }

        int operator()(int) const
        {
            return -1;
        }
    };

    auto e = tempest::expected<void, int>{};
    const auto result = tempest::visit(Callable{}, e);
    
    EXPECT_EQ(result, 42);
}
