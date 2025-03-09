#include <gtest/gtest.h>

#include <tempest/utility.hpp>

TEST(pair, default_constructor)
{
    tempest::pair<int, int> p{};

    EXPECT_EQ(p.first, 0);
    EXPECT_EQ(p.second, 0);
}

TEST(pair, copy_object_constructor)
{
    tempest::pair<int, int> p{1, 2};

    EXPECT_EQ(p.first, 1);
    EXPECT_EQ(p.second, 2);
}

TEST(pair, perfect_forward_constructor)
{
    tempest::pair<double, double> p{1.0f, 2.0f};

    EXPECT_EQ(p.first, 1.0);
    EXPECT_EQ(p.second, 2.0);
}

TEST(pair, copy_constructor_from_convertible)
{
    tempest::pair<float, float> p{1.0f, 2.0f};
    tempest::pair<double, double> p2{p};

    EXPECT_EQ(p2.first, 1.0);
    EXPECT_EQ(p2.second, 2.0);
}

TEST(pair, move_constructor_from_convertible)
{
    tempest::pair<float, float> p{1.0f, 2.0f};
    tempest::pair<double, double> p2{move(p)};

    EXPECT_EQ(p2.first, 1.0);
    EXPECT_EQ(p2.second, 2.0);
}

TEST(pair, copy_constructor)
{
    tempest::pair<int, int> p1{1, 2};
    tempest::pair<int, int> p2{p1};

    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, 2);
}

TEST(pair, move_constructor)
{
    tempest::pair<int, int> p1{1, 2};
    tempest::pair<int, int> p2{move(p1)};

    EXPECT_EQ(p2.first, 1);
    EXPECT_EQ(p2.second, 2);
}

TEST(pair, structured_binding)
{
    tempest::pair<int, int> p{1, 2};
    auto [first, second] = p;

    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(pair, structured_binding_ref)
{
    tempest::pair<int, int> p{1, 2};
    auto& [first, second] = p;

    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);

    first = 3;
    second = 4;

    EXPECT_EQ(p.first, 3);
    EXPECT_EQ(p.second, 4);
}

TEST(pair, structured_binding_const_ref)
{
    tempest::pair<int, int> p{1, 2};
    const auto& [first, second] = p;

    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(pair, structured_binding_move)
{
    tempest::pair<int, int> p{1, 2};
    auto [first, second] = move(p);

    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(pair, compare)
{
    tempest::pair<int, int> p1{1, 2};
    tempest::pair<int, int> p2{1, 2};
    tempest::pair<int, int> p3{2, 1};
    EXPECT_EQ(p1, p2);
    EXPECT_NE(p1, p3);
    EXPECT_LT(p1, p3);
    EXPECT_LE(p1, p2);
    EXPECT_GT(p3, p1);
    EXPECT_GE(p2, p1);
    EXPECT_EQ(p1 <=> p2, tempest::strong_ordering::equal);
    EXPECT_EQ(p1 <=> p3, tempest::strong_ordering::less);
    EXPECT_EQ(p3 <=> p1, tempest::strong_ordering::greater);
}
