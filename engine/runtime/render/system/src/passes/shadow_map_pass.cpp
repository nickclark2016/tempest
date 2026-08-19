#include <tempest/render_system/passes/shadow_map_pass.hpp>

#include <cmath>
#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/limits.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/transformations.hpp>

namespace tempest::render_system
{
    auto compute_csm_cascades(const math::vec3<float>& light_dir,
                              const render_camera& cam,
                              const shadow_map_component& shadow_cfg,
                              math::vec2<uint32_t> atlas_size)
        -> array<csm_cascade, 4>
    {
        auto cascades = array<csm_cascade, 4>{};
        const auto cascade_count = math::clamp(shadow_cfg.cascade_count, 1U, 4U);

        const auto norm_light_dir = math::normalize(light_dir);
        const auto up = std::abs(norm_light_dir.y) > 0.99F ? math::vec3<float>{0.0F, 0.0F, 1.0F} : math::vec3<float>{0.0F, 1.0F, 0.0F};

        const auto cols = (cascade_count > 1) ? 2U : 1U;
        const auto rows = (cascade_count > 2) ? 2U : 1U;
        const auto cascade_res = math::vec2<uint32_t>{atlas_size.x / cols, atlas_size.y / rows};

        const auto near_plane = 0.1F;
        const auto shadow_dist = shadow_cfg.shadow_distance > 0.0F ? shadow_cfg.shadow_distance : 100.0F;
        const auto clip_ratio = shadow_dist / near_plane;
        const auto clip_range = shadow_dist - near_plane;
        const auto lambda = shadow_cfg.split_lambda > 0.0F ? shadow_cfg.split_lambda : 0.85F;

        auto last_split = near_plane;

        for (auto i = 0U; i < cascade_count; ++i)
        {
            const auto p = static_cast<float>(i + 1) / static_cast<float>(cascade_count);
            const auto log_split = near_plane * std::pow(clip_ratio, p);
            const auto uniform_split = near_plane + clip_range * p;
            const auto split_depth = lambda * log_split + (1.0F - lambda) * uniform_split;

            const auto z_near_ndc = 1.0F - (last_split / shadow_dist);
            const auto z_far_ndc = 1.0F - (split_depth / shadow_dist);

            const auto ndc_corners = array<math::vec4<float>, 8>{
                math::vec4<float>{-1.0F, -1.0F, z_near_ndc, 1.0F},
                math::vec4<float>{ 1.0F, -1.0F, z_near_ndc, 1.0F},
                math::vec4<float>{-1.0F,  1.0F, z_near_ndc, 1.0F},
                math::vec4<float>{ 1.0F,  1.0F, z_near_ndc, 1.0F},
                math::vec4<float>{-1.0F, -1.0F, z_far_ndc, 1.0F},
                math::vec4<float>{ 1.0F, -1.0F, z_far_ndc, 1.0F},
                math::vec4<float>{-1.0F,  1.0F, z_far_ndc, 1.0F},
                math::vec4<float>{ 1.0F,  1.0F, z_far_ndc, 1.0F},
            };

            auto center = math::vec3<float>{0.0F, 0.0F, 0.0F};
            auto ws_corners = array<math::vec3<float>, 8>{};
            for (auto c = 0U; c < 8U; ++c)
            {
                auto ws = cam.inv_view * (cam.inv_proj * ndc_corners[c]);
                ws_corners[c] = math::vec3<float>{ws.x, ws.y, ws.z} / tempest::max(std::abs(ws.w), 1e-6F);
                center += ws_corners[c];
            }
            center /= 8.0F;

            auto radius = 0.0F;
            for (const auto& corner : ws_corners)
            {
                radius = tempest::max(radius, math::norm(corner - center));
            }

            const auto light_pos = center - norm_light_dir * (radius * 2.0F);
            const auto light_view = math::look_at(light_pos, center, up);

            auto min_ext = math::vec3<float>{numeric_limits<float>::max(), numeric_limits<float>::max(), numeric_limits<float>::max()};
            auto max_ext = math::vec3<float>{numeric_limits<float>::lowest(), numeric_limits<float>::lowest(), numeric_limits<float>::lowest()};

            for (const auto& corner : ws_corners)
            {
                const auto ls = light_view * math::vec4<float>{corner.x, corner.y, corner.z, 1.0F};
                min_ext.x = tempest::min(min_ext.x, ls.x);
                min_ext.y = tempest::min(min_ext.y, ls.y);
                min_ext.z = tempest::min(min_ext.z, ls.z);
                max_ext.x = tempest::max(max_ext.x, ls.x);
                max_ext.y = tempest::max(max_ext.y, ls.y);
                max_ext.z = tempest::max(max_ext.z, ls.z);
            }

            const auto z_range = max_ext.z - min_ext.z;
            min_ext.z -= z_range * 1.5F;
            max_ext.z += z_range * 0.1F;

            const auto texel_size_x = (max_ext.x - min_ext.x) / static_cast<float>(cascade_res.x);
            const auto texel_size_y = (max_ext.y - min_ext.y) / static_cast<float>(cascade_res.y);
            min_ext.x = std::floor(min_ext.x / texel_size_x) * texel_size_x;
            min_ext.y = std::floor(min_ext.y / texel_size_y) * texel_size_y;
            max_ext.x = std::floor(max_ext.x / texel_size_x) * texel_size_x;
            max_ext.y = std::floor(max_ext.y / texel_size_y) * texel_size_y;

            const auto light_proj = math::ortho(min_ext.x, max_ext.x, min_ext.y, max_ext.y, min_ext.z, max_ext.z);

            const auto col = i % cols;
            const auto row = i / cols;
            const auto offset_x = static_cast<float>(col * cascade_res.x) / static_cast<float>(atlas_size.x);
            const auto offset_y = static_cast<float>(row * cascade_res.y) / static_cast<float>(atlas_size.y);
            const auto scale_x = static_cast<float>(cascade_res.x) / static_cast<float>(atlas_size.x);
            const auto scale_y = static_cast<float>(cascade_res.y) / static_cast<float>(atlas_size.y);

            cascades[i] = csm_cascade{
                .light_view_projection = light_proj * light_view,
                .atlas_offset = {offset_x, offset_y},
                .atlas_scale = {scale_x, scale_y},
                .split_depth = split_depth,
                .blend_start = 0.9F,
                .texel_size_ws = (max_ext.x - min_ext.x) / static_cast<float>(cascade_res.x),
                .cascade_index = i,
            };

            last_split = split_depth;
        }

        return cascades;
    }

