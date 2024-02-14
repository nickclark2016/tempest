#include <gtest/gtest.h>

#include <tempest/vec4.hpp>

using tempest::math::vec4;

TEST(Vec4F, DefaultConstructor)
{
    vec4<float> v{};

    // test values
    ASSERT_FLOAT_EQ(v.x, 0);
    ASSERT_FLOAT_EQ(v.y, 0);
    ASSERT_FLOAT_EQ(v.z, 0);
    ASSERT_FLOAT_EQ(v.w, 0);

    // test aliasing
    ASSERT_FLOAT_EQ(v.x, v.r);
    ASSERT_FLOAT_EQ(v.y, v.g);
    ASSERT_FLOAT_EQ(v.z, v.b);
    ASSERT_FLOAT_EQ(v.w, v.a);
    ASSERT_FLOAT_EQ(v.x, v.data[0]);
    ASSERT_FLOAT_EQ(v.y, v.data[1]);
    ASSERT_FLOAT_EQ(v.z, v.data[2]);
    ASSERT_FLOAT_EQ(v.w, v.data[3]);
}

TEST(Vec4F, ScalarConstructor)
{
    vec4 v{ 1.0f };

    // test values
    ASSERT_FLOAT_EQ(v.x, 1.0f);
    ASSERT_FLOAT_EQ(v.y, 1.0f);
    ASSERT_FLOAT_EQ(v.z, 1.0f);
    ASSERT_FLOAT_EQ(v.w, 1.0f);

    // test aliasing
    ASSERT_FLOAT_EQ(v.x, v.r);
    ASSERT_FLOAT_EQ(v.y, v.g);
    ASSERT_FLOAT_EQ(v.z, v.b);
    ASSERT_FLOAT_EQ(v.w, v.a);
    ASSERT_FLOAT_EQ(v.x, v.data[0]);
    ASSERT_FLOAT_EQ(v.y, v.data[1]);
    ASSERT_FLOAT_EQ(v.z, v.data[2]);
    ASSERT_FLOAT_EQ(v.w, v.data[3]);
}

TEST(Vec4F, ComponentConstructor)
{
    vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };

    // test values
    ASSERT_FLOAT_EQ(v.x, 1.0f);
    ASSERT_FLOAT_EQ(v.y, 2.0f);
    ASSERT_FLOAT_EQ(v.z, 3.0f);
    ASSERT_FLOAT_EQ(v.w, 4.0f);

    // test aliasing
    ASSERT_FLOAT_EQ(v.x, v.r);
    ASSERT_FLOAT_EQ(v.y, v.g);
    ASSERT_FLOAT_EQ(v.z, v.b);
    ASSERT_FLOAT_EQ(v.w, v.a);
    ASSERT_FLOAT_EQ(v.x, v.data[0]);
    ASSERT_FLOAT_EQ(v.y, v.data[1]);
    ASSERT_FLOAT_EQ(v.z, v.data[2]);
    ASSERT_FLOAT_EQ(v.w, v.data[3]);
}

TEST(Vec4F, CopyConstructor)
{
    vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    vec4 w{ v };

    ASSERT_FLOAT_EQ(v.x, w.x);
    ASSERT_FLOAT_EQ(v.y, w.y);
    ASSERT_FLOAT_EQ(v.z, w.z);
    ASSERT_FLOAT_EQ(v.w, w.w);
}

TEST(Vec4F, MoveConstructor)
{
    vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    vec4 w{ std::move(v) };

    ASSERT_FLOAT_EQ(v.x, 1.0f);
    ASSERT_FLOAT_EQ(v.y, 2.0f);
    ASSERT_FLOAT_EQ(v.z, 3.0f);
    ASSERT_FLOAT_EQ(v.w, 4.0f);
}

TEST(Vec4F, IndexOperator)
{
    vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };

    ASSERT_FLOAT_EQ(v[0], 1.0f);
    ASSERT_FLOAT_EQ(v[1], 2.0f);
    ASSERT_FLOAT_EQ(v[2], 3.0f);
    ASSERT_FLOAT_EQ(v[3], 4.0f);
}

TEST(Vec4F, ConstIndexOperator)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };

    ASSERT_FLOAT_EQ(v[0], 1.0f);
    ASSERT_FLOAT_EQ(v[1], 2.0f);
    ASSERT_FLOAT_EQ(v[2], 3.0f);
    ASSERT_FLOAT_EQ(v[3], 4.0f);
}

TEST(Vec4F, EqualityOperatorEquals)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 1.0f, 2.0f, 3.0f, 4.0f };

    ASSERT_TRUE(v == w);
    ASSERT_TRUE(w == v);
}

TEST(Vec4F, EqualityOperatorNotEqualsFirstComponent)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 2.0f, 2.0f, 3.0f, 4.0f };

    ASSERT_FALSE(v == w);
    ASSERT_FALSE(w == v);
}

TEST(Vec4F, EqualityOperatorNotEqualsThirdComponent)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 1.0f, 2.0f, 2.0f, 4.0f };

    ASSERT_FALSE(v == w);
    ASSERT_FALSE(w == v);
}

TEST(Vec4F, EqualityOperatorNotEqualsFourthComponent)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 1.0f, 2.0f, 3.0f, 3.0f };

    ASSERT_FALSE(v == w);
    ASSERT_FALSE(w == v);
}

TEST(Vec4F, InequalityOperatorEquals)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 1.0f, 2.0f, 3.0f, 4.0f };

    ASSERT_FALSE(v != w);
    ASSERT_FALSE(w != v);
}

