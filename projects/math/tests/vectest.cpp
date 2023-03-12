#include <gtest/gtest.h>

#include <tempest/vec.hpp>
#include <cstdint>

using namespace tempest::math;

TEST(vec2, DefaultConstructor)
{
    vec<float, 2> a;

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);

    vec<double, 2> b;

    EXPECT_EQ(b.x, 0);
    EXPECT_EQ(b.y, 0);

    vec<std::int32_t, 2> c;

    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);

    vec<std::uint32_t, 2> d;

    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
}

TEST(vec3, DefaultConstructor)
{
    vec<float, 3> a;

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);
    EXPECT_EQ(a.z, 0);

    vec<double, 3> b;

    EXPECT_EQ(b.x, 0);
    EXPECT_EQ(b.y, 0);
    EXPECT_EQ(b.z, 0);

    vec<std::int32_t, 3> c;

    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);
    EXPECT_EQ(c.z, 0);

    vec<std::uint32_t, 3> d;

    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
    EXPECT_EQ(d.z, 0);
}

TEST(vec4, DefaultConstructor)
{
    vec<float, 4> a;

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);
    EXPECT_EQ(a.z, 0);
    EXPECT_EQ(a.w, 0);

    vec<double, 4> b;

    EXPECT_EQ(b.x, 0);
    EXPECT_EQ(b.y, 0);
    EXPECT_EQ(b.z, 0);
    EXPECT_EQ(b.w, 0);

    vec<std::int32_t, 4> c;

    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);
    EXPECT_EQ(c.z, 0);
    EXPECT_EQ(c.w, 0);

    vec<std::uint32_t, 4> d;

    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
    EXPECT_EQ(d.z, 0);
    EXPECT_EQ(d.w, 0);
}

TEST(vec2, ConstExprDefaultConstructor)
{
    constexpr vec<float, 2> a;

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);

    constexpr vec<double, 2> b;

    EXPECT_EQ(b.x, 0);
    EXPECT_EQ(b.y, 0);

    constexpr vec<std::int32_t, 2> c;

    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);

    constexpr vec<std::uint32_t, 2> d;

    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
}

TEST(vec3, ConstExprDefaultConstructor)
{
    constexpr vec<float, 3> a;

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);
    EXPECT_EQ(a.z, 0);

    constexpr vec<double, 3> b;

    EXPECT_EQ(b.x, 0);
    EXPECT_EQ(b.y, 0);
    EXPECT_EQ(b.z, 0);

    constexpr vec<std::int32_t, 3> c;

    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);
    EXPECT_EQ(c.z, 0);

    constexpr vec<std::uint32_t, 3> d;

    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
    EXPECT_EQ(d.z, 0);
}

TEST(vec4, ConstExprDefaultConstructor)
{
    constexpr vec<float, 4> a;

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);
    EXPECT_EQ(a.z, 0);
    EXPECT_EQ(a.w, 0);

    constexpr vec<double, 4> b;

    EXPECT_EQ(b.x, 0);
    EXPECT_EQ(b.y, 0);
    EXPECT_EQ(b.z, 0);
    EXPECT_EQ(b.w, 0);

    constexpr vec<std::int32_t, 4> c;

    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);
    EXPECT_EQ(c.z, 0);
    EXPECT_EQ(c.w, 0);

    constexpr vec<std::uint32_t, 4> d;

    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
    EXPECT_EQ(d.z, 0);
    EXPECT_EQ(d.w, 0);
}

TEST(vec2, SingleValueConstructor)
{
    vec<float, 2> a(5);

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 5);

    vec<double, 2> b(6);

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 6);

    vec<std::int32_t, 2> c(2);

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 2);

    vec<std::uint32_t, 2> d(9);

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 9);
}

TEST(vec3, SingleValueConstructor)
{
    vec<float, 3> a(2);

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 2);
    EXPECT_EQ(a.z, 2);

    vec<double, 3> b(5);

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 5);
    EXPECT_EQ(b.z, 5);

    vec<std::int32_t, 3> c(1);

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 1);
    EXPECT_EQ(c.z, 1);

    vec<std::uint32_t, 3> d(10);

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 10);
    EXPECT_EQ(d.z, 10);
}

TEST(vec4, SingleValueConstructor)
{
    vec<float, 4> a(6);

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 6);
    EXPECT_EQ(a.w, 6);

    vec<double, 4> b(2);

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 2);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 2);

    vec<std::int32_t, 4> c(7);

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 7);
    EXPECT_EQ(c.z, 7);
    EXPECT_EQ(c.w, 7);

    vec<std::uint32_t, 4> d(6);

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 6);
    EXPECT_EQ(d.z, 6);
    EXPECT_EQ(d.w, 6);
}

TEST(vec2, ConstExprSingleValueConstructor)
{
    constexpr vec<float, 2> a(5);

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 5);

    constexpr vec<double, 2> b(6);

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 6);

    constexpr vec<std::int32_t, 2> c(2);

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 2);

    constexpr vec<std::uint32_t, 2> d(9);

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 9);
}

TEST(vec3, ConstExprSingleValueConstructor)
{
    constexpr vec<float, 3> a(2);

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 2);
    EXPECT_EQ(a.z, 2);

    constexpr vec<double, 3> b(5);

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 5);
    EXPECT_EQ(b.z, 5);

    constexpr vec<std::int32_t, 3> c(1);

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 1);
    EXPECT_EQ(c.z, 1);

    constexpr vec<std::uint32_t, 3> d(10);

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 10);
    EXPECT_EQ(d.z, 10);
}

TEST(vec4, ConstExprSingleValueConstructor)
{
    constexpr vec<float, 4> a(6);

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 6);
    EXPECT_EQ(a.w, 6);

    constexpr vec<double, 4> b(2);

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 2);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 2);

    constexpr vec<std::int32_t, 4> c(7);

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 7);
    EXPECT_EQ(c.z, 7);
    EXPECT_EQ(c.w, 7);

    constexpr vec<std::uint32_t, 4> d(6);

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 6);
    EXPECT_EQ(d.z, 6);
    EXPECT_EQ(d.w, 6);
}

TEST(vec2, ParameterConstructor)
{
    vec<float, 2> a(5, 10);

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 10);

    vec<double, 2> b(6, 2);

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 2);

    vec<std::int32_t, 2> c(2, 1);

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 1);

    vec<std::uint32_t, 2> d(9, 8);

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 8);
}

TEST(vec3, ParameterConstructor)
{
    vec<float, 3> a(2, 1, 5);

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 1);
    EXPECT_EQ(a.z, 5);

    vec<double, 3> b(5, 8 ,9);

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 8);
    EXPECT_EQ(b.z, 9);

    vec<std::int32_t, 3> c(1, 2, 3);

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 2);
    EXPECT_EQ(c.z, 3);

    vec<std::uint32_t, 3> d(10, 5, 1);

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 5);
    EXPECT_EQ(d.z, 1);
}

TEST(vec4, ParameterConstructor)
{
    vec<float, 4> a(6, 6, 5, 2);

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 5);
    EXPECT_EQ(a.w, 2);

    vec<double, 4> b(2, 6, 2, 1);

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 6);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 1);

    vec<std::int32_t, 4> c(7, 6, 1, 4);

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 6);
    EXPECT_EQ(c.z, 1);
    EXPECT_EQ(c.w, 4);

    vec<std::uint32_t, 4> d(6, 2, 2, 9);

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 2);
    EXPECT_EQ(d.z, 2);
    EXPECT_EQ(d.w, 9);
}

