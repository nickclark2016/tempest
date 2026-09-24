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
using tempest::math::cross;
using tempest::math::dot;
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

/// @brief Tests quaternion multiplication with identity and non-symmetric rotations.
TEST(quat_test, quaternion_multiplication_identity_and_non_symmetric)
{
    // 1. Setup - Identity and arbitrary rotation
    const auto q_ident = fquat{0.0F, 0.0F, 0.0F, 1.0F};
    const auto q_rot = normalize(fquat{as_radians(vec3<float>{30.0F, 45.0F, 60.0F})});

    // 2. Act
    const auto q_left = q_ident * q_rot;
    const auto q_right = q_rot * q_ident;

    // 3. Assert - Multiplying by identity leaves quaternion unchanged
    EXPECT_NEAR(q_left.x, q_rot.x, 1e-6F);
    EXPECT_NEAR(q_left.y, q_rot.y, 1e-6F);
    EXPECT_NEAR(q_left.z, q_rot.z, 1e-6F);
    EXPECT_NEAR(q_left.w, q_rot.w, 1e-6F);

    EXPECT_NEAR(q_right.x, q_rot.x, 1e-6F);
    EXPECT_NEAR(q_right.y, q_rot.y, 1e-6F);
    EXPECT_NEAR(q_right.z, q_rot.z, 1e-6F);
    EXPECT_NEAR(q_right.w, q_rot.w, 1e-6F);
}

// ============================================================================
// Section: Component Arithmetic & Scalar Scaling
// ============================================================================

/// @brief Tests component-wise quaternion addition, subtraction, and negation.
TEST(quat_test, component_addition_subtraction_negation)
{
    // 1. Setup
    const auto q1 = fquat{1.0F, 2.0F, 3.0F, 4.0F};
    const auto q2 = fquat{5.0F, 6.0F, 7.0F, 8.0F};

    // 2. Act
    const auto q_sum = q1 + q2;
    const auto q_diff = q2 - q1;
    const auto q_neg = -q1;

    // 3. Assert
    EXPECT_FLOAT_EQ(q_sum.x, 6.0F);
    EXPECT_FLOAT_EQ(q_sum.y, 8.0F);
    EXPECT_FLOAT_EQ(q_sum.z, 10.0F);
    EXPECT_FLOAT_EQ(q_sum.w, 12.0F);

    EXPECT_FLOAT_EQ(q_diff.x, 4.0F);
    EXPECT_FLOAT_EQ(q_diff.y, 4.0F);
    EXPECT_FLOAT_EQ(q_diff.z, 4.0F);
    EXPECT_FLOAT_EQ(q_diff.w, 4.0F);

    EXPECT_FLOAT_EQ(q_neg.x, -1.0F);
    EXPECT_FLOAT_EQ(q_neg.y, -2.0F);
    EXPECT_FLOAT_EQ(q_neg.z, -3.0F);
    EXPECT_FLOAT_EQ(q_neg.w, -4.0F);
}

/// @brief Tests scalar multiplication from both left and right sides.
TEST(quat_test, scalar_multiplication)
{
    // 1. Setup
    const auto q = fquat{1.0F, -2.0F, 3.0F, -4.0F};
    const auto scalar = 2.5F;

    // 2. Act
    const auto q_scaled_right = q * scalar;
    const auto q_scaled_left = scalar * q;

    // 3. Assert
    EXPECT_FLOAT_EQ(q_scaled_right.x, 2.5F);
    EXPECT_FLOAT_EQ(q_scaled_right.y, -5.0F);
    EXPECT_FLOAT_EQ(q_scaled_right.z, 7.5F);
    EXPECT_FLOAT_EQ(q_scaled_right.w, -10.0F);

    EXPECT_FLOAT_EQ(q_scaled_left.x, 2.5F);
    EXPECT_FLOAT_EQ(q_scaled_left.y, -5.0F);
    EXPECT_FLOAT_EQ(q_scaled_left.z, 7.5F);
    EXPECT_FLOAT_EQ(q_scaled_left.w, -10.0F);
}

// ============================================================================
// Section: Algebraic Invariants & Group Properties
// ============================================================================

