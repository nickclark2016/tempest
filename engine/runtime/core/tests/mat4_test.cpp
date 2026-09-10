#include <gtest/gtest.h>

#include <tempest/mat4.hpp>
#include <tempest/utility.hpp>
#include <tempest/vec4.hpp>

using tempest::math::mat4;
using tempest::math::vec4;

TEST(Mat4F, ZeroInit)
{
    mat4<float> m{};

    ASSERT_FLOAT_EQ(m[0][0], 0.0F);
    ASSERT_FLOAT_EQ(m[0][1], 0.0F);
    ASSERT_FLOAT_EQ(m[0][2], 0.0F);
    ASSERT_FLOAT_EQ(m[0][3], 0.0F);

    ASSERT_FLOAT_EQ(m[1][0], 0.0F);
    ASSERT_FLOAT_EQ(m[1][1], 0.0F);
    ASSERT_FLOAT_EQ(m[1][2], 0.0F);
    ASSERT_FLOAT_EQ(m[1][3], 0.0F);

    ASSERT_FLOAT_EQ(m[2][0], 0.0F);
    ASSERT_FLOAT_EQ(m[2][1], 0.0F);
    ASSERT_FLOAT_EQ(m[2][2], 0.0F);
    ASSERT_FLOAT_EQ(m[2][3], 0.0F);

    ASSERT_FLOAT_EQ(m[3][0], 0.0F);
    ASSERT_FLOAT_EQ(m[3][1], 0.0F);
    ASSERT_FLOAT_EQ(m[3][2], 0.0F);
    ASSERT_FLOAT_EQ(m[3][3], 0.0F);

    ASSERT_EQ(m[0], m.columns[0]);
    ASSERT_EQ(m[1], m.columns[1]);
    ASSERT_EQ(m[2], m.columns[2]);
    ASSERT_EQ(m[3], m.columns[3]);

    ASSERT_EQ(m[0][0], m.m00);
    ASSERT_EQ(m[0][1], m.m10);
    ASSERT_EQ(m[0][2], m.m20);
    ASSERT_EQ(m[0][3], m.m30);
    ASSERT_EQ(m[1][0], m.m01);
    ASSERT_EQ(m[1][1], m.m11);
    ASSERT_EQ(m[1][2], m.m21);
    ASSERT_EQ(m[1][3], m.m31);
    ASSERT_EQ(m[2][0], m.m02);
    ASSERT_EQ(m[2][1], m.m12);
    ASSERT_EQ(m[2][2], m.m22);
    ASSERT_EQ(m[2][3], m.m32);
    ASSERT_EQ(m[3][0], m.m03);
    ASSERT_EQ(m[3][1], m.m13);
    ASSERT_EQ(m[3][2], m.m23);
    ASSERT_EQ(m[3][3], m.m33);
}

TEST(Mat4F, DiagonalConstructor)
{
    mat4 m(1.0F);

    ASSERT_FLOAT_EQ(m[0][0], 1.0F);
    ASSERT_FLOAT_EQ(m[0][1], 0.0F);
    ASSERT_FLOAT_EQ(m[0][2], 0.0F);
    ASSERT_FLOAT_EQ(m[0][3], 0.0F);

    ASSERT_FLOAT_EQ(m[1][0], 0.0F);
    ASSERT_FLOAT_EQ(m[1][1], 1.0F);
    ASSERT_FLOAT_EQ(m[1][2], 0.0F);
    ASSERT_FLOAT_EQ(m[1][3], 0.0F);

    ASSERT_FLOAT_EQ(m[2][0], 0.0F);
    ASSERT_FLOAT_EQ(m[2][1], 0.0F);
    ASSERT_FLOAT_EQ(m[2][2], 1.0F);
    ASSERT_FLOAT_EQ(m[2][3], 0.0F);

    ASSERT_FLOAT_EQ(m[3][0], 0.0F);
    ASSERT_FLOAT_EQ(m[3][1], 0.0F);
    ASSERT_FLOAT_EQ(m[3][2], 0.0F);
    ASSERT_FLOAT_EQ(m[3][3], 1.0F);

    ASSERT_EQ(m[0], m.columns[0]);
    ASSERT_EQ(m[1], m.columns[1]);
    ASSERT_EQ(m[2], m.columns[2]);
    ASSERT_EQ(m[3], m.columns[3]);

    ASSERT_EQ(m[0][0], m.m00);
    ASSERT_EQ(m[0][1], m.m10);
    ASSERT_EQ(m[0][2], m.m20);
    ASSERT_EQ(m[0][3], m.m30);
    ASSERT_EQ(m[1][0], m.m01);
    ASSERT_EQ(m[1][1], m.m11);
    ASSERT_EQ(m[1][2], m.m21);
    ASSERT_EQ(m[1][3], m.m31);
    ASSERT_EQ(m[2][0], m.m02);
    ASSERT_EQ(m[2][1], m.m12);
    ASSERT_EQ(m[2][2], m.m22);
    ASSERT_EQ(m[2][3], m.m32);
    ASSERT_EQ(m[3][0], m.m03);
    ASSERT_EQ(m[3][1], m.m13);
    ASSERT_EQ(m[3][2], m.m23);
    ASSERT_EQ(m[3][3], m.m33);
}

