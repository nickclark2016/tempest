#include <gtest/gtest.h>

#include <tempest/mat3.hpp>
#include <tempest/mat4.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/quat.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

using tempest::math::as_radians;
using tempest::math::decompose;
using tempest::math::fmat4;
using tempest::math::fquat;
using tempest::math::look_at;
using tempest::math::normalize;
using tempest::math::ortho;
using tempest::math::perspective;
using tempest::math::rotate;
using tempest::math::scale;
using tempest::math::transform;
using tempest::math::translate;
using tempest::math::vec3;
using tempest::math::vec4;

// ============================================================================
// Section: Basic Affine Transformations
// ============================================================================

/// @brief Tests translation matrix creation and vector transformation.
TEST(transformations_test, translate_vector_transformation)
{
    // 1. Setup
    const auto t_vec = vec3<float>{10.0F, -5.0F, 3.0F};
    const auto p = vec4<float>{1.0F, 2.0F, 3.0F, 1.0F};

    // 2. Act
    const auto m = translate(t_vec);
    const auto transformed = m * p;

    // 3. Assert
    EXPECT_FLOAT_EQ(transformed.x, 11.0F);
    EXPECT_FLOAT_EQ(transformed.y, -3.0F);
    EXPECT_FLOAT_EQ(transformed.z, 6.0F);
    EXPECT_FLOAT_EQ(transformed.w, 1.0F);
}

/// @brief Tests scaling matrix creation and vector transformation.
TEST(transformations_test, scale_vector_transformation)
{
    // 1. Setup
    const auto s_vec = vec3<float>{2.0F, 0.5F, -3.0F};
    const auto p = vec4<float>{4.0F, 8.0F, 2.0F, 1.0F};

    // 2. Act
    const auto m = scale(s_vec);
    const auto transformed = m * p;

    // 3. Assert
    EXPECT_FLOAT_EQ(transformed.x, 8.0F);
    EXPECT_FLOAT_EQ(transformed.y, 4.0F);
    EXPECT_FLOAT_EQ(transformed.z, -6.0F);
    EXPECT_FLOAT_EQ(transformed.w, 1.0F);
}

/// @brief Tests rotation matrix creation around an axis.
TEST(transformations_test, rotate_axis_angle_transformation)
{
    // 1. Setup - 90 degrees around Z axis
    const auto angle = as_radians(90.0F);
    const auto axis = vec3<float>{0.0F, 0.0F, 1.0F};
    const auto p = vec4<float>{1.0F, 0.0F, 0.0F, 1.0F};

    // 2. Act
    const auto m = rotate(angle, axis);
    const auto transformed = m * p;

    // 3. Assert
    EXPECT_NEAR(transformed.x, 0.0F, 1e-4F);
    EXPECT_NEAR(transformed.y, 1.0F, 1e-4F);
    EXPECT_NEAR(transformed.z, 0.0F, 1e-4F);
    EXPECT_FLOAT_EQ(transformed.w, 1.0F);
}

// ============================================================================
// Section: TRS Formulation & Matrix Decomposition
// ============================================================================

/// @brief Tests that math::transform produces T * R * S matrix composition.
TEST(transformations_test, transform_trs_composition)
{
    // 1. Setup
    const auto t = vec3<float>{1.0F, 2.0F, 3.0F};
    const auto r_euler = vec3<float>{as_radians(30.0F), as_radians(45.0F), as_radians(60.0F)};
    const auto r_quat = normalize(fquat{r_euler});
    const auto s = vec3<float>{2.0F, 3.0F, 4.0F};

    // 2. Act
    const auto trs_quat = transform(t, r_quat, s);
    const auto trs_euler = transform(t, r_euler, s);

    // 3. Assert
    for (size_t col = 0; col < 4; ++col)
    {
        for (size_t row = 0; row < 4; ++row)
        {
            EXPECT_NEAR(trs_quat[col][row], trs_euler[col][row], 1e-4F);
        }
    }
}

