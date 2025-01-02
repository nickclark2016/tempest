#include <gtest/gtest.h>

#include <tempest/variant.hpp>

TEST(variant, variant_alternative)
{
    tempest::variant<int, float> v;

    using T1 = tempest::variant_alternative_t<0, tempest::variant<int, float>>;
    using T2 = tempest::variant_alternative_t<1, tempest::variant<int, float>>;
    using T3 = tempest::variant_alternative_t<2, tempest::variant<int, float>>;

    EXPECT_TRUE((tempest::is_same_v<T1, int>));
    EXPECT_TRUE((tempest::is_same_v<T2, float>));
    EXPECT_TRUE((tempest::is_same_v<T3, void>));
}

TEST(variant, has_duplicate_types)
{
    EXPECT_TRUE((tempest::detail::has_duplicate_types<int, float, int>::value));
    EXPECT_FALSE((tempest::detail::has_duplicate_types<int, float, char>::value));
}

TEST(variant, default_constructor)
{
    tempest::variant<int, float> v;

    EXPECT_EQ(v.index(), 0);
}

TEST(variant, constructor_with_value)
{
    tempest::variant<int, float> v(42);
    EXPECT_EQ(v.index(), 0);
    EXPECT_EQ(tempest::get<0>(v), 42);
    EXPECT_EQ(tempest::get<int>(v), 42);
    EXPECT_EQ(tempest::get_if<1>(&v), nullptr);
    EXPECT_EQ(tempest::get_if<float>(&v), nullptr);

    tempest::variant<int, float> v2(3.14f);
    EXPECT_EQ(v2.index(), 1);
    EXPECT_EQ(tempest::get<1>(v2), 3.14f);
    EXPECT_EQ(tempest::get<float>(v2), 3.14f);
    EXPECT_EQ(tempest::get_if<0>(&v2), nullptr);
    EXPECT_EQ(tempest::get_if<int>(&v2), nullptr);
}

TEST(variant, emplace_construct)
{
    struct Foo
    {
        Foo(int a, float b) : a(a), b(b)
        {
        }

        int a;
        float b;
    };

    tempest::variant<int, Foo> v(tempest::in_place_type<Foo>, 42, 3.14f);
    EXPECT_EQ(v.index(), 1);
    EXPECT_EQ(tempest::get<Foo>(v).a, 42);
    EXPECT_EQ(tempest::get<Foo>(v).b, 3.14f);

    tempest::variant<int, Foo> v2(tempest::in_place_index<1>, 42, 3.14f);
    EXPECT_EQ(v2.index(), 1);
    EXPECT_EQ(tempest::get<Foo>(v2).a, 42);
    EXPECT_EQ(tempest::get<Foo>(v2).b, 3.14f);
}

TEST(variant, assignment)
{
    tempest::variant<int, float> v(42);
    tempest::variant<int, float> v2(3.14f);

    EXPECT_EQ(v.index(), 0);
    EXPECT_EQ(tempest::get<0>(v), 42);
    EXPECT_EQ(v2.index(), 1);
    EXPECT_EQ(tempest::get<1>(v2), 3.14f);

    v = v2;
    EXPECT_EQ(v.index(), 1);
    EXPECT_EQ(tempest::get<1>(v), 3.14f);

    v = 42;
    EXPECT_EQ(v.index(), 0);
    EXPECT_EQ(tempest::get<0>(v), 42);
}

TEST(variant, move_assignment)
{
    tempest::variant<int, float> v(42);
    tempest::variant<int, float> v2(3.14f);

    EXPECT_EQ(v.index(), 0);
    EXPECT_EQ(tempest::get<0>(v), 42);
    EXPECT_EQ(v2.index(), 1);
    EXPECT_EQ(tempest::get<1>(v2), 3.14f);

    v = tempest::move(v2);
    EXPECT_EQ(v.index(), 1);
    EXPECT_EQ(tempest::get<1>(v), 3.14f);
}

TEST(variant, visit)
{
    tempest::variant<int, float> v(42);

    auto callable = [](auto&& arg) -> bool {
        if constexpr (tempest::is_same_v<tempest::remove_cvref_t<decltype(arg)>, int>)
        {
            return true;
        }
        else
        {
            return false;
        }
    };

    EXPECT_EQ(v.visit(callable), true);
    EXPECT_EQ(tempest::visit(callable, v), true);

    tempest::variant<int, float> v2(3.14f);

    EXPECT_EQ(v2.visit(callable), false);
    EXPECT_EQ(tempest::visit(callable, v2), false);

    // Ensure void return compiles
    auto void_callable = []([[maybe_unused]] auto&& arg) {};
    v.visit(void_callable);
    tempest::visit(void_callable, v);
}

TEST(variant, visit_with_return)
{
    // Verify void return compiles
    auto void_callable = []([[maybe_unused]] auto&& arg) {};

    tempest::variant<int, float> v(42);
    v.visit<void>(void_callable);
    tempest::visit<void>(void_callable, v);

    // Verify non-void return compiles
    auto callable = [](auto&& arg) -> bool {
        if constexpr (tempest::is_same_v<tempest::remove_cvref_t<decltype(arg)>, int>)
        {
            return true;
        }
        else
        {
            return false;
        }
    };

    EXPECT_EQ(v.visit<bool>(callable), true);
    EXPECT_EQ(tempest::visit<bool>(callable, v), true);

    tempest::variant<int, float> v2(3.14f);

    EXPECT_EQ(v2.visit<bool>(callable), false);
    EXPECT_EQ(tempest::visit<bool>(callable, v2), false);
}

TEST(variant, visit_with_multiple_call_operators)
{
    struct Callable
    {
        bool operator()(int) const
        {
            return true;
        }
        bool operator()(float) const
        {
            return false;
        }
    };

    tempest::variant<int, float> v(42);
    EXPECT_EQ(v.visit(Callable{}), true);

    tempest::variant<int, float> v2(3.14f);
    EXPECT_EQ(v2.visit(Callable{}), false);
}