TEST(Vec4F, InequalityOperatorNotEqualsFirstComponent)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 2.0f, 2.0f, 3.0f, 4.0f };

    ASSERT_TRUE(v != w);
    ASSERT_TRUE(w != v);
}

TEST(Vec4F, InequalityOperatorNotEqualsThirdComponent)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 1.0f, 2.0f, 2.0f, 4.0f };

    ASSERT_TRUE(v != w);
    ASSERT_TRUE(w != v);
}

TEST(Vec4F, InequalityOperatorNotEqualsFourthComponent)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 1.0f, 2.0f, 3.0f, 3.0f };

    ASSERT_TRUE(v != w);
    ASSERT_TRUE(w != v);
}

TEST(Vec4F, AdditionOperator)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 5.0f, 6.0f, 7.0f, 8.0f };
    const vec4 sum = v + w;
    const vec4 comSum = w + v;

    ASSERT_NEAR(sum[0], 6.0f, 0.0001f);
    ASSERT_NEAR(sum[1], 8.0f, 0.0001f);
    ASSERT_NEAR(sum[2], 10.0f, 0.0001f);
    ASSERT_NEAR(sum[3], 12.0f, 0.0001f);

    ASSERT_NEAR(comSum[0], 6.0f, 0.0001f);
    ASSERT_NEAR(comSum[1], 8.0f, 0.0001f);
    ASSERT_NEAR(comSum[2], 10.0f, 0.0001f);
    ASSERT_NEAR(comSum[3], 12.0f, 0.0001f);

    ASSERT_NEAR(comSum[0], sum[0], 0.0001f);
    ASSERT_NEAR(comSum[1], sum[1], 0.0001f);
    ASSERT_NEAR(comSum[2], sum[2], 0.0001f);
    ASSERT_NEAR(comSum[3], sum[3], 0.0001f);
}

TEST(Vec4F, SubtractionOperator)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 8.0f, 7.0f, 6.0f, 5.0f };
    const vec4 diff = v - w;

    ASSERT_NEAR(diff[0], -7.0f, 0.0001f);
    ASSERT_NEAR(diff[1], -5.0f, 0.0001f);
    ASSERT_NEAR(diff[2], -3.0f, 0.0001f);
    ASSERT_NEAR(diff[3], -1.0f, 0.0001f);
}

TEST(Vec4F, VectorComponentMultiply)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 8.0f, 7.0f, 6.0f, 5.0f };
    const vec4 product = v * w;

    ASSERT_NEAR(product[0], 8.0f, 0.0001f);
    ASSERT_NEAR(product[1], 14.0f, 0.0001f);
    ASSERT_NEAR(product[2], 18.0f, 0.0001f);
    ASSERT_NEAR(product[3], 20.0f, 0.0001f);
}

TEST(Vec4F, VectorScalarMultiply)
{
    const float v = 2.0f;
    const vec4 w{ 8.0f, 7.0f, 6.0f, 5.0f };
    const vec4 product = v * w;

    ASSERT_NEAR(product[0], 16.0f, 0.0001f);
    ASSERT_NEAR(product[1], 14.0f, 0.0001f);
    ASSERT_NEAR(product[2], 12.0f, 0.0001f);
    ASSERT_NEAR(product[3], 10.0f, 0.0001f);
}

TEST(Vec4F, VectorDivide)
{
    const vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 8.0f, 7.0f, 6.0f, 5.0f };
    const vec4 quotient = v / w;

    ASSERT_NEAR(quotient[0], 1.0f / 8.0f, 0.0001f);
    ASSERT_NEAR(quotient[1], 2.0f / 7.0f, 0.0001f);
    ASSERT_NEAR(quotient[2], 3.0f / 6.0f, 0.0001f);
    ASSERT_NEAR(quotient[3], 4.0f / 5.0f, 0.0001f);
}

TEST(Vec4F, VectorPlusEquals)
{
    vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 5.0f, 6.0f, 7.0f, 8.0f };
    v += w;

    ASSERT_NEAR(v[0], 6.0f, 0.0001f);
    ASSERT_NEAR(v[1], 8.0f, 0.0001f);
    ASSERT_NEAR(v[2], 10.0f, 0.0001f);
    ASSERT_NEAR(v[3], 12.0f, 0.0001f);
}

TEST(Vec4F, VectorMinusEquals)
{
    vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 8.0f, 7.0f, 6.0f, 5.0f };
    v -= w;

    ASSERT_NEAR(v[0], -7.0f, 0.0001f);
    ASSERT_NEAR(v[1], -5.0f, 0.0001f);
    ASSERT_NEAR(v[2], -3.0f, 0.0001f);
    ASSERT_NEAR(v[3], -1.0f, 0.0001f);
}

TEST(Vec4F, VectorMulEquals)
{
    vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 5.0f, 6.0f, 7.0f, 8.0f };
    v *= w;

    ASSERT_NEAR(v[0], 5.0f, 0.0001f);
    ASSERT_NEAR(v[1], 12.0f, 0.0001f);
    ASSERT_NEAR(v[2], 21.0f, 0.0001f);
    ASSERT_NEAR(v[3], 32.0f, 0.0001f);
}

TEST(Vec4F, VectorDivEquals)
{
    vec4 v{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vec4 w{ 8.0f, 7.0f, 6.0f, 5.0f };
    v /= w;

    ASSERT_NEAR(v[0], 1.0f / 8.0f, 0.0001f);
    ASSERT_NEAR(v[1], 2.0f / 7.0f, 0.0001f);
    ASSERT_NEAR(v[2], 3.0f / 6.0f, 0.0001f);
    ASSERT_NEAR(v[3], 4.0f / 5.0f, 0.0001f);
}