TEST(vec2, ConstExprParameterConstructor)
{
    constexpr vec<float, 2> a(5, 10);

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 10);

    constexpr vec<double, 2> b(6, 2);

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 2);

    constexpr vec<std::int32_t, 2> c(2, 1);

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 1);

    constexpr vec<std::uint32_t, 2> d(9, 8);

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 8);
}

TEST(vec3, ConstExprParameterConstructor)
{
    constexpr vec<float, 3> a(2, 1, 5);

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 1);
    EXPECT_EQ(a.z, 5);

    constexpr vec<double, 3> b(5, 8, 9);

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 8);
    EXPECT_EQ(b.z, 9);

    constexpr vec<std::int32_t, 3> c(1, 2, 3);

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 2);
    EXPECT_EQ(c.z, 3);

    constexpr vec<std::uint32_t, 3> d(10, 5, 1);

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 5);
    EXPECT_EQ(d.z, 1);
}

TEST(vec4, ConstExprParameterConstructor)
{
    constexpr vec<float, 4> a(6, 6, 5, 2);

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 5);
    EXPECT_EQ(a.w, 2);

    constexpr vec<double, 4> b(2, 6, 2, 1);

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 6);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 1);

    constexpr vec<std::int32_t, 4> c(7, 6, 1, 4);

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 6);
    EXPECT_EQ(c.z, 1);
    EXPECT_EQ(c.w, 4);

    constexpr vec<std::uint32_t, 4> d(6, 2, 2, 9);

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 2);
    EXPECT_EQ(d.z, 2);
    EXPECT_EQ(d.w, 9);
}

TEST(vec2, CopyConstructor)
{
    vec<float, 2> a(vec<float, 2>(5, 10));

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 10);

    vec<double, 2> b(vec<double, 2>(6, 2));

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 2);

    vec<std::int32_t, 2> c(vec<std::int32_t, 2>(2, 1));

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 1);

    vec<std::uint32_t, 2> d(vec<std::uint32_t, 2>(9, 8));

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 8);
}

TEST(vec3, CopyConstructor)
{
    vec<float, 3> a(vec<float, 3>(2, 1, 5));

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 1);
    EXPECT_EQ(a.z, 5);

    vec<double, 3> b(vec<double, 3>(5, 8, 9));

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 8);
    EXPECT_EQ(b.z, 9);

    vec<std::int32_t, 3> c(vec<std::int32_t, 3>(1, 2, 3));

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 2);
    EXPECT_EQ(c.z, 3);

    vec<std::uint32_t, 3> d(vec<std::uint32_t, 3>(10, 5, 1));

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 5);
    EXPECT_EQ(d.z, 1);
}

TEST(vec4, CopyConstructor)
{
    vec<float, 4> a(vec<float, 4>(6, 6, 5, 1));

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 5);
    EXPECT_EQ(a.w, 1);

    vec<double, 4> b(vec<double, 4>(2, 6, 2, 1));

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 6);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 1);

    vec<std::int32_t, 4> c(vec<std::int32_t, 4>(7, 6, 1, 4));

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 6);
    EXPECT_EQ(c.z, 1);
    EXPECT_EQ(c.w, 4);

    vec<std::uint32_t, 4> d(vec<std::uint32_t, 4>(6, 2, 2, 9));

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 2);
    EXPECT_EQ(d.z, 2);
    EXPECT_EQ(d.w, 9);
}

TEST(vec2, ConstExprCopyConstructor)
{
    constexpr vec<float, 2> a(vec<float, 2>(5, 10));

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 10);

    constexpr vec<double, 2> b(vec<double, 2>(6, 2));

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 2);

    constexpr vec<std::int32_t, 2> c(vec<std::int32_t, 2>(2, 1));

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 1);

    constexpr vec<std::uint32_t, 2> d(vec<std::uint32_t, 2>(9, 8));

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 8);
}

TEST(vec3, ConstExprCopyConstructor)
{
    constexpr vec<float, 3> a(vec<float, 3>(2, 1, 5));

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 1);
    EXPECT_EQ(a.z, 5);

    constexpr vec<double, 3> b(vec<double, 3>(5, 8, 9));

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 8);
    EXPECT_EQ(b.z, 9);

    constexpr vec<std::int32_t, 3> c(vec<std::int32_t, 3>(1, 2, 3));

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 2);
    EXPECT_EQ(c.z, 3);

    constexpr vec<std::uint32_t, 3> d(vec<std::uint32_t, 3>(10, 5, 1));

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 5);
    EXPECT_EQ(d.z, 1);
}

TEST(vec4, ConstExprCopyConstructor)
{
    constexpr vec<float, 4> a(vec<float, 4>(6, 6, 5, 1));

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 5);
    EXPECT_EQ(a.w, 1);

    constexpr vec<double, 4> b(vec<double, 4>(2, 6, 2, 1));

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 6);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 1);

    constexpr vec<std::int32_t, 4> c(vec<std::int32_t, 4>(7, 6, 1, 4));

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 6);
    EXPECT_EQ(c.z, 1);
    EXPECT_EQ(c.w, 4);

    constexpr vec<std::uint32_t, 4> d(vec<std::uint32_t, 4>(6, 2, 2, 9));

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 2);
    EXPECT_EQ(d.z, 2);
    EXPECT_EQ(d.w, 9);
}

TEST(vec2, MoveConstructor)
{
    vec<float, 2> a(std::move(vec<float, 2>(5, 10)));

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 10);

    vec<double, 2> b(std::move(vec<double, 2>(6, 2)));

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 2);

    vec<std::int32_t, 2> c(std::move(vec<std::int32_t, 2>(2, 1)));

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 1);

    vec<std::uint32_t, 2> d(std::move(vec<std::uint32_t, 2>(9, 8)));

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 8);
}

TEST(vec3, MoveConstructor)
{
    vec<float, 3> a(std::move(vec<float, 3>(2, 1, 5)));

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 1);
    EXPECT_EQ(a.z, 5);

    vec<double, 3> b(std::move(vec<double, 3>(5, 8, 9)));

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 8);
    EXPECT_EQ(b.z, 9);

    vec<std::int32_t, 3> c(std::move(vec<std::int32_t, 3>(1, 2, 3)));

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 2);
    EXPECT_EQ(c.z, 3);

    vec<std::uint32_t, 3> d(std::move(vec<std::uint32_t, 3>(10, 5, 1)));

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 5);
    EXPECT_EQ(d.z, 1);
}

TEST(vec4, MoveConstructor)
{
    vec<float, 4> a(std::move(vec<float, 4>(6, 6, 5, 1)));

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 5);
    EXPECT_EQ(a.w, 1);

    vec<double, 4> b(std::move(vec<double, 4>(2, 6, 2, 1)));

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 6);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 1);

    vec<std::int32_t, 4> c(std::move(vec<std::int32_t, 4>(7, 6, 1, 4)));

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 6);
    EXPECT_EQ(c.z, 1);
    EXPECT_EQ(c.w, 4);

    vec<std::uint32_t, 4> d(std::move(vec<std::uint32_t, 4>(6, 2, 2, 9)));

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 2);
    EXPECT_EQ(d.z, 2);
    EXPECT_EQ(d.w, 9);
}