TEST(Mat4F, ColumnConstructor)
{
    const vec4 col0(1.0F, 2.0F, 3.0F, 4.0F);
    const vec4 col1(5.0F, 6.0F, 7.0F, 8.0F);
    const vec4 col2(9.0F, 10.0F, 11.0F, 12.0F);
    const vec4 col3(13.0F, 14.0F, 15.0F, 16.0F);
    mat4 m(col0, col1, col2, col3);

    ASSERT_FLOAT_EQ(m[0][0], 1.0F);
    ASSERT_FLOAT_EQ(m[0][1], 2.0F);
    ASSERT_FLOAT_EQ(m[0][2], 3.0F);
    ASSERT_FLOAT_EQ(m[0][3], 4.0F);

    ASSERT_FLOAT_EQ(m[1][0], 5.0F);
    ASSERT_FLOAT_EQ(m[1][1], 6.0F);
    ASSERT_FLOAT_EQ(m[1][2], 7.0F);
    ASSERT_FLOAT_EQ(m[1][3], 8.0F);

    ASSERT_FLOAT_EQ(m[2][0], 9.0F);
    ASSERT_FLOAT_EQ(m[2][1], 10.0F);
    ASSERT_FLOAT_EQ(m[2][2], 11.0F);
    ASSERT_FLOAT_EQ(m[2][3], 12.0F);

    ASSERT_FLOAT_EQ(m[3][0], 13.0F);
    ASSERT_FLOAT_EQ(m[3][1], 14.0F);
    ASSERT_FLOAT_EQ(m[3][2], 15.0F);
    ASSERT_FLOAT_EQ(m[3][3], 16.0F);

    ASSERT_EQ(m[0], m.columns[0]);
    ASSERT_EQ(m[1], m.columns[1]);
    ASSERT_EQ(m[2], m.columns[2]);
    ASSERT_EQ(m[3], m.columns[3]);

    ASSERT_EQ(m[0], col0);
    ASSERT_EQ(m[1], col1);
    ASSERT_EQ(m[2], col2);
    ASSERT_EQ(m[3], col3);

    ASSERT_EQ(m[0][0], m.m00);
    ASSERT_EQ(m[0][1], m.m10);
    ASSERT_EQ(m[0][2], m.m20);
    ASSERT_EQ(m[0][3], m.m30);
    ASSERT_EQ(m[1][0], m.m01);
    ASSERT_EQ(m[1][1], m.m11);
    ASSERT_EQ(m[1][2], m.m21);
    ASSERT_EQ(m[1][3], m.m31);
    ASSERT_EQ(m[2][0], m.m02);
    ASSERT_EQ(m[2][1], m.m12);
    ASSERT_EQ(m[2][2], m.m22);
    ASSERT_EQ(m[2][3], m.m32);
    ASSERT_EQ(m[3][0], m.m03);
    ASSERT_EQ(m[3][1], m.m13);
    ASSERT_EQ(m[3][2], m.m23);
    ASSERT_EQ(m[3][3], m.m33);
}

