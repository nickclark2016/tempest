#include <tempest/builtin_assets.hpp>

#include <tempest/asset_serializers.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/utility.hpp>

namespace tempest::assets::builtin
{
    // =========================================================================
    // Canonical Geometry Generators
    // =========================================================================

    auto create_plane_mesh(float size) -> core::mesh
    {
        auto mesh_data = core::mesh{};
        mesh_data.name = "plane";
        const auto half_size = size * 0.5f;

        // Counter-clockwise quad facing +Y
        mesh_data.vertices.push_back(core::vertex{
            .position = {-half_size, 0.0f, half_size},
            .uv = {0.0f, 1.0f},
            .normal = {0.0f, 1.0f, 0.0f},
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
            .color = {1.0f, 1.0f, 1.0f, 1.0f},
        });
        mesh_data.vertices.push_back(core::vertex{
            .position = {half_size, 0.0f, half_size},
            .uv = {1.0f, 1.0f},
            .normal = {0.0f, 1.0f, 0.0f},
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
            .color = {1.0f, 1.0f, 1.0f, 1.0f},
        });
        mesh_data.vertices.push_back(core::vertex{
            .position = {half_size, 0.0f, -half_size},
            .uv = {1.0f, 0.0f},
            .normal = {0.0f, 1.0f, 0.0f},
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
            .color = {1.0f, 1.0f, 1.0f, 1.0f},
        });
        mesh_data.vertices.push_back(core::vertex{
            .position = {-half_size, 0.0f, -half_size},
            .uv = {0.0f, 0.0f},
            .normal = {0.0f, 1.0f, 0.0f},
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
            .color = {1.0f, 1.0f, 1.0f, 1.0f},
        });

        mesh_data.indices.push_back(0);
        mesh_data.indices.push_back(1);
        mesh_data.indices.push_back(2);
        mesh_data.indices.push_back(2);
        mesh_data.indices.push_back(3);
        mesh_data.indices.push_back(0);

        mesh_data.has_normals = true;
        mesh_data.has_tangents = true;
        mesh_data.has_colors = true;
        return mesh_data;
    }