    auto add_shadow_map_pass(render_graph::render_graph& graph, resource_pool& pool,
                             shader_manager& shaders, render_graph::rg_texture_id shadow_atlas_tex,
                             const math::vec3<float>& light_dir,
                             const render_camera& cam,
                             const shadow_map_component& shadow_cfg,
                             math::vec2<uint32_t> atlas_size,
                             uint32_t draw_count)
        -> const shadow_map_pass_data&
    {
        const auto cascade_count = math::clamp(shadow_cfg.cascade_count, 1U, 4U);
        const auto computed_cascades = compute_csm_cascades(light_dir, cam, shadow_cfg, atlas_size);

        auto pipe_h = shaders.find_graphics_pipeline("shadow_map_pipeline");
        if (!pipe_h.has_value())
        {
            auto vs = shaders.register_shader_module("directional_shadow_map.vert.spv", rhi::shader_stage::vertex, "VSMain");
            auto fs = shaders.register_shader_module("directional_shadow_map.frag.spv", rhi::shader_stage::fragment, "FSMain");
            auto stages = array{vs, fs};

            auto tmpl = graphics_pipeline_template{
                .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
                .color_attachment_formats = {},
                .depth_stencil_attachment_format = rhi::data_format::depth32_float,
                .primitive_topology = rhi::primitive_topology::triangle_list,
                .rasterization_state =
                    {
                        .polygon_mode = rhi::polygon_mode::fill,
                        .cull_mode = rhi::cull_mode::none,
                        .front_face = rhi::vertex_winding_order::counter_clockwise,
                    },
                .depth_stencil_state =
                    {
                        .depth_test_enable = true,
                        .depth_write_enable = true,
                        .depth_compare_op = rhi::compare_op::greater,
                    },
            };
            pipe_h = shaders.register_graphics_pipeline("shadow_map_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        return graph.add_graphics_pass<shadow_map_pass_data>(
            "ShadowMapPass",
            [shadow_atlas_tex, &pool, computed_cascades, cascade_count, atlas_size, draw_count](
                render_graph::pass_builder& builder, shadow_map_pass_data& data) {
                data.shadow_atlas = builder.set_depth_stencil_attachment(
                    render_graph::rg_depth_stencil_attachment{
                        .texture = shadow_atlas_tex,
                        .depth_load_op = rhi::load_op::clear,
                        .depth_store_op = rhi::store_op::store,
                        .clear_value = {.depth = 0.0F, .stencil = 0},
                    });

                data.object_buffer = builder.import_buffer(pool.get_object_buffer());
                data.instance_buffer = builder.import_buffer(pool.get_instance_buffer());
                data.draw_commands = builder.import_buffer(pool.get_draw_commands_buffer());

                data.object_buffer = builder.read(data.object_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.instance_buffer = builder.read(data.instance_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.draw_commands = builder.read(data.draw_commands, rhi::pipeline_stage::indirect_commands, rhi::resource_access::read);

                data.draw_count = draw_count;
                data.cascade_count = cascade_count;
                data.cascades = computed_cascades;
                data.atlas_size = atlas_size;
            },
            [&pool, &shaders, pipe](const shadow_map_pass_data& data,
                                   [[maybe_unused]] render_graph::pass_execution_context& ctx,
                                   rhi::command_list& pass_cmd) {
                if (data.draw_count == 0 || data.cascade_count == 0)
                {
                    return;
                }

                auto rhi_pipe = shaders.get_rhi_pipeline(pipe);
                if (rhi_pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipe);
                pass_cmd.bind_index_buffer(pool.get_vertex_buffer(), rhi::index_type::uint32, 0);

                const auto cols = (data.cascade_count > 1) ? 2U : 1U;
                const auto rows = (data.cascade_count > 2) ? 2U : 1U;
                const auto cascade_w = data.atlas_size.x / cols;
                const auto cascade_h = data.atlas_size.y / rows;

                for (auto i = 0U; i < data.cascade_count; ++i)
                {
                    const auto& c = data.cascades[i];
                    const auto vp_x = c.atlas_offset.x * static_cast<float>(data.atlas_size.x);
                    const auto vp_y = c.atlas_offset.y * static_cast<float>(data.atlas_size.y);

                    pass_cmd.set_viewport(vp_x, vp_y, static_cast<float>(cascade_w), static_cast<float>(cascade_h), 0.0F, 1.0F);
                    pass_cmd.set_scissor(static_cast<int32_t>(vp_x), static_cast<int32_t>(vp_y), cascade_w, cascade_h);

                    const auto constants = shadow_map_push_constants{
                        .view_projection = c.light_view_projection,
                        .objects_address = pool.get_object_buffer().gpu_address,
                        .instance_indices_address = pool.get_instance_buffer().gpu_address,
                        .linear_sampler_index = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index),
                        .padding = 0,
                    };

                    pass_cmd.push_constants(rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                                            span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                    pass_cmd.draw_indexed_indirect(pool.get_draw_commands_buffer(), 0, data.draw_count,
                                                   sizeof(indexed_indirect_command));
                }
            });
    }
} // namespace tempest::render_system