TEST(Mat4F, ElementConstructor)
{
    mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);

    ASSERT_FLOAT_EQ(m[0][0], 1.0F);
    ASSERT_FLOAT_EQ(m[0][1], 2.0F);
    ASSERT_FLOAT_EQ(m[0][2], 3.0F);
    ASSERT_FLOAT_EQ(m[0][3], 4.0F);

    ASSERT_FLOAT_EQ(m[1][0], 5.0F);
    ASSERT_FLOAT_EQ(m[1][1], 6.0F);
    ASSERT_FLOAT_EQ(m[1][2], 7.0F);
    ASSERT_FLOAT_EQ(m[1][3], 8.0F);

    ASSERT_FLOAT_EQ(m[2][0], 9.0F);
    ASSERT_FLOAT_EQ(m[2][1], 10.0F);
    ASSERT_FLOAT_EQ(m[2][2], 11.0F);
    ASSERT_FLOAT_EQ(m[2][3], 12.0F);

    ASSERT_FLOAT_EQ(m[3][0], 13.0F);
    ASSERT_FLOAT_EQ(m[3][1], 14.0F);
    ASSERT_FLOAT_EQ(m[3][2], 15.0F);
    ASSERT_FLOAT_EQ(m[3][3], 16.0F);

    ASSERT_EQ(m[0], m.columns[0]);
    ASSERT_EQ(m[1], m.columns[1]);
    ASSERT_EQ(m[2], m.columns[2]);
    ASSERT_EQ(m[3], m.columns[3]);

    ASSERT_EQ(m[0][0], m.m00);
    ASSERT_EQ(m[0][1], m.m10);
    ASSERT_EQ(m[0][2], m.m20);
    ASSERT_EQ(m[0][3], m.m30);
    ASSERT_EQ(m[1][0], m.m01);
    ASSERT_EQ(m[1][1], m.m11);
    ASSERT_EQ(m[1][2], m.m21);
    ASSERT_EQ(m[1][3], m.m31);
    ASSERT_EQ(m[2][0], m.m02);
    ASSERT_EQ(m[2][1], m.m12);
    ASSERT_EQ(m[2][2], m.m22);
    ASSERT_EQ(m[2][3], m.m32);
    ASSERT_EQ(m[3][0], m.m03);
    ASSERT_EQ(m[3][1], m.m13);
    ASSERT_EQ(m[3][2], m.m23);
    ASSERT_EQ(m[3][3], m.m33);
}

TEST(Mat4F, CopyConstructor)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    mat4 n(m);

    ASSERT_FLOAT_EQ(n[0][0], 1.0F);
    ASSERT_FLOAT_EQ(n[0][1], 2.0F);
    ASSERT_FLOAT_EQ(n[0][2], 3.0F);
    ASSERT_FLOAT_EQ(n[0][3], 4.0F);

    ASSERT_FLOAT_EQ(n[1][0], 5.0F);
    ASSERT_FLOAT_EQ(n[1][1], 6.0F);
    ASSERT_FLOAT_EQ(n[1][2], 7.0F);
    ASSERT_FLOAT_EQ(n[1][3], 8.0F);

    ASSERT_FLOAT_EQ(n[2][0], 9.0F);
    ASSERT_FLOAT_EQ(n[2][1], 10.0F);
    ASSERT_FLOAT_EQ(n[2][2], 11.0F);
    ASSERT_FLOAT_EQ(n[2][3], 12.0F);

    ASSERT_FLOAT_EQ(n[3][0], 13.0F);
    ASSERT_FLOAT_EQ(n[3][1], 14.0F);
    ASSERT_FLOAT_EQ(n[3][2], 15.0F);
    ASSERT_FLOAT_EQ(n[3][3], 16.0F);
}

TEST(Mat4F, MoveConstructor)
{
    mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    mat4 n(tempest::move(m));

    ASSERT_FLOAT_EQ(n[0][0], 1.0F);
    ASSERT_FLOAT_EQ(n[0][1], 2.0F);
    ASSERT_FLOAT_EQ(n[0][2], 3.0F);
    ASSERT_FLOAT_EQ(n[0][3], 4.0F);

    ASSERT_FLOAT_EQ(n[1][0], 5.0F);
    ASSERT_FLOAT_EQ(n[1][1], 6.0F);
    ASSERT_FLOAT_EQ(n[1][2], 7.0F);
    ASSERT_FLOAT_EQ(n[1][3], 8.0F);

    ASSERT_FLOAT_EQ(n[2][0], 9.0F);
    ASSERT_FLOAT_EQ(n[2][1], 10.0F);
    ASSERT_FLOAT_EQ(n[2][2], 11.0F);
    ASSERT_FLOAT_EQ(n[2][3], 12.0F);

    ASSERT_FLOAT_EQ(n[3][0], 13.0F);
    ASSERT_FLOAT_EQ(n[3][1], 14.0F);
    ASSERT_FLOAT_EQ(n[3][2], 15.0F);
    ASSERT_FLOAT_EQ(n[3][3], 16.0F);
}

