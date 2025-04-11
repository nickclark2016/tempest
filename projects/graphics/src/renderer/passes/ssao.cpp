#include <tempest/passes/ssao.hpp>

#include <tempest/files.hpp>

#include <random>

namespace tempest::graphics::passes
{
    bool ssao_pass::init(render_device& device)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/ssao.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/ssao.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            ssao_pass::scene_constants_buffer_desc.to_binding_info(), ssao_pass::depth_image_desc.to_binding_info(),
            ssao_pass::normal_image_desc.to_binding_info(),           ssao_pass::noise_image_desc.to_binding_info(),
            ssao_pass::linear_sampler_desc.to_binding_info(),         ssao_pass::point_sampler_desc.to_binding_info(),
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        color_blend_attachment_state blending[] = {
            {
                // SSAO Visibility Buffer
                .enabled = false,
                .color = {},
                .alpha = {},
            },
        };

        resource_format color_formats[] = {resource_format::R16_FLOAT};
        auto pipeline = device.create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_formats,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "SSAO Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "SSAO Fragment Shader Module",
            },
            .depth_testing{
                .enable_test = true,
                .enable_write = false,
                .depth_test_op = compare_operation::LESS,
            },
            .blending{
                // Disable blending for the SSAO pass
                .attachment_blend_ops = blending,
            },
            .name = "SSAO Graphics Pipeline",
        });
        _pipeline = pipeline;

        {
            uint32_t width = noise_size;
            uint32_t height = noise_size;
            tempest::vector<byte> noise_data;
            noise_data.resize(sizeof(float) * 2 * width * height);

            // Generate random noise for SSAO
            std::random_device rd{};
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);

            for (size_t i = 0; i < width * height; ++i)
            {
                float r = dist(gen) * 2.0f - 1.0f;
                float g = dist(gen) * 2.0f - 1.0f;

                // Store the noise in RG format (2 floats)
                auto offset = i * 2 * sizeof(float); // 2 floats per pixel (R and G)

                tempest::memmove(&noise_data[offset + 0], &r, sizeof(float)); // R
                tempest::memmove(&noise_data[offset + 4], &g, sizeof(float)); // G
            }

            // Create the noise image
            texture_mip_descriptor noise_mip_desc{
                .width = width,
                .height = height,
                .bytes = span<byte>(noise_data.data(), noise_data.size()),
            };

            texture_data_descriptor noise_texture_desc{
                .fmt = resource_format::RG32_FLOAT, // Use RGBA8 for the noise texture
                .mips = {},
                .name = "SSAO Noise Texture",
            };

            noise_texture_desc.mips.push_back(noise_mip_desc);

            span<texture_data_descriptor> noise_texture_span(&noise_texture_desc, 1);

            auto textures = renderer_utilities::upload_textures(device, noise_texture_span, device.get_staging_buffer(),
                                                                false, false);
            _noise_image = textures[0];
        }

        {
            auto lerp = [](auto a, auto b, float t) { return a + t * (b - a); };

            std::default_random_engine engine{0};
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);

            for (size_t i = 0; i < 64; ++i)
            {
                float x = dist(engine) * 2.0f - 1.0f;
                float y = dist(engine) * 2.0f - 1.0f;
                float z = dist(engine);

                auto sample = math::normalize(math::vec3<float>(x, y, z));
                sample *= dist(engine);

                auto scale = static_cast<float>(i) / 64.0f;
                scale = lerp(0.1f, 1.0f, scale * scale);
                sample *= scale;

                _kernel[i] = math::vec4<float>(sample.x, sample.y, sample.z, 1.0f);
            }
        }

        return _pipeline != graphics_pipeline_resource_handle{};
    }

    bool ssao_pass::draw_batch([[maybe_unused]] render_device& device, command_list& cmds) const
    {
        cmds.set_cull_mode(false, true)
            .use_pipeline(_pipeline)
            .draw(3, 1, 0, 0); // Draw a single triangle for the SSAO pass

        return true;
    }

    void ssao_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_graphics_pipeline(_pipeline);
        }
    }

    bool ssao_blur_pass::init(render_device& device)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/ssao_blur.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/ssao_blur.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            ssao_blur_pass::ssao_image_desc.to_binding_info(),
            ssao_blur_pass::point_sampler_desc.to_binding_info(),
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        color_blend_attachment_state blending[] = {
            {
                // SSAO Blur Output
                .enabled = false,
                .color = {},
                .alpha = {},
            },
        };

        resource_format color_formats[] = {resource_format::R16_FLOAT}; // Output format for SSAO blur

        auto pipeline = device.create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_formats,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "SSAO Blur Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "SSAO Blur Fragment Shader Module",
            },
            .depth_testing{
                .enable_test = false,
                .enable_write = false,
                .depth_test_op = compare_operation::NEVER,
            },
            .blending{
                // Disable blending for the SSAO blur pass
                .attachment_blend_ops = blending,
            },
            .name = "SSAO Blur Graphics Pipeline",
        });

        _pipeline = pipeline;
        return pipeline != graphics_pipeline_resource_handle{};
    }

    bool ssao_blur_pass::draw_batch([[maybe_unused]] render_device& device, command_list& cmds) const
    {
        cmds.set_cull_mode(false, true)
            .use_pipeline(_pipeline)
            .draw(3, 1, 0, 0); // Draw a single triangle for the SSAO pass

        return true;
    }

    void ssao_blur_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_graphics_pipeline(_pipeline);
        }
    }
} // namespace tempest::graphics::passes