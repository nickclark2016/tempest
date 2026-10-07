#include <gtest/gtest.h>

#include <tempest/math_utils.hpp>
#include <tempest/vec3.hpp>

using tempest::math::cross;
using tempest::math::dot;
using tempest::math::length;
using tempest::math::norm;
using tempest::math::normalize;
using tempest::math::vec3;

// ============================================================================
// Section: Length & Norm Operations
// ============================================================================

/// @brief Tests that length(v) and norm(v) return identical results across zero, basis, and general 3D vectors.
TEST(vec3_test, length_and_norm_equivalence)
{
    // 1. Setup
    const auto v_zero = vec3<float>{0.0F, 0.0F, 0.0F};
    const auto v_basis_x = vec3<float>{1.0F, 0.0F, 0.0F};
    const auto v_basis_y = vec3<float>{0.0F, 1.0F, 0.0F};
    const auto v_basis_z = vec3<float>{0.0F, 0.0F, 1.0F};
    const auto v_general = vec3<float>{-2.0F, 3.0F, -6.0F};

    // 2. Act & Assert
    EXPECT_FLOAT_EQ(length(v_zero), 0.0F);
    EXPECT_FLOAT_EQ(norm(v_zero), 0.0F);

    EXPECT_FLOAT_EQ(length(v_basis_x), 1.0F);
    EXPECT_FLOAT_EQ(norm(v_basis_x), 1.0F);

    EXPECT_FLOAT_EQ(length(v_basis_y), 1.0F);
    EXPECT_FLOAT_EQ(norm(v_basis_y), 1.0F);

    EXPECT_FLOAT_EQ(length(v_basis_z), 1.0F);
    EXPECT_FLOAT_EQ(norm(v_basis_z), 1.0F);

    // (-2)^2 + 3^2 + (-6)^2 = 4 + 9 + 36 = 49 -> sqrt = 7
    EXPECT_FLOAT_EQ(length(v_general), 7.0F);
    EXPECT_FLOAT_EQ(norm(v_general), 7.0F);
    EXPECT_FLOAT_EQ(length(v_general), norm(v_general));
}

/// @brief Tests vector length against known 3D Pythagorean quadruples.
TEST(vec3_test, length_pythagorean_quadruples)
{
    // 1. Setup - (1, 2, 2) -> norm 3, and (3, 4, 12) -> norm 13
    const auto v1 = vec3<float>{1.0F, 2.0F, 2.0F};
    const auto v2 = vec3<float>{3.0F, -4.0F, 12.0F};

    // 2. Act & Assert
    EXPECT_FLOAT_EQ(length(v1), 3.0F);
    EXPECT_FLOAT_EQ(length(v2), 13.0F);
}

// ============================================================================
// Section: Normalization & Basic Arithmetic
// ============================================================================

/// @brief Tests vector normalization producing a unit vector along the same direction.
TEST(vec3_test, normalize_produces_unit_vector)
{
    // 1. Setup
    const auto v = vec3<float>{3.0F, 0.0F, 4.0F};

    // 2. Act
    const auto u = normalize(v);

    // 3. Assert
    EXPECT_NEAR(length(u), 1.0F, 1e-6F);
    EXPECT_FLOAT_EQ(u.x, 0.6F);
    EXPECT_FLOAT_EQ(u.y, 0.0F);
    EXPECT_FLOAT_EQ(u.z, 0.8F);
}

/// @brief Tests component-wise vector addition, subtraction, and scalar multiplication.
TEST(vec3_test, basic_arithmetic)
{
    // 1. Setup
    const auto a = vec3<float>{1.0F, 2.0F, 3.0F};
    const auto b = vec3<float>{4.0F, 5.0F, 6.0F};

    // 2. Act
    const auto sum = a + b;
    const auto diff = b - a;
    const auto scaled_right = a * 2.0F;
    const auto scaled_left = 2.0F * a;

    // 3. Assert
    EXPECT_FLOAT_EQ(sum.x, 5.0F);
    EXPECT_FLOAT_EQ(sum.y, 7.0F);
    EXPECT_FLOAT_EQ(sum.z, 9.0F);

    EXPECT_FLOAT_EQ(diff.x, 3.0F);
    EXPECT_FLOAT_EQ(diff.y, 3.0F);
    EXPECT_FLOAT_EQ(diff.z, 3.0F);

    EXPECT_FLOAT_EQ(scaled_right.x, 2.0F);
    EXPECT_FLOAT_EQ(scaled_right.y, 4.0F);
    EXPECT_FLOAT_EQ(scaled_right.z, 6.0F);

    EXPECT_FLOAT_EQ(scaled_left.x, 2.0F);
    EXPECT_FLOAT_EQ(scaled_left.y, 4.0F);
    EXPECT_FLOAT_EQ(scaled_left.z, 6.0F);
}

/// @brief Tests dot and cross product properties.
TEST(vec3_test, dot_and_cross_products)
{
    // 1. Setup - Standard basis vectors
    const auto ex = vec3<float>{1.0F, 0.0F, 0.0F};
    const auto ey = vec3<float>{0.0F, 1.0F, 0.0F};
    const auto ez = vec3<float>{0.0F, 0.0F, 1.0F};

    // 2. Act & Assert - Dot product
    EXPECT_FLOAT_EQ(dot(ex, ex), 1.0F);
    EXPECT_FLOAT_EQ(dot(ex, ey), 0.0F);
    EXPECT_FLOAT_EQ(dot(ex, ez), 0.0F);

    // Right-handed cross products: X x Y = Z, Y x Z = X, Z x X = Y
    const auto cross_xy = cross(ex, ey);
    EXPECT_FLOAT_EQ(cross_xy.x, ez.x);
    EXPECT_FLOAT_EQ(cross_xy.y, ez.y);
    EXPECT_FLOAT_EQ(cross_xy.z, ez.z);

    const auto cross_yz = cross(ey, ez);
    EXPECT_FLOAT_EQ(cross_yz.x, ex.x);
    EXPECT_FLOAT_EQ(cross_yz.y, ex.y);
    EXPECT_FLOAT_EQ(cross_yz.z, ex.z);

    const auto cross_zx = cross(ez, ex);
    EXPECT_FLOAT_EQ(cross_zx.x, ey.x);
    EXPECT_FLOAT_EQ(cross_zx.y, ey.y);
    EXPECT_FLOAT_EQ(cross_zx.z, ey.z);
}