TEST(Mat4F, IndexOperator)
{
    mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);

    ASSERT_EQ(m[0], vec4(1.0F, 2.0F, 3.0F, 4.0F));
    ASSERT_EQ(m[1], vec4(5.0F, 6.0F, 7.0F, 8.0F));
    ASSERT_EQ(m[2], vec4(9.0F, 10.0F, 11.0F, 12.0F));
    ASSERT_EQ(m[3], vec4(13.0F, 14.0F, 15.0F, 16.0F));
}

TEST(Mat4F, ConstIndexOperator)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);

    ASSERT_EQ(m[0], vec4(1.0F, 2.0F, 3.0F, 4.0F));
    ASSERT_EQ(m[1], vec4(5.0F, 6.0F, 7.0F, 8.0F));
    ASSERT_EQ(m[2], vec4(9.0F, 10.0F, 11.0F, 12.0F));
    ASSERT_EQ(m[3], vec4(13.0F, 14.0F, 15.0F, 16.0F));
}

TEST(Mat4F, EqualityOperatorEquals)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);

    ASSERT_TRUE(m == n);
    ASSERT_TRUE(n == m);
}

TEST(Mat4F, EqualityOperatorNotEqualsFirstElement)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(0.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);

    ASSERT_FALSE(m == n);
    ASSERT_FALSE(n == m);
}

TEST(Mat4F, EqualityOperatorNotEqualsLastElement)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 17.0F);

    ASSERT_FALSE(m == n);
    ASSERT_FALSE(n == m);
}

TEST(Mat4F, EqualityOperatorNotEqualsNoCommonElements)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(-1.0F);

    ASSERT_FALSE(m == n);
    ASSERT_FALSE(n == m);
}

TEST(Mat4F, InequalityOperatorEquals)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);

    ASSERT_FALSE(m != n);
    ASSERT_FALSE(n != m);
}

TEST(Mat4F, InequalityOperatorNotEqualsFirstElement)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(0.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);

    ASSERT_TRUE(m != n);
    ASSERT_TRUE(n != m);
}

TEST(Mat4F, InequalityOperatorNotEqualsLastElement)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 17.0F);

    ASSERT_TRUE(m != n);
    ASSERT_TRUE(n != m);
}

TEST(Mat4F, InqualityOperatorNotEqualsNoCommonElements)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(-1.0F);

    ASSERT_TRUE(m != n);
    ASSERT_TRUE(n != m);
}

TEST(Mat4F, AdditionOperator)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F, 17.0F, 18.0F);
    const mat4 sum = m + n;

    ASSERT_NEAR(sum[0][0], 4.0F, 0.0001F);
    ASSERT_NEAR(sum[0][1], 6.0F, 0.0001F);
    ASSERT_NEAR(sum[0][2], 8.0F, 0.0001F);
    ASSERT_NEAR(sum[0][3], 10.0F, 0.0001F);

    ASSERT_NEAR(sum[1][0], 12.0F, 0.0001F);
    ASSERT_NEAR(sum[1][1], 14.0F, 0.0001F);
    ASSERT_NEAR(sum[1][2], 16.0F, 0.0001F);
    ASSERT_NEAR(sum[1][3], 18.0F, 0.0001F);

    ASSERT_NEAR(sum[2][0], 20.0F, 0.0001F);
    ASSERT_NEAR(sum[2][1], 22.0F, 0.0001F);
    ASSERT_NEAR(sum[2][2], 24.0F, 0.0001F);
    ASSERT_NEAR(sum[2][3], 26.0F, 0.0001F);

    ASSERT_NEAR(sum[3][0], 28.0F, 0.0001F);
    ASSERT_NEAR(sum[3][1], 30.0F, 0.0001F);
    ASSERT_NEAR(sum[3][2], 32.0F, 0.0001F);
    ASSERT_NEAR(sum[3][3], 34.0F, 0.0001F);
}