/// @brief Tests quaternion multiplication associativity: (q1 * q2) * q3 == q1 * (q2 * q3).
TEST(quat_test, multiplication_associativity)
{
    // 1. Setup - Three arbitrary non-symmetric rotations
    const auto q1 = normalize(fquat{as_radians(vec3<float>{15.0F, 30.0F, 45.0F})});
    const auto q2 = normalize(fquat{as_radians(vec3<float>{-20.0F, 60.0F, -10.0F})});
    const auto q3 = normalize(fquat{as_radians(vec3<float>{40.0F, -25.0F, 70.0F})});

    // 2. Act
    const auto q_left_grouped = (q1 * q2) * q3;
    const auto q_right_grouped = q1 * (q2 * q3);

    // 3. Assert - Grouping must produce identical quaternion elements
    EXPECT_NEAR(q_left_grouped.x, q_right_grouped.x, 1e-5F);
    EXPECT_NEAR(q_left_grouped.y, q_right_grouped.y, 1e-5F);
    EXPECT_NEAR(q_left_grouped.z, q_right_grouped.z, 1e-5F);
    EXPECT_NEAR(q_left_grouped.w, q_right_grouped.w, 1e-5F);
}

/// @brief Tests unit quaternion conjugate as inverse: q * conjugate(q) == identity.
TEST(quat_test, conjugate_and_inverse)
{
    // 1. Setup - Arbitrary unit quaternion
    const auto q = normalize(fquat{as_radians(vec3<float>{35.0F, -50.0F, 22.0F})});
    const auto q_conj = fquat{-q.x, -q.y, -q.z, q.w};

    // 2. Act
    const auto q_product_right = q * q_conj;
    const auto q_product_left = q_conj * q;

    // 3. Assert - Product must equal identity quaternion (0, 0, 0, 1)
    EXPECT_NEAR(q_product_right.x, 0.0F, 1e-6F);
    EXPECT_NEAR(q_product_right.y, 0.0F, 1e-6F);
    EXPECT_NEAR(q_product_right.z, 0.0F, 1e-6F);
    EXPECT_NEAR(q_product_right.w, 1.0F, 1e-6F);

    EXPECT_NEAR(q_product_left.x, 0.0F, 1e-6F);
    EXPECT_NEAR(q_product_left.y, 0.0F, 1e-6F);
    EXPECT_NEAR(q_product_left.z, 0.0F, 1e-6F);
    EXPECT_NEAR(q_product_left.w, 1.0F, 1e-6F);
}

/// @brief Tests norm preservation under quaternion multiplication: norm(q1 * q2) == norm(q1) * norm(q2).
TEST(quat_test, norm_multiplication_preservation)
{
    // 1. Setup - Non-unit quaternions
    const auto q1 = fquat{1.0F, 2.0F, 3.0F, 4.0F};
    const auto q2 = fquat{2.0F, -1.0F, 4.0F, -3.0F};

    // 2. Act
    const auto q_prod = q1 * q2;
    const auto norm_prod = norm(q_prod);
    const auto expected_norm = norm(q1) * norm(q2);

    // 3. Assert
    EXPECT_NEAR(norm_prod, expected_norm, 1e-4F);
}

// ============================================================================
// Section: Incremental Integration & Continuity
// ============================================================================