/// @brief Tests matrix decomposition roundtrip with translation, rotation, and non-uniform scaling.
TEST(transformations_test, decompose_roundtrip_general_transform)
{
    // 1. Setup
    const auto original_t = vec3<float>{12.5F, -8.2F, 4.7F};
    const auto original_r = normalize(fquat{as_radians(vec3<float>{20.0F, -40.0F, 70.0F})});
    const auto original_s = vec3<float>{1.5F, 2.5F, 0.75F};

    const auto m = transform(original_t, original_r, original_s);

    // 2. Act
    auto extracted_t = vec3<float>{};
    auto extracted_r = fquat{};
    auto extracted_s = vec3<float>{};

    const auto success = decompose(m, extracted_t, extracted_r, extracted_s);

    // 3. Assert
    EXPECT_TRUE(success);

    EXPECT_NEAR(extracted_t.x, original_t.x, 1e-4F);
    EXPECT_NEAR(extracted_t.y, original_t.y, 1e-4F);
    EXPECT_NEAR(extracted_t.z, original_t.z, 1e-4F);

    EXPECT_NEAR(extracted_s.x, original_s.x, 1e-4F);
    EXPECT_NEAR(extracted_s.y, original_s.y, 1e-4F);
    EXPECT_NEAR(extracted_s.z, original_s.z, 1e-4F);

    // Reconstructed matrix from decomposed parameters must match original matrix
    const auto reconstructed_m = transform(extracted_t, extracted_r, extracted_s);
    for (size_t col = 0; col < 4; ++col)
    {
        for (size_t row = 0; row < 4; ++row)
        {
            EXPECT_NEAR(reconstructed_m[col][row], m[col][row], 1e-4F);
        }
    }
}

/// @brief Tests matrix decomposition on identity matrix.
TEST(transformations_test, decompose_identity_matrix)
{
    // 1. Setup
    const auto m = fmat4{1.0F};

    // 2. Act
    auto t = vec3<float>{};
    auto r = fquat{};
    auto s = vec3<float>{};

    const auto success = decompose(m, t, r, s);

    // 3. Assert
    EXPECT_TRUE(success);
    EXPECT_NEAR(t.x, 0.0F, 1e-5F);
    EXPECT_NEAR(t.y, 0.0F, 1e-5F);
    EXPECT_NEAR(t.z, 0.0F, 1e-5F);

    EXPECT_NEAR(s.x, 1.0F, 1e-5F);
    EXPECT_NEAR(s.y, 1.0F, 1e-5F);
    EXPECT_NEAR(s.z, 1.0F, 1e-5F);

    EXPECT_NEAR(r.w, 1.0F, 1e-5F);
    EXPECT_NEAR(r.x, 0.0F, 1e-5F);
    EXPECT_NEAR(r.y, 0.0F, 1e-5F);
    EXPECT_NEAR(r.z, 0.0F, 1e-5F);
}

// ============================================================================
// Section: Camera & Projection Matrices
// ============================================================================

/// @brief Tests look_at view matrix transformations.
TEST(transformations_test, look_at_view_matrix)
{
    // 1. Setup - Camera at (0, 0, 5) looking at (0, 0, 0) with up (0, 1, 0)
    const auto eye = vec3<float>{0.0F, 0.0F, 5.0F};
    const auto target = vec3<float>{0.0F, 0.0F, 0.0F};
    const auto up_dir = vec3<float>{0.0F, 1.0F, 0.0F};

    // 2. Act
    const auto view = look_at(eye, target, up_dir);
    const auto target_view_pos = view * vec4<float>{target.x, target.y, target.z, 1.0F};

    // 3. Assert - Target in view space must be at distance -5 along Z
    EXPECT_NEAR(target_view_pos.x, 0.0F, 1e-4F);
    EXPECT_NEAR(target_view_pos.y, 0.0F, 1e-4F);
    EXPECT_NEAR(target_view_pos.z, -5.0F, 1e-4F);
    EXPECT_FLOAT_EQ(target_view_pos.w, 1.0F);
}

/// @brief Tests infinite reverse-Z perspective projection near plane mapping.
TEST(transformations_test, reverse_z_perspective_near_plane)
{
    // 1. Setup
    const auto aspect = 16.0F / 9.0F;
    const auto fov = as_radians(60.0F);
    const auto near_plane = 0.1F;

    // 2. Act
    const auto proj = perspective(aspect, fov, near_plane);
    // Point on near plane in front of camera (z_view = -near_plane)
    const auto p_near = vec4<float>{0.0F, 0.0F, -near_plane, 1.0F};
    const auto clip_near = proj * p_near;
    const auto ndc_z = clip_near.z / clip_near.w;

    // 3. Assert - In reverse-Z, near plane maps to NDC z = 1.0
    EXPECT_NEAR(ndc_z, 1.0F, 1e-4F);
}