TEST(vec2, ConstExprMoveConstructor)
{
    constexpr vec<float, 2> a(std::move(vec<float, 2>(5, 10)));

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 10);

    constexpr vec<double, 2> b(std::move(vec<double, 2>(6, 2)));

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 2);

    constexpr vec<std::int32_t, 2> c(std::move(vec<std::int32_t, 2>(2, 1)));

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 1);

    constexpr vec<std::uint32_t, 2> d(std::move(vec<std::uint32_t, 2>(9, 8)));

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 8);
}

TEST(vec3, ConstExprMoveConstructor)
{
    constexpr vec<float, 3> a(std::move(vec<float, 3>(2, 1, 5)));

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 1);
    EXPECT_EQ(a.z, 5);

    constexpr vec<double, 3> b(std::move(vec<double, 3>(5, 8, 9)));

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 8);
    EXPECT_EQ(b.z, 9);

    constexpr vec<std::int32_t, 3> c(std::move(vec<std::int32_t, 3>(1, 2, 3)));

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 2);
    EXPECT_EQ(c.z, 3);

    constexpr vec<std::uint32_t, 3> d(std::move(vec<std::uint32_t, 3>(10, 5, 1)));

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 5);
    EXPECT_EQ(d.z, 1);
}

TEST(vec4, ConstExprMoveConstructor)
{
    constexpr vec<float, 4> a(std::move(vec<float, 4>(6, 6, 5, 1)));

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 5);
    EXPECT_EQ(a.w, 1);

    constexpr vec<double, 4> b(std::move(vec<double, 4>(2, 6, 2, 1)));

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 6);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 1);

    constexpr vec<std::int32_t, 4> c(std::move(vec<std::int32_t, 4>(7, 6, 1, 4)));

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 6);
    EXPECT_EQ(c.z, 1);
    EXPECT_EQ(c.w, 4);

    constexpr vec<std::uint32_t, 4> d(std::move(vec<std::uint32_t, 4>(6, 2, 2, 9)));

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 2);
    EXPECT_EQ(d.z, 2);
    EXPECT_EQ(d.w, 9);
}

TEST(vec2, Zero)
{
    vec<float, 2> a(5, 10);
    a = vec<float, 2>::zero();

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);

    vec<double, 2> b(6, 2);
    b = vec<double, 2>::zero();

    EXPECT_EQ(b.x, 0);
    EXPECT_EQ(b.y, 0);

    vec<std::int32_t, 2> c(2, 1);
    c = vec<std::int32_t, 2>::zero();

    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);

    vec<std::uint32_t, 2> d(9, 8);
    d = vec<std::uint32_t, 2>::zero();

    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
}

TEST(vec3, Zero)
{
    vec<float, 3> a(2, 1, 5);
    a = vec<float, 3>::zero();

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);
    EXPECT_EQ(a.z, 0);

    vec<double, 3> b(5, 8, 9);
    b = vec<double, 3>::zero();

    EXPECT_EQ(b.x, 0);
    EXPECT_EQ(b.y, 0);
    EXPECT_EQ(b.z, 0);

    vec<std::int32_t, 3> c(1, 2, 3);
    c = vec<std::int32_t, 3>::zero();

    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);
    EXPECT_EQ(c.z, 0);

    vec<std::uint32_t, 3> d(10, 5, 1);
    d = vec<std::uint32_t, 3 >::zero();

    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
    EXPECT_EQ(d.z, 0);
}

TEST(vec4, Zero)
{
    vec<float, 4> a(6, 6, 5, 2);
    a = vec<float, 4>::zero();

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);
    EXPECT_EQ(a.z, 0);
    EXPECT_EQ(a.w, 0);

    vec<double, 4> b(2, 6, 2, 1);
    b = vec<double, 4>::zero();

    EXPECT_EQ(b.x, 0);
    EXPECT_EQ(b.y, 0);
    EXPECT_EQ(b.z, 0);
    EXPECT_EQ(b.w, 0);

    vec<std::int32_t, 4> c(7, 6, 1, 4);
    c = vec<std::int32_t, 4>::zero();

    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);
    EXPECT_EQ(c.z, 0);
    EXPECT_EQ(c.w, 0);

    vec<std::uint32_t, 4> d(6, 2, 2, 9);
    d = vec<std::uint32_t, 4>::zero();

    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
    EXPECT_EQ(d.z, 0);
    EXPECT_EQ(d.w, 0);
}

TEST(vec2, Set)
{
    vec<float, 2> a(54, 102);
    a.set(5, 10);

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 10);

    vec<double, 2> b(63, 22);
    b.set(6, 2);

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 2);

    vec<std::int32_t, 2> c(22, 11);
    c.set(2, 1);

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 1);

    vec<std::uint32_t, 2> d(92, 81);
    d.set(9, 8);

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 8);
}

TEST(vec3, Set)
{
    vec<float, 3> a(21, 12, 54);
    a.set(2, 1, 5);

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 1);
    EXPECT_EQ(a.z, 5);

    vec<double, 3> b(52, 81, 94);
    b.set(5, 8, 9);

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 8);
    EXPECT_EQ(b.z, 9);

    vec<std::int32_t, 3> c(12, 12, 32);
    c.set(1, 2, 3);

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 2);
    EXPECT_EQ(c.z, 3);

    vec<std::uint32_t, 3> d(102, 51, 14);
    d.set(10, 5, 1);

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 5);
    EXPECT_EQ(d.z, 1);
}

TEST(vec4, Set)
{
    vec<float, 4> a(62, 61, 54, 22);
    a.set(6, 6, 5, 2);

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 5);
    EXPECT_EQ(a.w, 2);

    vec<double, 4> b(22, 61, 23, 12);
    b.set(2, 6, 2, 1);

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 6);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 1);

    vec<std::int32_t, 4> c(71, 62, 13, 44);
    c.set(7, 6, 1, 4);

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 6);
    EXPECT_EQ(c.z, 1);
    EXPECT_EQ(c.w, 4);

    vec<std::uint32_t, 4> d(61, 22, 23, 92);
    d.set(6, 2, 2, 9);

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 2);
    EXPECT_EQ(d.z, 2);
    EXPECT_EQ(d.w, 9);
}

TEST(vec2, AssignmentOperator)
{
    vec<float, 2> a(10, 15);
    a = vec<float, 2>(5, 10);

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 10);

    vec<double, 2> b(62, 21);
    b = vec<double, 2>(6, 2);

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 2);

    vec<std::int32_t, 2> c(20, 11);
    c = vec<std::int32_t, 2>(2, 1);

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 1);

    vec<std::uint32_t, 2> d(93, 82);
    d = vec<std::uint32_t, 2>(9, 8);

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 8);
}

TEST(vec3, AssignmentOperator)
{
    vec<float, 3> a(22, 11, 54);
    a = vec<float, 3>(2, 1, 5);

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 1);
    EXPECT_EQ(a.z, 5);

    vec<double, 3> b(54, 83, 92);
    b = vec<double, 3>(5, 8, 9);

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 8);
    EXPECT_EQ(b.z, 9);

    vec<std::int32_t, 3> c(12, 21, 32);
    c = vec<std::int32_t, 3>(1, 2, 3);

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 2);
    EXPECT_EQ(c.z, 3);

    vec<std::uint32_t, 3> d(103, 52, 11);
    d = vec<std::uint32_t, 3>(10, 5, 1);

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 5);
    EXPECT_EQ(d.z, 1);
}

