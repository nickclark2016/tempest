#include <gtest/gtest.h>

#include <tempest/mat3.hpp>
#include <tempest/mat4.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/quat.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec3.hpp>

using tempest::math::as_degrees;
using tempest::math::as_mat3;
using tempest::math::as_mat4;
using tempest::math::as_radians;
using tempest::math::euler;
using tempest::math::extract_forward;
using tempest::math::extract_right;
using tempest::math::extract_up;
using tempest::math::fmat3;
using tempest::math::fmat4;
using tempest::math::fquat;
using tempest::math::normalize;
using tempest::math::pitch;
using tempest::math::quat;
using tempest::math::roll;
using tempest::math::vec3;
using tempest::math::vec4;
using tempest::math::yaw;

// ============================================================================
// Section: Constructors & Basic Properties
// ============================================================================

/// @brief Tests default and scalar construction of quaternions.
TEST(quat_test, default_and_scalar_constructors)
{
    // 1. Setup & Act
    const auto q_default = fquat{};
    const auto q_scalar = fquat{2.0F};

    // 2. Assert
    EXPECT_FLOAT_EQ(q_scalar.x, 2.0F);
    EXPECT_FLOAT_EQ(q_scalar.y, 2.0F);
    EXPECT_FLOAT_EQ(q_scalar.z, 2.0F);
    EXPECT_FLOAT_EQ(q_scalar.w, 2.0F);

    EXPECT_FLOAT_EQ(q_default.x, 0.0F);
    EXPECT_FLOAT_EQ(q_default.y, 0.0F);
    EXPECT_FLOAT_EQ(q_default.z, 0.0F);
    EXPECT_FLOAT_EQ(q_default.w, 0.0F);
}

/// @brief Tests explicit component construction and indexing.
TEST(quat_test, component_constructor_and_indexing)
{
    // 1. Setup & Act
    const auto q = fquat{1.0F, 2.0F, 3.0F, 4.0F};

    // 2. Assert
    EXPECT_FLOAT_EQ(q[0], 1.0F);
    EXPECT_FLOAT_EQ(q[1], 2.0F);
    EXPECT_FLOAT_EQ(q[2], 3.0F);
    EXPECT_FLOAT_EQ(q[3], 4.0F);
    EXPECT_FLOAT_EQ(q.x, 1.0F);
    EXPECT_FLOAT_EQ(q.y, 2.0F);
    EXPECT_FLOAT_EQ(q.z, 3.0F);
    EXPECT_FLOAT_EQ(q.w, 4.0F);
}

// ============================================================================
// Section: Norm, Normalization, & Multiplication
// ============================================================================

/// @brief Tests quaternion norm and normalization operations.
TEST(quat_test, norm_and_normalize)
{
    // 1. Setup
    const auto q = fquat{1.0F, 2.0F, 3.0F, 4.0F};

    // 2. Act
    const auto magnitude = norm(q);
    const auto q_unit = normalize(q);

    // 3. Assert
    EXPECT_NEAR(magnitude, tempest::math::sqrt(30.0F), 1e-4F);
    EXPECT_NEAR(norm(q_unit), 1.0F, 1e-5F);
}

/// @brief Tests quaternion-quaternion multiplication composition.
TEST(quat_test, quaternion_multiplication_composition)
{
    // 1. Setup - 90 degree rotation around X and 90 degree rotation around Y
    const auto qx = normalize(fquat{as_radians(vec3<float>{90.0F, 0.0F, 0.0F})});
    const auto qy = normalize(fquat{as_radians(vec3<float>{0.0F, 90.0F, 0.0F})});

    // 2. Act
    const auto q_combined = qx * qy;
    const auto v = vec3<float>{0.0F, 0.0F, 1.0F};
    const auto rotated_combined = q_combined * v;
    const auto rotated_sequential = qx * (qy * v);

    // 3. Assert - (qx * qy) * v must equal qx * (qy * v)
    EXPECT_NEAR(norm(q_combined), 1.0F, 1e-5F);
    EXPECT_NEAR(rotated_combined.x, rotated_sequential.x, 1e-4F);
    EXPECT_NEAR(rotated_combined.y, rotated_sequential.y, 1e-4F);
    EXPECT_NEAR(rotated_combined.z, rotated_sequential.z, 1e-4F);

    // +Z rotated by +90 around Y -> +X, rotated by +90 around X -> stays +X
    EXPECT_NEAR(rotated_combined.x, 1.0F, 1e-4F);
    EXPECT_NEAR(rotated_combined.y, 0.0F, 1e-4F);
    EXPECT_NEAR(rotated_combined.z, 0.0F, 1e-4F);
}