TEST(Mat4F, SubtractionOperator)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(16.0F, 15.0F, 14.0F, 13.0F, 12.0F, 11.0F, 10.0F, 9.0F, 8.0F, 7.0F, 6.0F, 5.0F, 4.0F, 3.0F, 2.0F, 1.0F);
    const mat4 diff = m - n;

    ASSERT_NEAR(diff[0][0], -15.0F, 0.0001F);
    ASSERT_NEAR(diff[0][1], -13.0F, 0.0001F);
    ASSERT_NEAR(diff[0][2], -11.0F, 0.0001F);
    ASSERT_NEAR(diff[0][3], -9.0F, 0.0001F);

    ASSERT_NEAR(diff[1][0], -7.0F, 0.0001F);
    ASSERT_NEAR(diff[1][1], -5.0F, 0.0001F);
    ASSERT_NEAR(diff[1][2], -3.0F, 0.0001F);
    ASSERT_NEAR(diff[1][3], -1.0F, 0.0001F);

    ASSERT_NEAR(diff[2][0], 1.0F, 0.0001F);
    ASSERT_NEAR(diff[2][1], 3.0F, 0.0001F);
    ASSERT_NEAR(diff[2][2], 5.0F, 0.0001F);
    ASSERT_NEAR(diff[2][3], 7.0F, 0.0001F);

    ASSERT_NEAR(diff[3][0], 9.0F, 0.0001F);
    ASSERT_NEAR(diff[3][1], 11.0F, 0.0001F);
    ASSERT_NEAR(diff[3][2], 13.0F, 0.0001F);
    ASSERT_NEAR(diff[3][3], 15.0F, 0.0001F);
}

TEST(Mat4F, MultiplyMatricesOperator)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(16.0F, 15.0F, 14.0F, 13.0F, 12.0F, 11.0F, 10.0F, 9.0F, 8.0F, 7.0F, 6.0F, 5.0F, 4.0F, 3.0F, 2.0F, 1.0F);
    const mat4 prod = m * n;

    ASSERT_NEAR(prod[0][0], 386.0F, 0.001F);
    ASSERT_NEAR(prod[0][1], 444.0F, 0.001F);
    ASSERT_NEAR(prod[0][2], 502.0F, 0.001F);
    ASSERT_NEAR(prod[0][3], 560.0F, 0.001F);

    ASSERT_NEAR(prod[1][0], 274.0F, 0.001F);
    ASSERT_NEAR(prod[1][1], 316.0F, 0.001F);
    ASSERT_NEAR(prod[1][2], 358.0F, 0.001F);
    ASSERT_NEAR(prod[1][3], 400.0F, 0.001F);

    ASSERT_NEAR(prod[2][0], 162.0F, 0.001F);
    ASSERT_NEAR(prod[2][1], 188.0F, 0.001F);
    ASSERT_NEAR(prod[2][2], 214.0F, 0.001F);
    ASSERT_NEAR(prod[2][3], 240.0F, 0.001F);

    ASSERT_NEAR(prod[3][0], 50.0F, 0.001F);
    ASSERT_NEAR(prod[3][1], 60.0F, 0.001F);
    ASSERT_NEAR(prod[3][2], 70.0F, 0.001F);
    ASSERT_NEAR(prod[3][3], 80.0F, 0.001F);
}

TEST(Mat4F, MultiplyMatrixByVectorOperator)
{
    const mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const vec4 n(17.0F, 18.0F, 19.0F, 20.0F);
    const vec4 prod = m * n;

    ASSERT_NEAR(prod[0], 538.0F, 0.001F);
    ASSERT_NEAR(prod[1], 612.0F, 0.001F);
    ASSERT_NEAR(prod[2], 686.0F, 0.001F);
    ASSERT_NEAR(prod[3], 760.0F, 0.001F);
}