TEST(vec4, AssignmentOperator)
{
    vec<float, 4> a(62, 61, 53, 22);
    a = vec<float, 4>(6, 6, 5, 1);

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 5);
    EXPECT_EQ(a.w, 1);

    vec<double, 4> b(22, 11, 22, 12);
    b = vec<double, 4>(2, 6, 2, 1);

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 6);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 1);

    vec<std::int32_t, 4> c(72, 61, 12, 43);
    c = vec<std::int32_t, 4>(7, 6, 1, 4);

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 6);
    EXPECT_EQ(c.z, 1);
    EXPECT_EQ(c.w, 4);

    vec<std::uint32_t, 4> d(62, 21, 22, 93);
    d = vec<std::uint32_t, 4>(6, 2, 2, 9);

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 2);
    EXPECT_EQ(d.z, 2);
    EXPECT_EQ(d.w, 9);
}

TEST(vec2, MoveOperator)
{
    vec<float, 2> a(10, 15);
    a = std::move(vec<float, 2>(5, 10));

    EXPECT_EQ(a.x, 5);
    EXPECT_EQ(a.y, 10);

    vec<double, 2> b(62, 21);
    b = std::move(vec<double, 2>(6, 2));

    EXPECT_EQ(b.x, 6);
    EXPECT_EQ(b.y, 2);

    vec<std::int32_t, 2> c(20, 11);
    c = std::move(vec<std::int32_t, 2>(2, 1));

    EXPECT_EQ(c.x, 2);
    EXPECT_EQ(c.y, 1);

    vec<std::uint32_t, 2> d(93, 82);
    d = std::move(vec<std::uint32_t, 2>(9, 8));

    EXPECT_EQ(d.x, 9);
    EXPECT_EQ(d.y, 8);
}

TEST(vec3, MoveOperator)
{
    vec<float, 3> a(22, 11, 54);
    a = std::move(vec<float, 3>(2, 1, 5));

    EXPECT_EQ(a.x, 2);
    EXPECT_EQ(a.y, 1);
    EXPECT_EQ(a.z, 5);

    vec<double, 3> b(54, 83, 92);
    b = std::move(vec<double, 3>(5, 8, 9));

    EXPECT_EQ(b.x, 5);
    EXPECT_EQ(b.y, 8);
    EXPECT_EQ(b.z, 9);

    vec<std::int32_t, 3> c(12, 21, 32);
    c = std::move(vec<std::int32_t, 3>(1, 2, 3));

    EXPECT_EQ(c.x, 1);
    EXPECT_EQ(c.y, 2);
    EXPECT_EQ(c.z, 3);

    vec<std::uint32_t, 3> d(103, 52, 11);
    d = std::move(vec<std::uint32_t, 3>(10, 5, 1));

    EXPECT_EQ(d.x, 10);
    EXPECT_EQ(d.y, 5);
    EXPECT_EQ(d.z, 1);
}

TEST(vec4, MoveOperator)
{
    vec<float, 4> a(62, 61, 53, 22);
    a = std::move(vec<float, 4>(6, 6, 5, 1));

    EXPECT_EQ(a.x, 6);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 5);
    EXPECT_EQ(a.w, 1);

    vec<double, 4> b(22, 11, 22, 12);
    b = std::move(vec<double, 4>(2, 6, 2, 1));

    EXPECT_EQ(b.x, 2);
    EXPECT_EQ(b.y, 6);
    EXPECT_EQ(b.z, 2);
    EXPECT_EQ(b.w, 1);

    vec<std::int32_t, 4> c(72, 61, 12, 43);
    c = std::move(vec<std::int32_t, 4>(7, 6, 1, 4));

    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 6);
    EXPECT_EQ(c.z, 1);
    EXPECT_EQ(c.w, 4);

    vec<std::uint32_t, 4> d(62, 21, 22, 93);
    d = std::move(vec<std::uint32_t, 4>(6, 2, 2, 9));

    EXPECT_EQ(d.x, 6);
    EXPECT_EQ(d.y, 2);
    EXPECT_EQ(d.z, 2);
    EXPECT_EQ(d.w, 9);
}

TEST(vec2, IndexOperator)
{
    vec<float, 2> a(5, 10);

    EXPECT_EQ(a[0], 5);
    EXPECT_EQ(a[1], 10);

    vec<double, 2> b(6, 2);

    EXPECT_EQ(b[0], 6);
    EXPECT_EQ(b[1], 2);

    vec<std::int32_t, 2> c(2, 1);

    EXPECT_EQ(c[0], 2);
    EXPECT_EQ(c[1], 1);

    vec<std::uint32_t, 2> d(9, 8);

    EXPECT_EQ(d[0], 9);
    EXPECT_EQ(d[1], 8);
}

TEST(vec3, IndexOperator)
{
    vec<float, 3> a(2, 1, 5);

    EXPECT_EQ(a[0], 2);
    EXPECT_EQ(a[1], 1);
    EXPECT_EQ(a[2], 5);

    vec<double, 3> b(5, 8, 9);

    EXPECT_EQ(b[0], 5);
    EXPECT_EQ(b[1], 8);
    EXPECT_EQ(b[2], 9);

    vec<std::int32_t, 3> c(1, 2, 3);

    EXPECT_EQ(c[0], 1);
    EXPECT_EQ(c[1], 2);
    EXPECT_EQ(c[2], 3);

    vec<std::uint32_t, 3> d(10, 5, 1);

    EXPECT_EQ(d[0], 10);
    EXPECT_EQ(d[1], 5);
    EXPECT_EQ(d[2], 1);
}

TEST(vec4, IndexOperator)
{
    vec<float, 4> a(6, 6, 5, 2);

    EXPECT_EQ(a[0], 6);
    EXPECT_EQ(a[1], 6);
    EXPECT_EQ(a[2], 5);
    EXPECT_EQ(a[3], 2);

    vec<double, 4> b(2, 6, 2, 1);

    EXPECT_EQ(b[0], 2);
    EXPECT_EQ(b[1], 6);
    EXPECT_EQ(b[2], 2);
    EXPECT_EQ(b[3], 1);

    vec<std::int32_t, 4> c(7, 6, 1, 4);

    EXPECT_EQ(c[0], 7);
    EXPECT_EQ(c[1], 6);
    EXPECT_EQ(c[2], 1);
    EXPECT_EQ(c[3], 4);

    vec<std::uint32_t, 4> d(6, 2, 2, 9);

    EXPECT_EQ(d[0], 6);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 2);
    EXPECT_EQ(d[3], 9);
}

TEST(vec2, EqualsNotEqualsOperator)
{
    vec<float, 2> a(5, 10);
    vec<float, 2> a1(5, 10);

    EXPECT_TRUE(a == a1);
    EXPECT_FALSE(a != a1);

    vec<double, 2> b(6, 2);
    vec<double, 2> b1(6, 2);

    EXPECT_TRUE(b == b1);
    EXPECT_FALSE(b != b1);

    vec<std::int32_t, 2> c(2, 1);
    vec<std::int32_t, 2> c1(2, 1);

    EXPECT_TRUE(c == c1);
    EXPECT_FALSE(c != c1);

    vec<std::uint32_t, 2> d(9, 8);
    vec<std::uint32_t, 2> d1(9, 8);

    EXPECT_TRUE(d == d1);
    EXPECT_FALSE(d != d1);
}

