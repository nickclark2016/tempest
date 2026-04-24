#include <tempest/tuple.hpp>

#include <gtest/gtest.h>

TEST(tuple, default_construct)
{
    tempest::tuple<int, float, char> t;

    EXPECT_EQ(tempest::get<0>(t), 0);
    EXPECT_EQ(tempest::get<1>(t), 0.0f);
    EXPECT_EQ(tempest::get<2>(t), '\0');
}

TEST(tuple, construct)
{
    tempest::tuple<int, float, char> t(1, 2.0f, '3');

    EXPECT_EQ(tempest::get<0>(t), 1);
    EXPECT_EQ(tempest::get<1>(t), 2.0f);
    EXPECT_EQ(tempest::get<2>(t), '3');
}

TEST(tuple, make_tuple)
{
    auto t = tempest::make_tuple(1, 2.0f, '3');

    EXPECT_EQ(tempest::get<0>(t), 1);
    EXPECT_EQ(tempest::get<1>(t), 2.0f);
    EXPECT_EQ(tempest::get<2>(t), '3');

    int n = 1;
    auto t2 = tempest::make_tuple(10, "Test", 3.14, tempest::ref(n), n);
    n = 7;

    EXPECT_EQ(tempest::get<0>(t2), 10);
    EXPECT_EQ(tempest::get<1>(t2), "Test");
    EXPECT_EQ(tempest::get<2>(t2), 3.14);
    EXPECT_EQ(tempest::get<3>(t2), 7);
    EXPECT_EQ(tempest::get<4>(t2), 1);
}

TEST(tuple, make_tuple_const_ref)
{
    const int n = 1;
    auto t = tempest::make_tuple(10, "Test", 3.14, tempest::cref(n));
    
    EXPECT_EQ(tempest::get<0>(t), 10);
    EXPECT_EQ(tempest::get<1>(t), "Test");
    EXPECT_EQ(tempest::get<2>(t), 3.14);
    EXPECT_EQ(tempest::get<3>(t), 1);
}

TEST(tuple, structured_bindings)
{
    auto t = tempest::make_tuple(1, 2.0f, '3');
    const auto& [a, b, c] = t;

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2.0f);
    EXPECT_EQ(c, '3');
}

TEST(tuple, apply_to_lambda)
{
    auto t = tempest::make_tuple(1, 2.0f, '3');
    auto result = tempest::apply([](int a, float b, char c) { return a + b + c; }, t);
    EXPECT_EQ(result, 1 + 2.0f + '3');
}

TEST(tuple, apply_to_static_member_fn)
{
    struct foo
    {
        static auto bar(int a, float b, char c)
        {
            return a + b + c;
        }
    };

    auto t = tempest::make_tuple(1, 2.0f, '3');
    auto result = tempest::apply(&foo::bar, t);
    EXPECT_EQ(result, 1 + 2.0f + '3');
}

TEST(tuple, apply_to_member_fn)
{
    struct foo
    {
        auto bar(int a, float b, char c)
        {
            return a + b + c;
        }
    };

    auto t = tempest::make_tuple(foo(), 1, 2.0f, '3');
    auto result = tempest::apply(&foo::bar, t);
    EXPECT_EQ(result, 1 + 2.0f + '3');
}

namespace
{
    auto free_fn(int a, float b, char c)
    {
        return a + b + c;
    }
} // namespace

TEST(tuple, apply_to_free_fn)
{
    auto t = tempest::make_tuple(1, 2.0f, '3');
    auto result = tempest::apply(free_fn, t);
    EXPECT_EQ(result, 1 + 2.0f + '3');
}