    auto create_box_mesh(float size) -> core::mesh
    {
        auto mesh_data = core::mesh{};
        mesh_data.name = "box";
        const auto half_size = size * 0.5f;

        auto add_face = [&](math::vec3<float> face_normal, math::vec3<float> face_tangent,
                            math::vec3<float> vertex_corner_zero, math::vec3<float> vertex_corner_one,
                            math::vec3<float> vertex_corner_two, math::vec3<float> vertex_corner_three) -> void {
            const auto base_vertex_index = static_cast<uint32_t>(mesh_data.vertices.size());
            mesh_data.vertices.push_back(core::vertex{
                .position = vertex_corner_zero,
                .uv = {0.0f, 0.0f},
                .normal = face_normal,
                .tangent = {face_tangent.x, face_tangent.y, face_tangent.z, 1.0f},
                .color = {1.0f, 1.0f, 1.0f, 1.0f},
            });
            mesh_data.vertices.push_back(core::vertex{
                .position = vertex_corner_one,
                .uv = {1.0f, 0.0f},
                .normal = face_normal,
                .tangent = {face_tangent.x, face_tangent.y, face_tangent.z, 1.0f},
                .color = {1.0f, 1.0f, 1.0f, 1.0f},
            });
            mesh_data.vertices.push_back(core::vertex{
                .position = vertex_corner_two,
                .uv = {1.0f, 1.0f},
                .normal = face_normal,
                .tangent = {face_tangent.x, face_tangent.y, face_tangent.z, 1.0f},
                .color = {1.0f, 1.0f, 1.0f, 1.0f},
            });
            mesh_data.vertices.push_back(core::vertex{
                .position = vertex_corner_three,
                .uv = {0.0f, 1.0f},
                .normal = face_normal,
                .tangent = {face_tangent.x, face_tangent.y, face_tangent.z, 1.0f},
                .color = {1.0f, 1.0f, 1.0f, 1.0f},
            });

            mesh_data.indices.push_back(base_vertex_index + 0);
            mesh_data.indices.push_back(base_vertex_index + 1);
            mesh_data.indices.push_back(base_vertex_index + 2);
            mesh_data.indices.push_back(base_vertex_index + 2);
            mesh_data.indices.push_back(base_vertex_index + 3);
            mesh_data.indices.push_back(base_vertex_index + 0);
        };

        // Front (+Z)
        add_face({0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {-half_size, -half_size, half_size},
                 {half_size, -half_size, half_size}, {half_size, half_size, half_size},
                 {-half_size, half_size, half_size});
        // Back (-Z)
        add_face({0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {half_size, -half_size, -half_size},
                 {-half_size, -half_size, -half_size}, {-half_size, half_size, -half_size},
                 {half_size, half_size, -half_size});
        // Left (-X)
        add_face({-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-half_size, -half_size, -half_size},
                 {-half_size, -half_size, half_size}, {-half_size, half_size, half_size},
                 {-half_size, half_size, -half_size});
        // Right (+X)
        add_face({1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {half_size, -half_size, half_size},
                 {half_size, -half_size, -half_size}, {half_size, half_size, -half_size},
                 {half_size, half_size, half_size});
        // Top (+Y)
        add_face({0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {-half_size, half_size, half_size},
                 {half_size, half_size, half_size}, {half_size, half_size, -half_size},
                 {-half_size, half_size, -half_size});
        // Bottom (-Y)
        add_face({0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {-half_size, -half_size, -half_size},
                 {half_size, -half_size, -half_size}, {half_size, -half_size, half_size},
                 {-half_size, -half_size, half_size});

        mesh_data.has_normals = true;
        mesh_data.has_tangents = true;
        mesh_data.has_colors = true;
        return mesh_data;
    }

    auto create_sphere_mesh(float radius, uint32_t rings, uint32_t sectors) -> core::mesh
    {
        auto mesh_data = core::mesh{};
        mesh_data.name = "sphere";
        const auto ring_count = tempest::max(rings, 2u);
        const auto sector_count = tempest::max(sectors, 3u);
        const auto pi_constant = math::constants::pi<float>;

        for (uint32_t ring_index = 0; ring_index <= ring_count; ++ring_index)
        {
            const auto vertical_fraction = static_cast<float>(ring_index) / static_cast<float>(ring_count);
            const auto phi_angle = vertical_fraction * pi_constant;
            const auto sin_phi = math::sin(phi_angle);
            const auto cos_phi = math::cos(phi_angle);
            const auto ring_radius = radius * sin_phi;
            const auto y_position = radius * cos_phi;

            for (uint32_t sector_index = 0; sector_index <= sector_count; ++sector_index)
            {
                const auto horizontal_fraction = static_cast<float>(sector_index) / static_cast<float>(sector_count);
                const auto theta_angle = horizontal_fraction * 2.0f * pi_constant;
                const auto sin_theta = math::sin(theta_angle);
                const auto cos_theta = math::cos(theta_angle);

                const auto x_position = ring_radius * sin_theta;
                const auto z_position = ring_radius * cos_theta;

                const auto normal_vector = (radius > 1e-6f)
                                               ? math::vec3<float>{x_position / radius, y_position / radius, z_position / radius}
                                               : math::vec3<float>{0.0f, 1.0f, 0.0f};

                mesh_data.vertices.push_back(core::vertex{
                    .position = {x_position, y_position, z_position},
                    .uv = {horizontal_fraction, vertical_fraction},
                    .normal = normal_vector,
                    .tangent = {cos_theta, 0.0f, -sin_theta, 1.0f},
                    .color = {1.0f, 1.0f, 1.0f, 1.0f},
                });
            }
        }

        for (uint32_t ring_index = 0; ring_index < ring_count; ++ring_index)
        {
            for (uint32_t sector_index = 0; sector_index < sector_count; ++sector_index)
            {
                const auto current_row_first = ring_index * (sector_count + 1) + sector_index;
                const auto next_row_first = (ring_index + 1) * (sector_count + 1) + sector_index;

                mesh_data.indices.push_back(current_row_first);
                mesh_data.indices.push_back(next_row_first);
                mesh_data.indices.push_back(current_row_first + 1);

                mesh_data.indices.push_back(current_row_first + 1);
                mesh_data.indices.push_back(next_row_first);
                mesh_data.indices.push_back(next_row_first + 1);
            }
        }

        mesh_data.has_normals = true;
        mesh_data.has_tangents = true;
        mesh_data.has_colors = true;
        return mesh_data;
    }

    auto create_cylinder_mesh(float radius, float height, uint32_t sectors) -> core::mesh
    {
        auto mesh_data = core::mesh{};
        mesh_data.name = "cylinder";
        const auto sector_count = tempest::max(sectors, 3u);
        const auto half_height = height * 0.5f;
        const auto pi_constant = math::constants::pi<float>;

        // 1. Generate cylinder side wall vertices
        for (uint32_t ring_index = 0; ring_index <= 1; ++ring_index)
        {
            const auto y_position = (ring_index == 0) ? half_height : -half_height;
            const auto vertical_uv = (ring_index == 0) ? 0.0f : 1.0f;

            for (uint32_t sector_index = 0; sector_index <= sector_count; ++sector_index)
            {
                const auto horizontal_fraction = static_cast<float>(sector_index) / static_cast<float>(sector_count);
                const auto theta_angle = horizontal_fraction * 2.0f * pi_constant;
                const auto sin_theta = math::sin(theta_angle);
                const auto cos_theta = math::cos(theta_angle);

                mesh_data.vertices.push_back(core::vertex{
                    .position = {radius * sin_theta, y_position, radius * cos_theta},
                    .uv = {horizontal_fraction, vertical_uv},
                    .normal = {sin_theta, 0.0f, cos_theta},
                    .tangent = {cos_theta, 0.0f, -sin_theta, 1.0f},
                    .color = {1.0f, 1.0f, 1.0f, 1.0f},
                });
            }
        }

        // Side indices
        for (uint32_t sector_index = 0; sector_index < sector_count; ++sector_index)
        {
            const auto top_vertex_index = sector_index;
            const auto bottom_vertex_index = (sector_count + 1) + sector_index;

            mesh_data.indices.push_back(top_vertex_index);
            mesh_data.indices.push_back(bottom_vertex_index);
            mesh_data.indices.push_back(top_vertex_index + 1);

            mesh_data.indices.push_back(top_vertex_index + 1);
            mesh_data.indices.push_back(bottom_vertex_index);
            mesh_data.indices.push_back(bottom_vertex_index + 1);
        }

        // 2. Top cap (+Y)
        const auto top_center_index = static_cast<uint32_t>(mesh_data.vertices.size());
        mesh_data.vertices.push_back(core::vertex{
            .position = {0.0f, half_height, 0.0f},
            .uv = {0.5f, 0.5f},
            .normal = {0.0f, 1.0f, 0.0f},
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
            .color = {1.0f, 1.0f, 1.0f, 1.0f},
        });

        const auto top_ring_start = static_cast<uint32_t>(mesh_data.vertices.size());
        for (uint32_t sector_index = 0; sector_index <= sector_count; ++sector_index)
        {
            const auto horizontal_fraction = static_cast<float>(sector_index) / static_cast<float>(sector_count);
            const auto theta_angle = horizontal_fraction * 2.0f * pi_constant;
            const auto sin_theta = math::sin(theta_angle);
            const auto cos_theta = math::cos(theta_angle);

            mesh_data.vertices.push_back(core::vertex{
                .position = {radius * sin_theta, half_height, radius * cos_theta},
                .uv = {0.5f + 0.5f * sin_theta, 0.5f + 0.5f * cos_theta},
                .normal = {0.0f, 1.0f, 0.0f},
                .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
                .color = {1.0f, 1.0f, 1.0f, 1.0f},
            });
        }

        for (uint32_t sector_index = 0; sector_index < sector_count; ++sector_index)
        {
            mesh_data.indices.push_back(top_center_index);
            mesh_data.indices.push_back(top_ring_start + sector_index + 1);
            mesh_data.indices.push_back(top_ring_start + sector_index);
        }

        // 3. Bottom cap (-Y)
        const auto bottom_center_index = static_cast<uint32_t>(mesh_data.vertices.size());
        mesh_data.vertices.push_back(core::vertex{
            .position = {0.0f, -half_height, 0.0f},
            .uv = {0.5f, 0.5f},
            .normal = {0.0f, -1.0f, 0.0f},
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
            .color = {1.0f, 1.0f, 1.0f, 1.0f},
        });

        const auto bottom_ring_start = static_cast<uint32_t>(mesh_data.vertices.size());
        for (uint32_t sector_index = 0; sector_index <= sector_count; ++sector_index)
        {
            const auto horizontal_fraction = static_cast<float>(sector_index) / static_cast<float>(sector_count);
            const auto theta_angle = horizontal_fraction * 2.0f * pi_constant;
            const auto sin_theta = math::sin(theta_angle);
            const auto cos_theta = math::cos(theta_angle);

            mesh_data.vertices.push_back(core::vertex{
                .position = {radius * sin_theta, -half_height, radius * cos_theta},
                .uv = {0.5f + 0.5f * sin_theta, 0.5f - 0.5f * cos_theta},
                .normal = {0.0f, -1.0f, 0.0f},
                .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
                .color = {1.0f, 1.0f, 1.0f, 1.0f},
            });
        }

        for (uint32_t sector_index = 0; sector_index < sector_count; ++sector_index)
        {
            mesh_data.indices.push_back(bottom_center_index);
            mesh_data.indices.push_back(bottom_ring_start + sector_index);
            mesh_data.indices.push_back(bottom_ring_start + sector_index + 1);
        }

        mesh_data.has_normals = true;
        mesh_data.has_tangents = true;
        mesh_data.has_colors = true;
        return mesh_data;
    }

    auto create_capsule_mesh(float radius, float height, uint32_t rings, uint32_t sectors) -> core::mesh
    {
        auto mesh_data = core::mesh{};
        mesh_data.name = "capsule";
        const auto ring_count = tempest::max(rings, 2u);
        const auto sector_count = tempest::max(sectors, 3u);
        const auto cylinder_height = tempest::max(0.0f, height - 2.0f * radius);
        const auto half_cylinder_height = cylinder_height * 0.5f;
        const auto pi_constant = math::constants::pi<float>;
        const auto half_pi_constant = math::constants::half_pi<float>;

        const auto total_ring_count = 2 * ring_count + 1;

        for (uint32_t ring_index = 0; ring_index <= total_ring_count; ++ring_index)
        {
            const auto ring_fraction = static_cast<float>(ring_index) / static_cast<float>(total_ring_count);

            auto center_y = 0.0f;
            auto phi_angle = 0.0f;

            if (ring_index <= ring_count)
            {
                // Top hemisphere: phi goes from 0 to pi/2
                const auto hemisphere_fraction = static_cast<float>(ring_index) / static_cast<float>(ring_count);
                phi_angle = hemisphere_fraction * half_pi_constant;
                center_y = half_cylinder_height;
            }
            else
            {
                // Bottom hemisphere: phi goes from pi/2 to pi
                const auto hemisphere_fraction =
                    static_cast<float>(ring_index - ring_count - 1) / static_cast<float>(ring_count);
                phi_angle = half_pi_constant + hemisphere_fraction * half_pi_constant;
                center_y = -half_cylinder_height;
            }

            const auto sin_phi = math::sin(phi_angle);
            const auto cos_phi = math::cos(phi_angle);
            const auto ring_radius = radius * sin_phi;
            const auto y_position = center_y + radius * cos_phi;

            for (uint32_t sector_index = 0; sector_index <= sector_count; ++sector_index)
            {
                const auto horizontal_fraction = static_cast<float>(sector_index) / static_cast<float>(sector_count);
                const auto theta_angle = horizontal_fraction * 2.0f * pi_constant;
                const auto sin_theta = math::sin(theta_angle);
                const auto cos_theta = math::cos(theta_angle);

                const auto x_position = ring_radius * sin_theta;
                const auto z_position = ring_radius * cos_theta;

                mesh_data.vertices.push_back(core::vertex{
                    .position = {x_position, y_position, z_position},
                    .uv = {horizontal_fraction, ring_fraction},
                    .normal = {sin_phi * sin_theta, cos_phi, sin_phi * cos_theta},
                    .tangent = {cos_theta, 0.0f, -sin_theta, 1.0f},
                    .color = {1.0f, 1.0f, 1.0f, 1.0f},
                });
            }
        }

        for (uint32_t ring_index = 0; ring_index < total_ring_count; ++ring_index)
        {
            for (uint32_t sector_index = 0; sector_index < sector_count; ++sector_index)
            {
                const auto current_row_first = ring_index * (sector_count + 1) + sector_index;
                const auto next_row_first = (ring_index + 1) * (sector_count + 1) + sector_index;

                mesh_data.indices.push_back(current_row_first);
                mesh_data.indices.push_back(next_row_first);
                mesh_data.indices.push_back(current_row_first + 1);

                mesh_data.indices.push_back(current_row_first + 1);
                mesh_data.indices.push_back(next_row_first);
                mesh_data.indices.push_back(next_row_first + 1);
            }
        }

        mesh_data.has_normals = true;
        mesh_data.has_tangents = true;
        mesh_data.has_colors = true;
        return mesh_data;
    }

    // =========================================================================
    // Canonical Texture Generators
    // =========================================================================

    auto create_missing_texture() -> core::texture
    {
        auto texture_data = core::texture{};
        texture_data.name = "missing";
        texture_data.width = 2;
        texture_data.height = 2;
        texture_data.format = core::texture_format::rgba8_unorm;
        texture_data.compression = core::texture_compression::none;

        texture_data.sampler.mag_filter = core::magnify_texture_filter::nearest;
        texture_data.sampler.min_filter = core::minify_texture_filter::nearest;
        texture_data.sampler.wrap_s = core::texture_wrap_mode::repeat;
        texture_data.sampler.wrap_t = core::texture_wrap_mode::repeat;

        auto mip_info = core::texture_mip_data{};
        mip_info.width = 2;
        mip_info.height = 2;
        mip_info.data.resize(16);

        // 2x2 Checkerboard: (0,0) Magenta, (1,0) Black, (0,1) Black, (1,1) Magenta
        // Pixel (0,0): Magenta
        mip_info.data[0] = static_cast<byte>(0xFF);
        mip_info.data[1] = static_cast<byte>(0x00);
        mip_info.data[2] = static_cast<byte>(0xFF);
        mip_info.data[3] = static_cast<byte>(0xFF);

        // Pixel (1,0): Black
        mip_info.data[4] = static_cast<byte>(0x00);
        mip_info.data[5] = static_cast<byte>(0x00);
        mip_info.data[6] = static_cast<byte>(0x00);
        mip_info.data[7] = static_cast<byte>(0xFF);

        // Pixel (0,1): Black
        mip_info.data[8] = static_cast<byte>(0x00);
        mip_info.data[9] = static_cast<byte>(0x00);
        mip_info.data[10] = static_cast<byte>(0x00);
        mip_info.data[11] = static_cast<byte>(0xFF);

        // Pixel (1,1): Magenta
        mip_info.data[12] = static_cast<byte>(0xFF);
        mip_info.data[13] = static_cast<byte>(0x00);
        mip_info.data[14] = static_cast<byte>(0xFF);
        mip_info.data[15] = static_cast<byte>(0xFF);

        texture_data.mips.push_back(tempest::move(mip_info));
        return texture_data;
    }

    auto create_white_texture() -> core::texture
    {
        auto texture_data = core::texture{};
        texture_data.name = "white";
        texture_data.width = 1;
        texture_data.height = 1;
        texture_data.format = core::texture_format::rgba8_unorm;
        texture_data.compression = core::texture_compression::none;

        texture_data.sampler.mag_filter = core::magnify_texture_filter::linear;
        texture_data.sampler.min_filter = core::minify_texture_filter::linear;
        texture_data.sampler.wrap_s = core::texture_wrap_mode::repeat;
        texture_data.sampler.wrap_t = core::texture_wrap_mode::repeat;

        auto mip_info = core::texture_mip_data{};
        mip_info.width = 1;
        mip_info.height = 1;
        mip_info.data.resize(4);

        mip_info.data[0] = static_cast<byte>(0xFF);
        mip_info.data[1] = static_cast<byte>(0xFF);
        mip_info.data[2] = static_cast<byte>(0xFF);
        mip_info.data[3] = static_cast<byte>(0xFF);

        texture_data.mips.push_back(tempest::move(mip_info));
        return texture_data;
    }

    auto create_normal_flat_texture() -> core::texture
    {
        auto texture_data = core::texture{};
        texture_data.name = "normal_flat";
        texture_data.width = 1;
        texture_data.height = 1;
        texture_data.format = core::texture_format::rgba8_unorm;
        texture_data.compression = core::texture_compression::none;

        texture_data.sampler.mag_filter = core::magnify_texture_filter::linear;
        texture_data.sampler.min_filter = core::minify_texture_filter::linear;
        texture_data.sampler.wrap_s = core::texture_wrap_mode::repeat;
        texture_data.sampler.wrap_t = core::texture_wrap_mode::repeat;

        auto mip_info = core::texture_mip_data{};
        mip_info.width = 1;
        mip_info.height = 1;
        mip_info.data.resize(4);

        // RGB (128, 128, 255, 255)
        mip_info.data[0] = static_cast<byte>(0x80);
        mip_info.data[1] = static_cast<byte>(0x80);
        mip_info.data[2] = static_cast<byte>(0xFF);
        mip_info.data[3] = static_cast<byte>(0xFF);

        texture_data.mips.push_back(tempest::move(mip_info));
        return texture_data;
    }

    // =========================================================================
    // Canonical Material Generators
    // =========================================================================

    auto create_default_material() -> core::material
    {
        auto material_data = core::material{};
        material_data.set_name("default");
        material_data.set_string("t__shading_model", "openpbr");
        material_data.set_vec4(core::material::base_color_factor_name, math::vec4<float>{1.0f, 1.0f, 1.0f, 1.0f});
        material_data.set_scalar(core::material::roughness_factor_name, 0.5f);
        material_data.set_scalar(core::material::metallic_factor_name, 0.0f);
        material_data.set_scalar(core::material::transmissive_factor_name, 0.0f);
        material_data.set_vec3(core::material::emissive_factor_name, math::vec3<float>{0.0f, 0.0f, 0.0f});
        material_data.set_scalar(core::material::normal_scale_name, 1.0f);
        material_data.set_scalar(core::material::occlusion_strength_name, 1.0f);
        material_data.set_scalar(core::material::alpha_cutoff_name, 0.5f);
        material_data.set_string(core::material::alpha_mode_name, "opaque");
        material_data.set_bool(core::material::double_sided_name, false);

        material_data.set_texture(core::material::base_color_texture_name, white_texture_guid);
        material_data.set_texture(core::material::normal_texture_name, normal_flat_texture_guid);

        return material_data;
    }

    auto create_missing_material() -> core::material
    {
        auto material_data = core::material{};
        material_data.set_name("missing");
        material_data.set_string("t__shading_model", "openpbr");
        material_data.set_vec4(core::material::base_color_factor_name, math::vec4<float>{1.0f, 0.0f, 1.0f, 1.0f});
        material_data.set_scalar(core::material::roughness_factor_name, 0.5f);
        material_data.set_scalar(core::material::metallic_factor_name, 0.0f);
        material_data.set_scalar(core::material::transmissive_factor_name, 0.0f);
        material_data.set_vec3(core::material::emissive_factor_name, math::vec3<float>{0.0f, 0.0f, 0.0f});
        material_data.set_scalar(core::material::normal_scale_name, 1.0f);
        material_data.set_scalar(core::material::occlusion_strength_name, 1.0f);
        material_data.set_scalar(core::material::alpha_cutoff_name, 0.5f);
        material_data.set_string(core::material::alpha_mode_name, "opaque");
        material_data.set_bool(core::material::double_sided_name, false);

        material_data.set_texture(core::material::base_color_texture_name, missing_texture_guid);

        return material_data;
    }

    // =========================================================================
    // Registration
    // =========================================================================

    auto register_builtin_assets(asset_database& database, core::mesh_registry* mesh_reg,
                                 core::texture_registry* texture_reg,
                                 core::material_registry* material_reg) -> void
    {
        database.mount_root("@builtin", 1000, "builtin");

        auto store_and_register_mesh = [&](const guid& asset_id, string_view source_path, core::mesh&& mesh_obj) {
            serialization::binary_archive archive;
            serialization::serializer<serialization::binary_archive, core::mesh>::serialize(archive, mesh_obj);
            const auto blob = archive.read(archive.written_size());

            if (database.find_by_guid(asset_id) == nullptr)
            {
                database.register_asset_with_guid(asset_id, asset_type_id::of<core::mesh>(), source_path);
                database.store_blob(asset_id, blob);
            }

            if (mesh_reg != nullptr && !mesh_reg->find(asset_id).has_value())
            {
                [[maybe_unused]] const auto registered =
                    mesh_reg->register_mesh_with_id(asset_id, tempest::move(mesh_obj));
            }
        };

        auto store_and_register_texture = [&](const guid& asset_id, string_view source_path, core::texture&& texture_obj) {
            serialization::binary_archive archive;
            serialization::serializer<serialization::binary_archive, core::texture>::serialize(archive, texture_obj);
            const auto blob = archive.read(archive.written_size());

            if (database.find_by_guid(asset_id) == nullptr)
            {
                database.register_asset_with_guid(asset_id, asset_type_id::of<core::texture>(), source_path);
                database.store_blob(asset_id, blob);
            }

            if (texture_reg != nullptr && !texture_reg->get_texture(asset_id).has_value())
            {
                [[maybe_unused]] const auto registered =
                    texture_reg->register_texture_with_id(asset_id, tempest::move(texture_obj));
            }
        };

        auto store_and_register_material = [&](const guid& asset_id, string_view source_path, core::material&& material_obj) {
            serialization::binary_archive archive;
            serialization::serializer<serialization::binary_archive, core::material>::serialize(archive, material_obj);
            const auto blob = archive.read(archive.written_size());

            if (database.find_by_guid(asset_id) == nullptr)
            {
                database.register_asset_with_guid(asset_id, asset_type_id::of<core::material>(), source_path);
                database.store_blob(asset_id, blob);
            }

            if (material_reg != nullptr && !material_reg->find(asset_id).has_value())
            {
                [[maybe_unused]] const auto registered =
                    material_reg->register_material_with_id(asset_id, tempest::move(material_obj));
            }
        };

        // Meshes
        store_and_register_mesh(plane_mesh_guid, "@builtin/meshes/plane", create_plane_mesh());
        store_and_register_mesh(box_mesh_guid, "@builtin/meshes/box", create_box_mesh());
        store_and_register_mesh(sphere_mesh_guid, "@builtin/meshes/sphere", create_sphere_mesh());
        store_and_register_mesh(capsule_mesh_guid, "@builtin/meshes/capsule", create_capsule_mesh());
        store_and_register_mesh(cylinder_mesh_guid, "@builtin/meshes/cylinder", create_cylinder_mesh());

        // Textures
        store_and_register_texture(missing_texture_guid, "@builtin/textures/missing", create_missing_texture());
        store_and_register_texture(white_texture_guid, "@builtin/textures/white", create_white_texture());
        store_and_register_texture(normal_flat_texture_guid, "@builtin/textures/normal_flat", create_normal_flat_texture());

        // Materials
        store_and_register_material(default_material_guid, "@builtin/materials/default", create_default_material());
        store_and_register_material(missing_material_guid, "@builtin/materials/missing", create_missing_material());
    }
} // namespace tempest::assets::builtin