TEST(vec3, EqualsNotEqualsOperator)
{
    vec<float, 3> a(2, 1, 5);
    vec<float, 3> a1(2, 1, 5);

    EXPECT_TRUE(a == a1);
    EXPECT_FALSE(a != a1);

    vec<double, 3> b(5, 8, 9);
    vec<double, 3> b1(5, 8, 9);

    EXPECT_TRUE(b == b1);
    EXPECT_FALSE(b != b1);

    vec<std::int32_t, 3> c(1, 2, 3);
    vec<std::int32_t, 3> c1(1, 2, 3);

    EXPECT_TRUE(c == c1);
    EXPECT_FALSE(c != c1);

    vec<std::uint32_t, 3> d(10, 5, 1);
    vec<std::uint32_t, 3> d1(10, 5, 1);

    EXPECT_TRUE(d == d1);
    EXPECT_FALSE(d != d1);
}

TEST(vec4, EqualsNotEqualsOperator)
{
    vec<float, 4> a(6, 6, 5, 2);
    vec<float, 4> a1(6, 6, 5, 2);

    EXPECT_TRUE(a == a1);
    EXPECT_FALSE(a != a1);

    vec<double, 4> b(2, 6, 2, 1);
    vec<double, 4> b1(2, 6, 2, 1);

    EXPECT_TRUE(b == b1);
    EXPECT_FALSE(b != b1);

    vec<std::int32_t, 4> c(7, 6, 1, 4);
    vec<std::int32_t, 4> c1(7, 6, 1, 4);

    EXPECT_TRUE(c == c1);
    EXPECT_FALSE(c != c1);

    vec<std::uint32_t, 4> d(6, 2, 2, 9);
    vec<std::uint32_t, 4> d1(6, 2, 2, 9);

    EXPECT_TRUE(d == d1);
    EXPECT_FALSE(d != d1);
}

TEST(vec2, ConstExprEqualsNotEqualsOperator)
{
    constexpr vec<float, 2> a(5, 10);
    constexpr vec<float, 2> a1(5, 10);

    EXPECT_TRUE(a == a1);
    EXPECT_FALSE(a != a1);

    constexpr vec<double, 2> b(6, 2);
    constexpr vec<double, 2> b1(6, 2);

    EXPECT_TRUE(b == b1);
    EXPECT_FALSE(b != b1);

    constexpr vec<std::int32_t, 2> c(2, 1);
    constexpr vec<std::int32_t, 2> c1(2, 1);

    EXPECT_TRUE(c == c1);
    EXPECT_FALSE(c != c1);

    constexpr vec<std::uint32_t, 2> d(9, 8);
    constexpr vec<std::uint32_t, 2> d1(9, 8);

    EXPECT_TRUE(d == d1);
    EXPECT_FALSE(d != d1);
}

TEST(vec3, ConstExprEqualsNotEqualsOperator)
{
    constexpr vec<float, 3> a(2, 1, 5);
    constexpr vec<float, 3> a1(2, 1, 5);

    EXPECT_TRUE(a == a1);
    EXPECT_FALSE(a != a1);

    constexpr vec<double, 3> b(5, 8, 9);
    constexpr vec<double, 3> b1(5, 8, 9);

    EXPECT_TRUE(b == b1);
    EXPECT_FALSE(b != b1);

    constexpr vec<std::int32_t, 3> c(1, 2, 3);
    constexpr vec<std::int32_t, 3> c1(1, 2, 3);

    EXPECT_TRUE(c == c1);
    EXPECT_FALSE(c != c1);

    constexpr vec<std::uint32_t, 3> d(10, 5, 1);
    constexpr vec<std::uint32_t, 3> d1(10, 5, 1);

    EXPECT_TRUE(d == d1);
    EXPECT_FALSE(d != d1);
}

TEST(vec4, ConstExprEqualsNotEqualsOperator)
{
    constexpr vec<float, 4> a(6, 6, 5, 2);
    constexpr vec<float, 4> a1(6, 6, 5, 2);

    EXPECT_TRUE(a == a1);
    EXPECT_FALSE(a != a1);

    constexpr vec<double, 4> b(2, 6, 2, 1);
    constexpr vec<double, 4> b1(2, 6, 2, 1);

    EXPECT_TRUE(b == b1);
    EXPECT_FALSE(b != b1);

    constexpr vec<std::int32_t, 4> c(7, 6, 1, 4);
    constexpr vec<std::int32_t, 4> c1(7, 6, 1, 4);

    EXPECT_TRUE(c == c1);
    EXPECT_FALSE(c != c1);

    constexpr vec<std::uint32_t, 4> d(6, 2, 2, 9);
    constexpr vec<std::uint32_t, 4> d1(6, 2, 2, 9);

    EXPECT_TRUE(d == d1);
    EXPECT_FALSE(d != d1);
}

TEST(vec2, PlusOperator)
{
    vec<float, 2> a(5, 10);
    vec<float, 2> a1(5, 10);

    vec<float, 2> a2 = a + a1;

    EXPECT_EQ(a2.x, 10);
    EXPECT_EQ(a2.y, 20);

    vec<double, 2> b(6, 2);
    vec<double, 2> b1(6, 2);

    vec<double, 2> b2 = b + b1;

    EXPECT_EQ(b2.x, 12);
    EXPECT_EQ(b2.y, 4);

    vec<std::int32_t, 2> c(2, 1);
    vec<std::int32_t, 2> c1(2, 1);

    vec<std::int32_t, 2> c2 = c + c1;

    EXPECT_EQ(c2.x, 4);
    EXPECT_EQ(c2.y, 2);

    vec<std::uint32_t, 2> d(9, 8);
    vec<std::uint32_t, 2> d1(9, 8);

    vec<std::uint32_t, 2> d2 = d + d1;

    EXPECT_EQ(d2.x, 18);
    EXPECT_EQ(d2.y, 16);
}

TEST(vec3, PlusOperator)
{
    vec<float, 3> a(5, 10, 6);
    vec<float, 3> a1(5, 10, 1);

    vec<float, 3> a2 = a + a1;

    EXPECT_EQ(a2.x, 10);
    EXPECT_EQ(a2.y, 20);
    EXPECT_EQ(a2.z, 7);

    vec<double, 3> b(6, 2, 4);
    vec<double, 3> b1(6, 2, 1);

    vec<double, 3> b2 = b + b1;

    EXPECT_EQ(b2.x, 12);
    EXPECT_EQ(a2.y, 20);
    EXPECT_EQ(b2.z, 5);

    vec<std::int32_t, 3> c(2, 1, 6);
    vec<std::int32_t, 3> c1(2, 1, 1);

    vec<std::int32_t, 3> c2 = c + c1;

    EXPECT_EQ(c2.x, 4);
    EXPECT_EQ(c2.y, 2);
    EXPECT_EQ(c2.z, 7);

    vec<std::uint32_t, 3> d(9, 8, 3);
    vec<std::uint32_t, 3> d1(9, 8, 4);

    vec<std::uint32_t, 3> d2 = d + d1;

    EXPECT_EQ(d2.x, 18);
    EXPECT_EQ(d2.y, 16);
    EXPECT_EQ(d2.z, 7);
}