// ============================================================================
// Section: Euler Angle Conversions & Roundtrips
// ============================================================================

/// @brief Tests that converting Euler angles to quaternion and back preserves angles.
TEST(quat_test, euler_angle_roundtrip)
{
    // 1. Setup - Various pitch, yaw, roll combinations in radians
    const auto test_angles = {
        vec3<float>{0.0F, 0.0F, 0.0F},
        vec3<float>{as_radians(30.0F), 0.0F, 0.0F},
        vec3<float>{0.0F, as_radians(45.0F), 0.0F},
        vec3<float>{0.0F, 0.0F, as_radians(60.0F)},
        vec3<float>{as_radians(25.0F), as_radians(-35.0F), as_radians(50.0F)},
    };

    for (const auto& original_euler : test_angles)
    {
        // 2. Act
        const auto q = fquat{original_euler};
        const auto reconstructed_euler = euler(q);
        const auto q_reconstructed = fquat{reconstructed_euler};

        // 3. Assert
        EXPECT_NEAR(q.x, q_reconstructed.x, 1e-4F);
        EXPECT_NEAR(q.y, q_reconstructed.y, 1e-4F);
        EXPECT_NEAR(q.z, q_reconstructed.z, 1e-4F);
        EXPECT_NEAR(q.w, q_reconstructed.w, 1e-4F);
    }
}

// ============================================================================
// Section: Vector Rotation Equivalence & Matrix Conversion
// ============================================================================

/// @brief Tests that q * v produces identical results to as_mat3(q) * v and as_mat4(q) * v.
TEST(quat_test, vector_rotation_matrix_equivalence)
{
    // 1. Setup
    const auto angles = vec3<float>{as_radians(30.0F), as_radians(45.0F), as_radians(60.0F)};
    const auto q = normalize(fquat{angles});
    const auto m3 = as_mat3(q);
    const auto m4 = as_mat4(q);
    const auto v = vec3<float>{1.0F, 2.0F, 3.0F};

    // 2. Act
    const auto v_quat = q * v;
    const auto v_mat3 = m3 * v;
    const auto v4_res = m4 * vec4<float>{v.x, v.y, v.z, 1.0F};

    // 3. Assert
    EXPECT_NEAR(v_quat.x, v_mat3.x, 1e-4F);
    EXPECT_NEAR(v_quat.y, v_mat3.y, 1e-4F);
    EXPECT_NEAR(v_quat.z, v_mat3.z, 1e-4F);

    EXPECT_NEAR(v_quat.x, v4_res.x, 1e-4F);
    EXPECT_NEAR(v_quat.y, v4_res.y, 1e-4F);
    EXPECT_NEAR(v_quat.z, v4_res.z, 1e-4F);
}

/// @brief Tests extract_forward, extract_up, and extract_right against basis vector rotations.
TEST(quat_test, direction_extraction)
{
    // 1. Setup - Rotate 90 degrees around Y (turns forward +Z to +X)
    const auto q = normalize(fquat{as_radians(vec3<float>{0.0F, 90.0F, 0.0F})});

    // 2. Act
    const auto fwd = extract_forward(q);
    const auto up = extract_up(q);
    const auto rgt = extract_right(q);

    // 3. Assert
    EXPECT_NEAR(fwd.x, 1.0F, 1e-4F);
    EXPECT_NEAR(fwd.y, 0.0F, 1e-4F);
    EXPECT_NEAR(fwd.z, 0.0F, 1e-4F);

    EXPECT_NEAR(up.x, 0.0F, 1e-4F);
    EXPECT_NEAR(up.y, 1.0F, 1e-4F);
    EXPECT_NEAR(up.z, 0.0F, 1e-4F);

    EXPECT_NEAR(rgt.x, 0.0F, 1e-4F);
    EXPECT_NEAR(rgt.y, 0.0F, 1e-4F);
    EXPECT_NEAR(rgt.z, -1.0F, 1e-4F);
}