TEST(Mat4F, MatrixPlusEquals)
{
    mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F, 17.0F, 18.0F);
    m += n;

    ASSERT_NEAR(m[0][0], 4.0F, 0.0001F);
    ASSERT_NEAR(m[0][1], 6.0F, 0.0001F);
    ASSERT_NEAR(m[0][2], 8.0F, 0.0001F);
    ASSERT_NEAR(m[0][3], 10.0F, 0.0001F);
 
    ASSERT_NEAR(m[1][0], 12.0F, 0.0001F);
    ASSERT_NEAR(m[1][1], 14.0F, 0.0001F);
    ASSERT_NEAR(m[1][2], 16.0F, 0.0001F);
    ASSERT_NEAR(m[1][3], 18.0F, 0.0001F);

    ASSERT_NEAR(m[2][0], 20.0F, 0.0001F);
    ASSERT_NEAR(m[2][1], 22.0F, 0.0001F);
    ASSERT_NEAR(m[2][2], 24.0F, 0.0001F);
    ASSERT_NEAR(m[2][3], 26.0F, 0.0001F);

    ASSERT_NEAR(m[3][0], 28.0F, 0.0001F);
    ASSERT_NEAR(m[3][1], 30.0F, 0.0001F);
    ASSERT_NEAR(m[3][2], 32.0F, 0.0001F);
    ASSERT_NEAR(m[3][3], 34.0F, 0.0001F);
}

TEST(Mat4F, MatrixMinusEquals)
{
    mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(16.0F, 15.0F, 14.0F, 13.0F, 12.0F, 11.0F, 10.0F, 9.0F, 8.0F, 7.0F, 6.0F, 5.0F, 4.0F, 3.0F, 2.0F, 1.0F);
    m -= n;

    ASSERT_NEAR(m[0][0], -15.0F, 0.0001F);
    ASSERT_NEAR(m[0][1], -13.0F, 0.0001F);
    ASSERT_NEAR(m[0][2], -11.0F, 0.0001F);
    ASSERT_NEAR(m[0][3], -9.0F, 0.0001F);

    ASSERT_NEAR(m[1][0], -7.0F, 0.0001F);
    ASSERT_NEAR(m[1][1], -5.0F, 0.0001F);
    ASSERT_NEAR(m[1][2], -3.0F, 0.0001F);
    ASSERT_NEAR(m[1][3], -1.0F, 0.0001F);

    ASSERT_NEAR(m[2][0], 1.0F, 0.0001F);
    ASSERT_NEAR(m[2][1], 3.0F, 0.0001F);
    ASSERT_NEAR(m[2][2], 5.0F, 0.0001F);
    ASSERT_NEAR(m[2][3], 7.0F, 0.0001F);

    ASSERT_NEAR(m[3][0], 9.0F, 0.0001F);
    ASSERT_NEAR(m[3][1], 11.0F, 0.0001F);
    ASSERT_NEAR(m[3][2], 13.0F, 0.0001F);
    ASSERT_NEAR(m[3][3], 15.0F, 0.0001F);
}

TEST(Mat4F, MultiplyEquals)
{
    mat4 m(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F);
    const mat4 n(16.0F, 15.0F, 14.0F, 13.0F, 12.0F, 11.0F, 10.0F, 9.0F, 8.0F, 7.0F, 6.0F, 5.0F, 4.0F, 3.0F, 2.0F, 1.0F);
    m *= n;

    ASSERT_NEAR(m[0][0], 386.0F, 0.001F);
    ASSERT_NEAR(m[0][1], 444.0F, 0.001F);
    ASSERT_NEAR(m[0][2], 502.0F, 0.001F);
    ASSERT_NEAR(m[0][3], 560.0F, 0.001F);

    ASSERT_NEAR(m[1][0], 274.0F, 0.001F);
    ASSERT_NEAR(m[1][1], 316.0F, 0.001F);
    ASSERT_NEAR(m[1][2], 358.0F, 0.001F);
    ASSERT_NEAR(m[1][3], 400.0F, 0.001F);

    ASSERT_NEAR(m[2][0], 162.0F, 0.001F);
    ASSERT_NEAR(m[2][1], 188.0F, 0.001F);
    ASSERT_NEAR(m[2][2], 214.0F, 0.001F);
    ASSERT_NEAR(m[2][3], 240.0F, 0.001F);

    ASSERT_NEAR(m[3][0], 50.0F, 0.001F);
    ASSERT_NEAR(m[3][1], 60.0F, 0.001F);
    ASSERT_NEAR(m[3][2], 70.0F, 0.001F);
    ASSERT_NEAR(m[3][3], 80.0F, 0.001F);
}