TEST(vec4, PlusOperator)
{
    vec<float, 4> a(5, 10, 6, 2);
    vec<float, 4> a1(5, 10, 1, 9);

    vec<float, 4> a2 = a + a1;

    EXPECT_EQ(a2.x, 10);
    EXPECT_EQ(a2.y, 20);
    EXPECT_EQ(a2.z, 7);
    EXPECT_EQ(a2.w, 11);

    vec<double, 4> b(6, 2, 4, 5);
    vec<double, 4> b1(6, 2, 1, 1);

    vec<double, 4> b2 = b + b1;

    EXPECT_EQ(b2.x, 12);
    EXPECT_EQ(a2.y, 20);
    EXPECT_EQ(b2.z, 5);
    EXPECT_EQ(b2.w, 6);

    vec<std::int32_t, 4> c(2, 1, 6, 10);
    vec<std::int32_t, 4> c1(2, 1, 1, 9);

    vec<std::int32_t, 4> c2 = c + c1;

    EXPECT_EQ(c2.x, 4);
    EXPECT_EQ(c2.y, 2);
    EXPECT_EQ(c2.z, 7);
    EXPECT_EQ(c2.w, 19);

    vec<std::uint32_t, 4> d(9, 8, 3, 2);
    vec<std::uint32_t, 4> d1(9, 8, 4, 8);

    vec<std::uint32_t, 4> d2 = d + d1;

    EXPECT_EQ(d2.x, 18);
    EXPECT_EQ(d2.y, 16);
    EXPECT_EQ(d2.z, 7);
    EXPECT_EQ(d2.w, 10);
}

TEST(vec2, ConstExprPlusOperator)
{
    constexpr vec<float, 2> a(5, 10);
    constexpr vec<float, 2> a1(5, 10);

    constexpr vec<float, 2> a2 = a + a1;

    EXPECT_EQ(a2.x, 10);
    EXPECT_EQ(a2.y, 20);

    constexpr vec<double, 2> b(6, 2);
    constexpr vec<double, 2> b1(6, 2);

    constexpr vec<double, 2> b2 = b + b1;

    EXPECT_EQ(b2.x, 12);
    EXPECT_EQ(b2.y, 4);

    constexpr vec<std::int32_t, 2> c(2, 1);
    constexpr vec<std::int32_t, 2> c1(2, 1);

    constexpr vec<std::int32_t, 2> c2 = c + c1;

    EXPECT_EQ(c2.x, 4);
    EXPECT_EQ(c2.y, 2);

    constexpr vec<std::uint32_t, 2> d(9, 8);
    constexpr vec<std::uint32_t, 2> d1(9, 8);

    constexpr vec<std::uint32_t, 2> d2 = d + d1;

    EXPECT_EQ(d2.x, 18);
    EXPECT_EQ(d2.y, 16);
}

TEST(vec3, ConstExprPlusOperator)
{
    constexpr vec<float, 3> a(5, 10, 6);
    constexpr vec<float, 3> a1(5, 10, 1);

    constexpr vec<float, 3> a2 = a + a1;

    EXPECT_EQ(a2.x, 10);
    EXPECT_EQ(a2.y, 20);
    EXPECT_EQ(a2.z, 7);

    constexpr vec<double, 3> b(6, 2, 4);
    constexpr vec<double, 3> b1(6, 2, 1);

    constexpr vec<double, 3> b2 = b + b1;

    EXPECT_EQ(b2.x, 12);
    EXPECT_EQ(a2.y, 20);
    EXPECT_EQ(b2.z, 5);

    constexpr vec<std::int32_t, 3> c(2, 1, 6);
    constexpr vec<std::int32_t, 3> c1(2, 1, 1);

    constexpr vec<std::int32_t, 3> c2 = c + c1;

    EXPECT_EQ(c2.x, 4);
    EXPECT_EQ(c2.y, 2);
    EXPECT_EQ(c2.z, 7);

    constexpr vec<std::uint32_t, 3> d(9, 8, 3);
    constexpr vec<std::uint32_t, 3> d1(9, 8, 4);

    constexpr vec<std::uint32_t, 3> d2 = d + d1;

    EXPECT_EQ(d2.x, 18);
    EXPECT_EQ(d2.y, 16);
    EXPECT_EQ(d2.z, 7);
}

TEST(vec4, ConstExprPlusOperator)
{
    constexpr vec<float, 4> a(5, 10, 6, 2);
    constexpr vec<float, 4> a1(5, 10, 1, 9);

    constexpr vec<float, 4> a2 = a + a1;

    EXPECT_EQ(a2.x, 10);
    EXPECT_EQ(a2.y, 20);
    EXPECT_EQ(a2.z, 7);
    EXPECT_EQ(a2.w, 11);

    constexpr vec<double, 4> b(6, 2, 4, 5);
    constexpr vec<double, 4> b1(6, 2, 1, 1);

    constexpr vec<double, 4> b2 = b + b1;

    EXPECT_EQ(b2.x, 12);
    EXPECT_EQ(a2.y, 20);
    EXPECT_EQ(b2.z, 5);
    EXPECT_EQ(b2.w, 6);

    constexpr vec<std::int32_t, 4> c(2, 1, 6, 10);
    constexpr vec<std::int32_t, 4> c1(2, 1, 1, 9);

    constexpr vec<std::int32_t, 4> c2 = (c + c1);

    EXPECT_EQ(c2.x, 4);
    EXPECT_EQ(c2.y, 2);
    EXPECT_EQ(c2.z, 7);
    EXPECT_EQ(c2.w, 19);

    constexpr vec<std::uint32_t, 4> d(9, 8, 3, 2);
    constexpr vec<std::uint32_t, 4> d1(9, 8, 4, 8);

    constexpr vec<std::uint32_t, 4> d2 = d + d1;

    EXPECT_EQ(d2.x, 18);
    EXPECT_EQ(d2.y, 16);
    EXPECT_EQ(d2.z, 7);
    EXPECT_EQ(d2.w, 10);
}

TEST(vec2, MinusOperator)
{
    vec<float, 2> a(5, 10);
    vec<float, 2> a1(5, 10);

    vec<float, 2> a2 = a - a1;

    EXPECT_EQ(a2.x, 0);
    EXPECT_EQ(a2.y, 0);

    vec<double, 2> b(6, 2);
    vec<double, 2> b1(6, 2);

    vec<double, 2> b2 = b - b1;

    EXPECT_EQ(b2.x, 0);
    EXPECT_EQ(b2.y, 0);

    vec<std::int32_t, 2> c(2, 1);
    vec<std::int32_t, 2> c1(2, 1);

    vec<std::int32_t, 2> c2 = c - c1;

    EXPECT_EQ(c2.x, 0);
    EXPECT_EQ(c2.y, 0);

    vec<std::uint32_t, 2> d(9, 8);
    vec<std::uint32_t, 2> d1(9, 8);

    vec<std::uint32_t, 2> d2 = d - d1;

    EXPECT_EQ(d2.x, 0);
    EXPECT_EQ(d2.y, 0);
}

TEST(vec3, MinusOperator)
{
    vec<float, 3> a(5, 10, 6);
    vec<float, 3> a1(5, 10, 1);

    vec<float, 3> a2 = a - a1;

    EXPECT_EQ(a2.x, 0);
    EXPECT_EQ(a2.y, 0);
    EXPECT_EQ(a2.z, 5);

    vec<double, 3> b(6, 2, 4);
    vec<double, 3> b1(6, 2, 1);

    vec<double, 3> b2 = b - b1;

    EXPECT_EQ(b2.x, 0);
    EXPECT_EQ(a2.y, 0);
    EXPECT_EQ(b2.z, 3);

    vec<std::int32_t, 3> c(2, 1, 6);
    vec<std::int32_t, 3> c1(2, 1, 1);

    vec<std::int32_t, 3> c2 = c - c1;

    EXPECT_EQ(c2.x, 0);
    EXPECT_EQ(c2.y, 0);
    EXPECT_EQ(c2.z, 5);

    vec<std::uint32_t, 3> d(9, 8, 4);
    vec<std::uint32_t, 3> d1(9, 8, 3);

    vec<std::uint32_t, 3> d2 = d - d1;

    EXPECT_EQ(d2.x, 0);
    EXPECT_EQ(d2.y, 0);
    EXPECT_EQ(d2.z, 1);
}