/// @brief Tests numerical stability and normalization across 1,000 continuous incremental delta rotations.
TEST(quat_test, continuous_incremental_rotation_integration)
{
    // 1. Setup - Small rotation increment (0.5 deg pitch, 1.0 deg yaw per step)
    const auto delta_dt = 1.0F / 60.0F;
    const auto pitch_rate = 30.0F;
    const auto yaw_rate = 60.0F;
    const auto delta_rot = fquat{as_radians(vec3<float>{pitch_rate * delta_dt, yaw_rate * delta_dt, 0.0F})};

    auto current_orientation = fquat{0.0F, 0.0F, 0.0F, 1.0F};

    // 2. Act - Integrate 1,000 steps (simulating game loop execution)
    for (auto step_idx = 0; step_idx < 1000; ++step_idx)
    {
        current_orientation = normalize(current_orientation * delta_rot);

        // 3. Assert invariant at every single step: unit norm and valid real components
        ASSERT_NEAR(norm(current_orientation), 1.0F, 1e-5F);
        ASSERT_TRUE(tempest::is_finite(current_orientation.x));
        ASSERT_TRUE(tempest::is_finite(current_orientation.y));
        ASSERT_TRUE(tempest::is_finite(current_orientation.z));
        ASSERT_TRUE(tempest::is_finite(current_orientation.w));
    }
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

/// @brief Tests pure axis rotations produce quaternions with isolated non-zero components.
TEST(quat_test, pure_axis_rotation_isolation)
{
    // 1. Setup - Pure 90 degree rotations along X, Y, and Z
    const auto q_pitch = fquat{as_radians(vec3<float>{90.0F, 0.0F, 0.0F})};
    const auto q_yaw = fquat{as_radians(vec3<float>{0.0F, 90.0F, 0.0F})};
    const auto q_roll = fquat{as_radians(vec3<float>{0.0F, 0.0F, 90.0F})};

    // 2. Assert - Pure pitch affects only X and W
    EXPECT_NEAR(q_pitch.x, tempest::math::sin(as_radians(45.0F)), 1e-5F);
    EXPECT_NEAR(q_pitch.y, 0.0F, 1e-5F);
    EXPECT_NEAR(q_pitch.z, 0.0F, 1e-5F);
    EXPECT_NEAR(q_pitch.w, tempest::math::cos(as_radians(45.0F)), 1e-5F);

    // 3. Assert - Pure yaw affects only Y and W
    EXPECT_NEAR(q_yaw.x, 0.0F, 1e-5F);
    EXPECT_NEAR(q_yaw.y, tempest::math::sin(as_radians(45.0F)), 1e-5F);
    EXPECT_NEAR(q_yaw.z, 0.0F, 1e-5F);
    EXPECT_NEAR(q_yaw.w, tempest::math::cos(as_radians(45.0F)), 1e-5F);

    // 4. Assert - Pure roll affects only Z and W
    EXPECT_NEAR(q_roll.x, 0.0F, 1e-5F);
    EXPECT_NEAR(q_roll.y, 0.0F, 1e-5F);
    EXPECT_NEAR(q_roll.z, tempest::math::sin(as_radians(45.0F)), 1e-5F);
    EXPECT_NEAR(q_roll.w, tempest::math::cos(as_radians(45.0F)), 1e-5F);
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

/// @brief Tests that as_mat3 produces an orthonormal, right-handed matrix with determinant +1.
TEST(quat_test, matrix_orthogonality_and_determinant)
{
    // 1. Setup - Arbitrary rotation quaternion
    const auto q = normalize(fquat{as_radians(vec3<float>{25.0F, -40.0F, 65.0F})});

    // 2. Act
    const auto m = as_mat3(q);
    const auto c0 = m[0];
    const auto c1 = m[1];
    const auto c2 = m[2];

    // 3. Assert - Columns must be orthonormal unit vectors
    EXPECT_NEAR(dot(c0, c0), 1.0F, 1e-5F);
    EXPECT_NEAR(dot(c1, c1), 1.0F, 1e-5F);
    EXPECT_NEAR(dot(c2, c2), 1.0F, 1e-5F);

    EXPECT_NEAR(dot(c0, c1), 0.0F, 1e-5F);
    EXPECT_NEAR(dot(c1, c2), 0.0F, 1e-5F);
    EXPECT_NEAR(dot(c0, c2), 0.0F, 1e-5F);

    // Right-handed orientation: c0 x c1 == c2
    const auto c0_cross_c1 = cross(c0, c1);
    EXPECT_NEAR(c0_cross_c1.x, c2.x, 1e-5F);
    EXPECT_NEAR(c0_cross_c1.y, c2.y, 1e-5F);
    EXPECT_NEAR(c0_cross_c1.z, c2.z, 1e-5F);

    // Determinant: det(M) = c0 . (c1 x c2) == 1.0
    const auto det = dot(c0, cross(c1, c2));
    EXPECT_NEAR(det, 1.0F, 1e-5F);
}

// ============================================================================
// Section: Dot Product, Slerp, & Arithmetic
// ============================================================================

/// @brief Tests quaternion dot product properties including orthogonal and identical quaternions.
TEST(quat_test, dot_product)
{
    // 1. Setup
    const auto q1 = fquat{1.0F, 0.0F, 0.0F, 0.0F};
    const auto q2 = fquat{0.0F, 1.0F, 0.0F, 0.0F};
    const auto q3 = fquat{0.5F, 0.5F, 0.5F, 0.5F};

    // 2. Act & Assert
    EXPECT_FLOAT_EQ(tempest::math::dot(q1, q1), 1.0F);
    EXPECT_FLOAT_EQ(tempest::math::dot(q1, q2), 0.0F);
    EXPECT_FLOAT_EQ(tempest::math::dot(q3, q3), 1.0F);
}

/// @brief Tests quaternion slerp at endpoints and midpoint.
TEST(quat_test, slerp_endpoints_and_midpoint)
{
    // 1. Setup - 0 deg rotation and 90 deg rotation around Y
    const auto q_start = normalize(fquat{as_radians(vec3<float>{0.0F, 0.0F, 0.0F})});
    const auto q_end = normalize(fquat{as_radians(vec3<float>{0.0F, 90.0F, 0.0F})});

    // 2. Act
    const auto q_at_zero = tempest::math::slerp(q_start, q_end, 0.0F);
    const auto q_at_one = tempest::math::slerp(q_start, q_end, 1.0F);
    const auto q_at_half = tempest::math::slerp(q_start, q_end, 0.5F);

    // 3. Assert
    EXPECT_NEAR(q_at_zero.x, q_start.x, 1e-5F);
    EXPECT_NEAR(q_at_zero.y, q_start.y, 1e-5F);
    EXPECT_NEAR(q_at_zero.z, q_start.z, 1e-5F);
    EXPECT_NEAR(q_at_zero.w, q_start.w, 1e-5F);

    EXPECT_NEAR(q_at_one.x, q_end.x, 1e-5F);
    EXPECT_NEAR(q_at_one.y, q_end.y, 1e-5F);
    EXPECT_NEAR(q_at_one.z, q_end.z, 1e-5F);
    EXPECT_NEAR(q_at_one.w, q_end.w, 1e-5F);

    // Halfway rotation should rotate forward (+Z) by 45 degrees around Y
    const auto v = vec3<float>{0.0F, 0.0F, 1.0F};
    const auto v_half = q_at_half * v;
    const auto expected_rad = as_radians(45.0F);
    EXPECT_NEAR(v_half.x, tempest::math::sin(expected_rad), 1e-4F);
    EXPECT_NEAR(v_half.y, 0.0F, 1e-4F);
    EXPECT_NEAR(v_half.z, tempest::math::cos(expected_rad), 1e-4F);
}

/// @brief Tests that slerp traverses the shortest arc when dot product is negative (antipodal symmetry).
TEST(quat_test, slerp_shortest_arc_negative_dot)
{
    // 1. Setup - q and -q represent the exact same rotation, but dot(q, -q) = -1
    const auto q_start = normalize(fquat{as_radians(vec3<float>{0.0F, 10.0F, 0.0F})});
    const auto q_target = normalize(fquat{as_radians(vec3<float>{0.0F, 20.0F, 0.0F})});
    const auto q_neg_target = -q_target;

    EXPECT_LT(tempest::math::dot(q_start, q_neg_target), 0.0F);

    // 2. Act
    const auto q_interpolated = tempest::math::slerp(q_start, q_neg_target, 0.5F);

    // 3. Assert - intermediate rotation must be around 15 degrees, not 195 degrees
    const auto v = vec3<float>{0.0F, 0.0F, 1.0F};
    const auto v_mid = q_interpolated * v;
    const auto expected_rad = as_radians(15.0F);
    EXPECT_NEAR(v_mid.x, tempest::math::sin(expected_rad), 1e-3F);
    EXPECT_NEAR(v_mid.z, tempest::math::cos(expected_rad), 1e-3F);
}

/// @brief Tests slerp fallback to nlerp for nearly collinear quaternions.
TEST(quat_test, slerp_near_parallel)
{
    // 1. Setup - two quaternions separated by a tiny angle
    const auto q_start = normalize(fquat{0.0F, 0.0F, 0.0F, 1.0F});
    const auto q_end = normalize(fquat{1e-5F, 0.0F, 0.0F, 1.0F});

    // 2. Act
    const auto q_mid = tempest::math::slerp(q_start, q_end, 0.5F);

    // 3. Assert
    EXPECT_NEAR(norm(q_mid), 1.0F, 1e-5F);
    EXPECT_NEAR(q_mid.x, 0.5F * q_end.x, 1e-6F);
}

/// @brief Tests generalized vec3 lerp with scalar factor.
TEST(quat_test, vec3_lerp_with_scalar)
{
    // 1. Setup
    const auto v1 = vec3<float>{0.0F, 10.0F, 20.0F};
    const auto v2 = vec3<float>{10.0F, 20.0F, 40.0F};

    // 2. Act
    const auto result = tempest::math::lerp(v1, v2, 0.25F);

    // 3. Assert
    EXPECT_FLOAT_EQ(result.x, 2.5F);
    EXPECT_FLOAT_EQ(result.y, 12.5F);
    EXPECT_FLOAT_EQ(result.z, 25.0F);
}

/// @brief Tests slerp between identical quaternions produces identical result without NaN.
TEST(quat_test, slerp_identical_quaternions)
{
    // 1. Setup
    const auto q = normalize(fquat{as_radians(vec3<float>{30.0F, 40.0F, 50.0F})});

    // 2. Act
    const auto q_mid = tempest::math::slerp(q, q, 0.5F);

    // 3. Assert
    EXPECT_NEAR(q_mid.x, q.x, 1e-6F);
    EXPECT_NEAR(q_mid.y, q.y, 1e-6F);
    EXPECT_NEAR(q_mid.z, q.z, 1e-6F);
    EXPECT_NEAR(q_mid.w, q.w, 1e-6F);
}

/// @brief Tests slerp between near-antipodal rotations (179.9 degrees apart) evaluates stably.
TEST(quat_test, slerp_near_antipodal_angle)
{
    // 1. Setup - 0 deg and 179.9 deg rotation around Y
    const auto q_start = normalize(fquat{as_radians(vec3<float>{0.0F, 0.0F, 0.0F})});
    const auto q_end = normalize(fquat{as_radians(vec3<float>{0.0F, 179.9F, 0.0F})});

    // 2. Act
    const auto q_mid = tempest::math::slerp(q_start, q_end, 0.5F);

    // 3. Assert - Midpoint must rotate forward (+Z) by approx 89.95 deg around Y
    const auto v = vec3<float>{0.0F, 0.0F, 1.0F};
    const auto v_rotated = q_mid * v;
    const auto expected_rad = as_radians(89.95F);
    EXPECT_NEAR(v_rotated.x, tempest::math::sin(expected_rad), 1e-3F);
    EXPECT_NEAR(v_rotated.z, tempest::math::cos(expected_rad), 1e-3F);
    EXPECT_NEAR(norm(q_mid), 1.0F, 1e-5F);
}

/// @brief Tests slerp maintains constant angular velocity across uniform parameter intervals.
TEST(quat_test, slerp_constant_angular_velocity)
{
    // 1. Setup - 0 to 90 degrees around Y
    const auto q_start = normalize(fquat{as_radians(vec3<float>{0.0F, 0.0F, 0.0F})});
    const auto q_end = normalize(fquat{as_radians(vec3<float>{0.0F, 90.0F, 0.0F})});

    // 2. Act & Assert - Each 0.25 increment in alpha advances rotation by exactly 22.5 degrees
    const auto v = vec3<float>{0.0F, 0.0F, 1.0F};
    for (auto step_idx = 0; step_idx <= 4; ++step_idx)
    {
        const auto alpha = static_cast<float>(step_idx) * 0.25F;
        const auto q_interp = tempest::math::slerp(q_start, q_end, alpha);
        const auto v_rot = q_interp * v;
        const auto expected_angle_rad = as_radians(static_cast<float>(step_idx) * 22.5F);

        EXPECT_NEAR(v_rot.x, tempest::math::sin(expected_angle_rad), 1e-4F);
        EXPECT_NEAR(v_rot.y, 0.0F, 1e-4F);
        EXPECT_NEAR(v_rot.z, tempest::math::cos(expected_angle_rad), 1e-4F);
    }
}
