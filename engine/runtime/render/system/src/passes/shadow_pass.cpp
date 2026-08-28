#include <tempest/render_system/passes/shadow_pass.hpp>

#include <cmath>
#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/limits.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_system
{
    namespace
    {
        struct shadow_depth_push_constants
        {
            math::mat4<float> light_view_proj{1.0F};
            uint64_t objects_address{0};
            uint64_t instance_indices_address{0};
            int32_t linear_sampler_index{0};
            uint32_t padding{0};
        };

        struct cascade_render_info
        {
            math::mat4<float> view_proj{1.0F};
            viewport_rect rect{};
        };

        struct resolved_sun
        {
            math::vec3<float> direction{0.0F, -1.0F, 0.0F};
            shadow_caster_component caster{};
            bool has_light{false};
        };

        auto resolve_sun(const ecs::archetype_registry& registry) -> resolved_sun
        {
            auto result = resolved_sun{};
            auto best_priority = numeric_limits<uint32_t>::max();

            for (const auto [self, dl, tx] :
                 registry.with<ecs::self_component, directional_light_component, ecs::transform_component>())
            {
                const auto rot = math::quat(tx.rotation());
                const auto forward = math::extract_forward(rot);
                const auto* sc = registry.try_get<shadow_caster_component>(self.entity);
                const auto priority = sc ? sc->priority : 0U;

                if (!result.has_light || priority < best_priority)
                {
                    result.has_light = true;
                    result.direction = math::normalize(math::vec3<float>{forward.x, forward.y, forward.z});
                    result.caster = sc ? *sc : shadow_caster_component{};
                    best_priority = priority;
                }
            }

            return result;
        }
    } // namespace

    auto add_shadow_pass(shadow_pass_params params) -> shadow_pass_result
    {
        auto shadow_data = directional_shadow_data{};
        shadow_data.cascade_count = 0;

        const auto sun = resolve_sun(params.registry);
        const auto cam_opt = params.camera_override.has_value()
                                 ? params.camera_override
                                 : (params.camera_sys ? params.camera_sys->get_active_camera() : tempest::nullopt);

        if (!sun.has_light || !cam_opt.has_value())
        {
            return shadow_pass_result{
                .shadow_data = shadow_data,
                .shadow_atlas = params.shadow_atlas,
            };
        }

        auto near_plane = 0.1F;
        auto fov_y = 1.0F;
        auto aspect = 16.0F / 9.0F;

        if (!params.camera_override.has_value() && params.camera_sys)
        {
            const auto cam_ent_opt = params.camera_sys->get_active_camera_entity();
            if (cam_ent_opt.has_value())
            {
                if (const auto* cam_comp = params.registry.try_get<camera_component>(*cam_ent_opt))
                {
                    near_plane = cam_comp->near_plane;
                    fov_y = cam_comp->vertical_fov;
                    aspect = cam_comp->aspect_ratio;
                }
            }
        }
        else
        {
            const auto f = std::abs(cam_opt->proj[1][1]);
            if (f > 1e-4F)
            {
                fov_y = 2.0F * std::atan(1.0F / f);
                if (cam_opt->proj[0][0] > 0.0F)
                {
                    aspect = f / cam_opt->proj[0][0];
                }
            }
            if (cam_opt->proj[3][2] > 0.0F)
            {
                near_plane = cam_opt->proj[3][2];
            }
        }

        const auto& cam = *cam_opt;
        const auto cam_eye = math::vec3<float>{cam.eye_position.x, cam.eye_position.y, cam.eye_position.z};
        auto cam_forward =
            math::normalize(math::vec3<float>{-cam.inv_view[2][0], -cam.inv_view[2][1], -cam.inv_view[2][2]});
        if (!params.camera_override.has_value() && params.camera_sys)
        {
            const auto cam_ent_opt = params.camera_sys->get_active_camera_entity();
            if (cam_ent_opt.has_value())
            {
                if (const auto* cam_tx = params.registry.try_get<ecs::transform_component>(*cam_ent_opt))
                {
                    cam_forward = math::extract_forward(math::quat(cam_tx->rotation()));
                }
            }
        }

        const auto num_cascades = math::clamp(sun.caster.num_cascades, 1U, 4U);
        const auto lambda = math::clamp(sun.caster.split_lambda, 0.0F, 1.0F);
        const auto z_near = tempest::max(0.01F, near_plane);
        const auto z_far = tempest::max(z_near + 1.0F, sun.caster.max_shadow_distance);

        auto splits = array<float, 5>{};
        splits[0] = z_near;
        for (auto i = 1U; i < num_cascades; ++i)
        {
            const auto fi = static_cast<float>(i);
            const auto f_num = static_cast<float>(num_cascades);
            const auto log_split = z_near * std::pow(z_far / z_near, fi / f_num);
            const auto lin_split = z_near + (fi / f_num) * (z_far - z_near);
            splits[i] = lambda * log_split + (1.0F - lambda) * lin_split;
        }
        splits[num_cascades] = z_far;

        const auto tan_half_fov = std::tan(fov_y * 0.5F);
        const auto alpha = tan_half_fov * std::sqrt(1.0F + aspect * aspect);
        const auto alpha_sq = alpha * alpha;

        auto rendered_cascades = vector<cascade_render_info>{};
        rendered_cascades.reserve(num_cascades);

        for (auto i = 0U; i < num_cascades; ++i)
        {
            const auto z0 = splits[i];
            const auto z1 = splits[i + 1];

            auto cz = 0.5F * (z0 + z1) * (1.0F + alpha_sq);
            if (cz > z1)
            {
                cz = z1;
            }

            const auto radius = std::sqrt((z1 - cz) * (z1 - cz) + (z1 * alpha) * (z1 * alpha));
            const auto sphere_center_world = cam_eye + cam_forward * cz;

            const auto sun_dir = sun.direction;
            auto light_up = math::vec3<float>{0.0F, 1.0F, 0.0F};
            if (std::abs(math::dot(sun_dir, light_up)) > 0.99F)
            {
                light_up = math::vec3<float>{0.0F, 0.0F, 1.0F};
            }

            const auto light_view_zero =
                math::look_at(-sun_dir * (radius * 2.0F), math::vec3<float>{0.0F, 0.0F, 0.0F}, light_up);
            const auto center_light = light_view_zero * math::vec4<float>{sphere_center_world.x, sphere_center_world.y,
                                                                          sphere_center_world.z, 1.0F};

            const auto cascade_res = sun.caster.resolution;
            const auto world_units_per_texel = (2.0F * radius) / static_cast<float>(cascade_res);
            const auto snapped_x = std::floor(center_light.x / world_units_per_texel) * world_units_per_texel;
            const auto snapped_y = std::floor(center_light.y / world_units_per_texel) * world_units_per_texel;
            const auto delta_x = snapped_x - center_light.x;
            const auto delta_y = snapped_y - center_light.y;

            const auto light_eye = sphere_center_world - sun_dir * (radius * 2.0F);
            const auto light_target = sphere_center_world;
            const auto light_view = math::look_at(light_eye, light_target, light_up);

            const auto z_far_light = radius * 4.0F;
            const auto inv_r = 1.0F / radius;
            const auto inv_zfar = 1.0F / z_far_light;

            const auto ortho_proj =
                math::mat4<float>{inv_r, 0.0F, 0.0F, 0.0F,     0.0F, -inv_r,           0.0F,
                                  0.0F,  0.0F, 0.0F, inv_zfar, 0.0F, -delta_x * inv_r, delta_y * inv_r,
                                  1.0F,  1.0F};

            const auto light_view_proj = ortho_proj * light_view;

            const auto alloc_res = params.allocator.allocate(cascade_res, cascade_res);
            if (!alloc_res.has_value())
            {
                break;
            }

            const auto rect = *alloc_res;
            const auto atlas_w = static_cast<float>(params.allocator.get_atlas_width());
            const auto atlas_h = static_cast<float>(params.allocator.get_atlas_height());

            shadow_data.cascades[i] = shadow_cascade_data{
                .view_proj = light_view_proj,
                .uv_offset_scale =
                    math::vec4<float>{
                        static_cast<float>(rect.x) / atlas_w,
                        static_cast<float>(rect.y) / atlas_h,
                        static_cast<float>(rect.width) / atlas_w,
                        static_cast<float>(rect.height) / atlas_h,
                    },
                .split_depth = z1,
                .padding = {0.0F, 0.0F, 0.0F},
            };

            rendered_cascades.push_back(cascade_render_info{
                .view_proj = light_view_proj,
                .rect = rect,
            });
        }

        shadow_data.cascade_count = static_cast<uint32_t>(rendered_cascades.size());
        shadow_data.normal_bias = sun.caster.normal_bias;
        shadow_data.depth_bias = sun.caster.depth_bias;
        shadow_data.debug_mode = static_cast<uint32_t>(sun.caster.debug_mode);

        auto pipe_h = params.shaders.find_graphics_pipeline("shadow_depth_pipeline");
        if (!pipe_h.has_value())
        {
            auto vs =
                params.shaders.register_shader_module("shadow_depth.vert.spv", rhi::shader_stage::vertex, "VSMain");
            auto fs =
                params.shaders.register_shader_module("shadow_depth.frag.spv", rhi::shader_stage::fragment, "FSMain");
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
                        .depth_bias =
                            rhi::depth_bias_state{
                                .constant_factor = 1.25F,
                                .clamp = 0.0F,
                                .slope_factor = 1.75F,
                            },
                    },
                .depth_stencil_state =
                    {
                        .depth_test_enable = true,
                        .depth_write_enable = true,
                        .depth_compare_op = rhi::compare_op::greater_or_equal,
                    },
            };
            pipe_h = params.shaders.register_graphics_pipeline("shadow_depth_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        const auto& pass_data = params.graph.add_graphics_pass<shadow_pass_data>(
            "ShadowPass",
            [&pool = params.pool, shadow_atlas_tex = params.shadow_atlas](render_graph::pass_builder& builder,
                                                                          shadow_pass_data& data) {
                data.shadow_atlas = builder.set_depth_stencil_attachment(render_graph::rg_depth_stencil_attachment{
                    .texture = shadow_atlas_tex,
                    .depth_load_op = rhi::load_op::clear,
                    .depth_store_op = rhi::store_op::store,
                    .clear_value = {.depth = 0.0F, .stencil = 0},
                });

                auto obj_buf = builder.import_buffer(pool.get_object_buffer());
                auto inst_buf = builder.import_buffer(pool.get_instance_buffer());
                auto cmd_buf = builder.import_buffer(pool.get_draw_commands_buffer());
                auto vtx_buf = builder.import_buffer(pool.get_vertex_buffer());

                builder.read(obj_buf, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                builder.read(inst_buf, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                builder.read(cmd_buf, rhi::pipeline_stage::indirect_commands, rhi::resource_access::read);
                builder.read(vtx_buf, rhi::pipeline_stage::vertex, rhi::resource_access::read);
            },
            [&pool = params.pool, &shaders = params.shaders, draw_count = params.draw_count,
             draw_offset = params.draw_offset, pipe, cascades_to_render = rendered_cascades](
                [[maybe_unused]] const shadow_pass_data& data,
                [[maybe_unused]] render_graph::pass_execution_context& ctx, rhi::command_list& pass_cmd) {
                if (draw_count == 0 || cascades_to_render.empty())
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
                pass_cmd.set_depth_bias(1.25F, 0.0F, 1.75F);

                const auto linear_sampler_idx = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index);
                const auto objects_addr = pool.get_object_buffer_address();
                const auto instances_addr = pool.get_instance_buffer_address();

                for (const auto& cascade : cascades_to_render)
                {
                    pass_cmd.set_viewport(static_cast<float>(cascade.rect.x), static_cast<float>(cascade.rect.y),
                                          static_cast<float>(cascade.rect.width),
                                          static_cast<float>(cascade.rect.height), 0.0F, 1.0F);

                    pass_cmd.set_scissor(static_cast<int32_t>(cascade.rect.x), static_cast<int32_t>(cascade.rect.y),
                                         cascade.rect.width, cascade.rect.height);

                    const auto push_constants = shadow_depth_push_constants{
                        .light_view_proj = cascade.view_proj,
                        .objects_address = objects_addr,
                        .instance_indices_address = instances_addr,
                        .linear_sampler_index = linear_sampler_idx,
                        .padding = 0,
                    };

                    pass_cmd.push_constants(
                        rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                        span<const byte>{reinterpret_cast<const byte*>(&push_constants), sizeof(push_constants)});

                    const auto byte_offset = pool.get_draw_commands_buffer_offset() +
                                             static_cast<uint64_t>(draw_offset) * sizeof(indexed_indirect_command);
                    pass_cmd.draw_indexed_indirect(pool.get_draw_commands_buffer(), byte_offset, draw_count,
                                                   sizeof(indexed_indirect_command));
                }
            });

        return shadow_pass_result{
            .shadow_data = shadow_data,
            .shadow_atlas = pass_data.shadow_atlas,
        };
    }
} // namespace tempest::render_system