TEST(vec4, MinusOperator)
{
    vec<float, 4> a(5, 10, 6, 2);
    vec<float, 4> a1(5, 10, 1, 9);

    vec<float, 4> a2 = a - a1;

    EXPECT_EQ(a2.x, 0);
    EXPECT_EQ(a2.y, 0);
    EXPECT_EQ(a2.z, 5);
    EXPECT_EQ(a2.w, -7);

    vec<double, 4> b(6, 2, 4, 5);
    vec<double, 4> b1(6, 2, 1, 1);

    vec<double, 4> b2 = b - b1;

    EXPECT_EQ(b2.x, 0);
    EXPECT_EQ(a2.y, 0);
    EXPECT_EQ(b2.z, 3);
    EXPECT_EQ(b2.w, 4);

    vec<std::int32_t, 4> c(2, 1, 6, 10);
    vec<std::int32_t, 4> c1(2, 1, 1, 9);

    vec<std::int32_t, 4> c2 = c - c1;

    EXPECT_EQ(c2.x, 0);
    EXPECT_EQ(c2.y, 0);
    EXPECT_EQ(c2.z, 5);
    EXPECT_EQ(c2.w, 1);

    vec<std::uint32_t, 4> d(9, 8, 4, 8);
    vec<std::uint32_t, 4> d1(9, 8, 3, 2);

    vec<std::uint32_t, 4> d2 = d - d1;

    EXPECT_EQ(d2.x, 0);
    EXPECT_EQ(d2.y, 0);
    EXPECT_EQ(d2.z, 1);
    EXPECT_EQ(d2.w, 6);
}

TEST(vec2, ConstExprMinusOperator)
{
    constexpr vec<float, 2> a(5, 10);
    constexpr vec<float, 2> a1(5, 10);

    constexpr vec<float, 2> a2 = a - a1;

    EXPECT_EQ(a2.x, 0);
    EXPECT_EQ(a2.y, 0);

    constexpr vec<double, 2> b(6, 2);
    constexpr vec<double, 2> b1(6, 2);

    constexpr vec<double, 2> b2 = b - b1;

    EXPECT_EQ(b2.x, 0);
    EXPECT_EQ(b2.y, 0);

    constexpr vec<std::int32_t, 2> c(2, 1);
    constexpr vec<std::int32_t, 2> c1(2, 1);

    constexpr vec<std::int32_t, 2> c2 = c - c1;

    EXPECT_EQ(c2.x, 0);
    EXPECT_EQ(c2.y, 0);

    constexpr vec<std::uint32_t, 2> d(9, 8);
    constexpr vec<std::uint32_t, 2> d1(9, 8);

    constexpr vec<std::uint32_t, 2> d2 = d - d1;

    EXPECT_EQ(d2.x, 0);
    EXPECT_EQ(d2.y, 0);
}

TEST(vec3, ConstExprMinusOperator)
{
    constexpr vec<float, 3> a(5, 10, 6);
    constexpr vec<float, 3> a1(5, 10, 1);

    constexpr vec<float, 3> a2 = a - a1;

    EXPECT_EQ(a2.x, 0);
    EXPECT_EQ(a2.y, 0);
    EXPECT_EQ(a2.z, 5);

    constexpr vec<double, 3> b(6, 2, 4);
    constexpr vec<double, 3> b1(6, 2, 1);

    constexpr vec<double, 3> b2 = b - b1;

    EXPECT_EQ(b2.x, 0);
    EXPECT_EQ(a2.y, 0);
    EXPECT_EQ(b2.z, 3);

    constexpr vec<std::int32_t, 3> c(2, 1, 6);
    constexpr vec<std::int32_t, 3> c1(2, 1, 1);

    constexpr vec<std::int32_t, 3> c2 = c - c1;

    EXPECT_EQ(c2.x, 0);
    EXPECT_EQ(c2.y, 0);
    EXPECT_EQ(c2.z, 5);

    constexpr vec<std::uint32_t, 3> d(9, 8, 4);
    constexpr vec<std::uint32_t, 3> d1(9, 8, 3);

    constexpr vec<std::uint32_t, 3> d2 = d - d1;

    EXPECT_EQ(d2.x, 0);
    EXPECT_EQ(d2.y, 0);
    EXPECT_EQ(d2.z, 1);
}

TEST(vec4, ConstExprMinusOperator)
{
    constexpr vec<float, 4> a(5, 10, 6, 2);
    constexpr vec<float, 4> a1(5, 10, 1, 9);

    constexpr vec<float, 4> a2 = a - a1;

    EXPECT_EQ(a2.x, 0);
    EXPECT_EQ(a2.y, 0);
    EXPECT_EQ(a2.z, 5);
    EXPECT_EQ(a2.w, -7);

    constexpr vec<double, 4> b(6, 2, 4, 5);
    constexpr vec<double, 4> b1(6, 2, 1, 1);

    constexpr vec<double, 4> b2 = b - b1;

    EXPECT_EQ(b2.x, 0);
    EXPECT_EQ(a2.y, 0);
    EXPECT_EQ(b2.z, 3);
    EXPECT_EQ(b2.w, 4);

    constexpr vec<std::int32_t, 4> c(2, 1, 6, 10);
    constexpr vec<std::int32_t, 4> c1(2, 1, 1, 9);

    constexpr vec<std::int32_t, 4> c2 = (c - c1);

    EXPECT_EQ(c2.x, 0);
    EXPECT_EQ(c2.y, 0);
    EXPECT_EQ(c2.z, 5);
    EXPECT_EQ(c2.w, 1);

    constexpr vec<std::uint32_t, 4> d(9, 8, 4, 8);
    constexpr vec<std::uint32_t, 4> d1(9, 8, 3, 2);

    constexpr vec<std::uint32_t, 4> d2 = d - d1;

    EXPECT_EQ(d2.x, 0);
    EXPECT_EQ(d2.y, 0);
    EXPECT_EQ(d2.z, 1);
    EXPECT_EQ(d2.w, 6);
}

TEST(vec2, DivOperator)
{
    vec<float, 2> a(5, 10);
    vec<float, 2> a1(5, 10);

    vec<float, 2> a2 = a / a1;

    EXPECT_EQ(a2.x, 1);
    EXPECT_EQ(a2.y, 1);

    vec<double, 2> b(6, 2);
    vec<double, 2> b1(6, 2);

    vec<double, 2> b2 = b / b1;

    EXPECT_EQ(b2.x, 1);
    EXPECT_EQ(b2.y, 1);

    vec<std::int32_t, 2> c(2, 1);
    vec<std::int32_t, 2> c1(2, 1);

    vec<std::int32_t, 2> c2 = c / c1;

    EXPECT_EQ(c2.x, 1);
    EXPECT_EQ(c2.y, 1);

    vec<std::uint32_t, 2> d(9, 8);
    vec<std::uint32_t, 2> d1(9, 8);

    vec<std::uint32_t, 2> d2 = d / d1;

    EXPECT_EQ(d2.x, 1);
    EXPECT_EQ(d2.y, 1);
}

TEST(vec3, DivOperator)
{
    vec<float, 3> a(5, 10, 12);
    vec<float, 3> a1(5, 10, 6);

    vec<float, 3> a2 = a / a1;

    EXPECT_EQ(a2.x, 1);
    EXPECT_EQ(a2.y, 1);
    EXPECT_EQ(a2.z, 2);

    vec<double, 3> b(6, 2, 4);
    vec<double, 3> b1(6, 2, 1);

    vec<double, 3> b2 = b / b1;

    EXPECT_EQ(b2.x, 1);
    EXPECT_EQ(a2.y, 1);
    EXPECT_EQ(b2.z, 4);

    vec<std::int32_t, 3> c(2, 1, 6);
    vec<std::int32_t, 3> c1(2, 1, 1);

    vec<std::int32_t, 3> c2 = c / c1;

    EXPECT_EQ(c2.x, 1);
    EXPECT_EQ(c2.y, 1);
    EXPECT_EQ(c2.z, 6);

    vec<std::uint32_t, 3> d(9, 8, 4);
    vec<std::uint32_t, 3> d1(9, 8, 2);

    vec<std::uint32_t, 3> d2 = d / d1;

    EXPECT_EQ(d2.x, 1);
    EXPECT_EQ(d2.y, 1);
    EXPECT_EQ(d2.z, 2);
}

TEST(vec4, DivOperator)
{
    vec<float, 4> a(5, 10, 6, 8);
    vec<float, 4> a1(5, 10, 1, 2);

    vec<float, 4> a2 = a / a1;

    EXPECT_EQ(a2.x, 1);
    EXPECT_EQ(a2.y, 1);
    EXPECT_EQ(a2.z, 6);
    EXPECT_EQ(a2.w, 4);

    vec<double, 4> b(6, 2, 4, 5);
    vec<double, 4> b1(6, 2, 1, 1);

    vec<double, 4> b2 = b / b1;

    EXPECT_EQ(b2.x, 1);
    EXPECT_EQ(a2.y, 1);
    EXPECT_EQ(b2.z, 4);
    EXPECT_EQ(b2.w, 5);

    vec<std::int32_t, 4> c(2, 1, 6, 10);
    vec<std::int32_t, 4> c1(2, 1, 1, 2);

    vec<std::int32_t, 4> c2 = c / c1;

    EXPECT_EQ(c2.x, 1);
    EXPECT_EQ(c2.y, 1);
    EXPECT_EQ(c2.z, 6);
    EXPECT_EQ(c2.w, 5);

    vec<std::uint32_t, 4> d(9, 8, 4, 8);
    vec<std::uint32_t, 4> d1(9, 8, 1, 2);

    vec<std::uint32_t, 4> d2 = d / d1;

    EXPECT_EQ(d2.x, 1);
    EXPECT_EQ(d2.y, 1);
    EXPECT_EQ(d2.z, 4);
    EXPECT_EQ(d2.w, 4);
}

TEST(vec2, ConstExprDivOperator)
{
    constexpr vec<float, 2> a(5, 10);
    constexpr vec<float, 2> a1(5, 10);

    constexpr vec<float, 2> a2 = a / a1;

    EXPECT_EQ(a2.x, 1);
    EXPECT_EQ(a2.y, 1);

    constexpr vec<double, 2> b(6, 2);
    constexpr vec<double, 2> b1(6, 2);

    constexpr vec<double, 2> b2 = b / b1;

    EXPECT_EQ(b2.x, 1);
    EXPECT_EQ(b2.y, 1);

    constexpr vec<std::int32_t, 2> c(2, 1);
    constexpr vec<std::int32_t, 2> c1(2, 1);

    constexpr vec<std::int32_t, 2> c2 = c / c1;

    EXPECT_EQ(c2.x, 1);
    EXPECT_EQ(c2.y, 1);

    constexpr vec<std::uint32_t, 2> d(9, 8);
    constexpr vec<std::uint32_t, 2> d1(9, 8);

    constexpr vec<std::uint32_t, 2> d2 = d / d1;

    EXPECT_EQ(d2.x, 1);
    EXPECT_EQ(d2.y, 1);
}

TEST(vec3, ConstExprDivOperator)
{
    constexpr vec<float, 3> a(5, 10, 6);
    constexpr vec<float, 3> a1(5, 10, 1);

    constexpr vec<float, 3> a2 = a / a1;

    EXPECT_EQ(a2.x, 1);
    EXPECT_EQ(a2.y, 1);
    EXPECT_EQ(a2.z, 6);

    constexpr vec<double, 3> b(6, 2, 4);
    constexpr vec<double, 3> b1(6, 2, 1);

    constexpr vec<double, 3> b2 = b / b1;

    EXPECT_EQ(b2.x, 1);
    EXPECT_EQ(a2.y, 1);
    EXPECT_EQ(b2.z, 4);

    constexpr vec<std::int32_t, 3> c(2, 1, 6);
    constexpr vec<std::int32_t, 3> c1(2, 1, 1);

    constexpr vec<std::int32_t, 3> c2 = c / c1;

    EXPECT_EQ(c2.x, 1);
    EXPECT_EQ(c2.y, 1);
    EXPECT_EQ(c2.z, 6);

    constexpr vec<std::uint32_t, 3> d(9, 8, 4);
    constexpr vec<std::uint32_t, 3> d1(9, 8, 2);

    constexpr vec<std::uint32_t, 3> d2 = d / d1;

    EXPECT_EQ(d2.x, 1);
    EXPECT_EQ(d2.y, 1);
    EXPECT_EQ(d2.z, 2);
}

TEST(vec4, ConstExprDivOperator)
{
    constexpr vec<float, 4> a(5, 10, 6, 8);
    constexpr vec<float, 4> a1(5, 10, 1, 4);

    constexpr vec<float, 4> a2 = a / a1;

    EXPECT_EQ(a2.x, 1);
    EXPECT_EQ(a2.y, 1);
    EXPECT_EQ(a2.z, 6);
    EXPECT_EQ(a2.w, 2);

    constexpr vec<double, 4> b(6, 2, 4, 5);
    constexpr vec<double, 4> b1(6, 2, 1, 1);

    constexpr vec<double, 4> b2 = b / b1;

    EXPECT_EQ(b2.x, 1);
    EXPECT_EQ(a2.y, 1);
    EXPECT_EQ(b2.z, 4);
    EXPECT_EQ(b2.w, 5);

    constexpr vec<std::int32_t, 4> c(2, 1, 6, 10);
    constexpr vec<std::int32_t, 4> c1(2, 1, 1, 2);

    constexpr vec<std::int32_t, 4> c2 = (c / c1);

    EXPECT_EQ(c2.x, 1);
    EXPECT_EQ(c2.y, 1);
    EXPECT_EQ(c2.z, 6);
    EXPECT_EQ(c2.w, 5);

    constexpr vec<std::uint32_t, 4> d(9, 8, 4, 8);
    constexpr vec<std::uint32_t, 4> d1(9, 8, 1, 2);

    constexpr vec<std::uint32_t, 4> d2 = d / d1;

    EXPECT_EQ(d2.x, 1);
    EXPECT_EQ(d2.y, 1);
    EXPECT_EQ(d2.z, 4);
    EXPECT_EQ(d2.w, 4);
}