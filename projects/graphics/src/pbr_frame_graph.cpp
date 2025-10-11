#include "tempest/frame_graph.hpp"

#include <tempest/archetype.hpp>
#include <tempest/enum.hpp>
#include <tempest/files.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/mat4.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>

namespace tempest::graphics
{
    namespace
    {
        struct camera_payload
        {
            math::mat4<float> proj;
            math::mat4<float> inv_proj;
            math::mat4<float> view;
            math::mat4<float> inv_view;
            math::vec3<float> position;
        };

        struct object_payload
        {
            math::mat4<float> model_matrix;
            math::mat4<float> normal_matrix;

            uint32_t mesh_index;
            uint32_t material_index;
            uint32_t parent_index;
            uint32_t self_index;
        };

        auto create_per_frame_upload_pass(graph_builder& builder, rhi::device* device,
                                          const pbr_frame_graph_inputs& inputs, uint32_t max_objects)
        {
            auto object_buffer = builder.create_per_frame_buffer({
                .size = sizeof(object_payload) * max_objects,
                .location = rhi::memory_location::device,
                .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
                .access_type = rhi::host_access_type::none,
                .access_pattern = rhi::host_access_pattern::none,
                .name = "Object Buffer",
            });

            builder.create_transfer_pass(
                "Upload Data",
                [&](transfer_task_builder& builder) {
                    builder.write(object_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                                  make_enum_mask(rhi::memory_access::shader_read));
                },
                [](transfer_task_execution_context& ctx, auto object_buffer, ecs::archetype_registry* entities) {
                    entities->each([]([[maybe_unused]] ecs::self_component self,
                                      [[maybe_unused]] const ecs::transform_component& tx,
                                      [[maybe_unused]] const renderable_component& renderable) {

                    });
                },
                object_buffer, inputs.entity_registry);

            return make_tuple(object_buffer);
        }

        auto create_depth_prepass(graph_builder& builder, rhi::device* device, const pbr_frame_graph_config& cfg,
                                  graph_resource_handle<rhi::rhi_handle_type::buffer> objects)
        {
            struct scene_constants
            {
                camera_payload camera;
                math::vec2<float> screen_size;
            };

            constexpr auto encoded_normal_format = rhi::image_format::rg16_float;

            auto depth_buffer = builder.create_render_target({
                .format = cfg.depth_format,
                .type = rhi::image_type::image_2d,
                .width = cfg.render_target_width,
                .height = cfg.render_target_height,
                .depth = 1,
                .array_layers = 1,
                .mip_levels = 1,
                .sample_count = rhi::image_sample_count::sample_count_1,
                .tiling = rhi::image_tiling_type::optimal,
                .location = rhi::memory_location::device,
                .usage = make_enum_mask(rhi::image_usage::depth_attachment, rhi::image_usage::sampled),
                .name = "Depth Target",
            });

            auto encoded_normals = builder.create_render_target({
                .format = encoded_normal_format,
                .type = rhi::image_type::image_2d,
                .width = cfg.render_target_width,
                .height = cfg.render_target_height,
                .depth = 1,
                .array_layers = 1,
                .mip_levels = 1,
                .sample_count = rhi::image_sample_count::sample_count_1,
                .tiling = rhi::image_tiling_type::optimal,
                .location = rhi::memory_location::device,
                .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled),
                .name = "Encoded Normals Target",
            });

            auto scene_set_bindings = vector<rhi::descriptor_binding_layout>();

            // Scene Constants
            scene_set_bindings.push_back({
                .binding_index = 0,
                .type = rhi::descriptor_type::constant_buffer,
                .count = 1,
                .stages = make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            });

            // Vertex Pull Buffer
            scene_set_bindings.push_back({
                .binding_index = 1,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = make_enum_mask(rhi::shader_stage::vertex),
            });

            // Mesh Buffer
            scene_set_bindings.push_back({
                .binding_index = 2,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = make_enum_mask(rhi::shader_stage::vertex),
            });

            // Object Buffer
            scene_set_bindings.push_back({
                .binding_index = 3,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = make_enum_mask(rhi::shader_stage::vertex),
            });

            // Instance Buffer
            scene_set_bindings.push_back({
                .binding_index = 4,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = make_enum_mask(rhi::shader_stage::vertex),
            });

            // Material Buffer
            scene_set_bindings.push_back({
                .binding_index = 5,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = make_enum_mask(rhi::shader_stage::fragment),
            });

            // Linear Sampler
            scene_set_bindings.push_back({
                .binding_index = 15,
                .type = rhi::descriptor_type::sampler,
                .count = 1,
                .stages = make_enum_mask(rhi::shader_stage::fragment),
            });

            // Bindless Textures
            scene_set_bindings.push_back({.binding_index = 16,
                                          .type = rhi::descriptor_type::sampled_image,
                                          .count = 512,
                                          .stages = make_enum_mask(rhi::shader_stage::fragment),
                                          .flags = make_enum_mask(rhi::descriptor_binding_flags::partially_bound)});

            auto scene_set_layout = device->create_descriptor_set_layout(scene_set_bindings);

            auto set_layouts = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
            set_layouts.push_back(scene_set_layout);

            auto scene_pipeline_layout = device->create_pipeline_layout({
                .descriptor_set_layouts = set_layouts,
                .push_constants = {},
            });

            auto vert_source = core::read_bytes("assets/shaders/zprepass.vert.spv");
            auto frag_source = core::read_bytes("assets/shaders/zprepass.frag.spv");

            const auto color_formats = array{encoded_normal_format};

            const auto encoded_normal_blend = rhi::color_blend_attachment{
                .blend_enable = false,
                .src_color_blend_factor = rhi::blend_factor::one,
                .dst_color_blend_factor = rhi::blend_factor::zero,
                .color_blend_op = rhi::blend_op::add,
                .src_alpha_blend_factor = rhi::blend_factor::one,
                .dst_alpha_blend_factor = rhi::blend_factor::zero,
                .alpha_blend_op = rhi::blend_op::add,
            };

            const auto blend = array{
                encoded_normal_blend,
            };

            auto pipeline_desc = rhi::graphics_pipeline_desc{
                .color_attachment_formats = vector(color_formats.begin(), color_formats.end()),
                .depth_attachment_format = cfg.depth_format,
                .stencil_attachment_format = none(),
                .vertex_shader = tempest::move(vert_source),
                .tessellation_control_shader = {},
                .tessellation_evaluation_shader = {},
                .geometry_shader = {},
                .fragment_shader = tempest::move(frag_source),
                .input_assembly =
                    {
                        .topology = rhi::primitive_topology::triangle_list,
                    },
                .vertex_input = none(),
                .tessellation = none(),
                .multisample =
                    {
                        .sample_count = rhi::image_sample_count::sample_count_1,
                        .sample_shading = none(),
                        .alpha_to_coverage = false,
                        .alpha_to_one = false,
                    },
                .rasterization =
                    {
                        .depth_clamp_enable = false,
                        .rasterizer_discard_enable = false,
                        .polygon_mode = rhi::polygon_mode::fill,
                        .cull_mode = make_enum_mask(rhi::cull_mode::back),
                        .vertex_winding = rhi::vertex_winding::counter_clockwise,
                        .depth_bias = none(),
                        .line_width = 1.0f,
                    },
                .depth_stencil =
                    {
                        .depth =
                            rhi::depth_test{
                                .write_enable = true,
                                .compare_op = rhi::compare_op::greater_equal,
                                .depth_bounds_test_enable = false,
                                .min_depth_bounds = 0.0f,
                                .max_depth_bounds = 1.0f,
                            },
                        .stencil = none(),
                    },
                .color_blend =
                    {
                        .attachments = vector(blend.begin(), blend.end()),
                        .blend_constants = {},
                    },
                .layout = scene_pipeline_layout,
                .name = "Z Pre-Pass Pipeline",
            };

            auto z_prepass_pipeline = device->create_graphics_pipeline(pipeline_desc);

            builder.create_graphics_pass(
                "Z Pre-Pass",
                [&](graphics_task_builder& builder) {
                    builder.write(depth_buffer, rhi::image_layout::depth,
                                  make_enum_mask(rhi::pipeline_stage::early_fragment_tests),
                                  make_enum_mask(rhi::memory_access::depth_stencil_attachment_write));
                    builder.write(encoded_normals, rhi::image_layout::color_attachment,
                                  make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                                  make_enum_mask(rhi::memory_access::color_attachment_write));
                    builder.read(objects, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                                 make_enum_mask(rhi::memory_access::shader_read));
                },
                [](graphics_task_execution_context& ctx, auto z_prepass_pipeline) {

                },
                z_prepass_pipeline);

            return make_tuple(depth_buffer, encoded_normals);
        }
    } // namespace

    pbr_frame_graph_handles create_pbr_frame_graph(graph_builder& graph_builder, rhi::device* device,
                                                   pbr_frame_graph_config cfg, pbr_frame_graph_inputs inputs)
    {
        auto handles = pbr_frame_graph_handles{};

        auto hdr_color_buffer = graph_builder.create_render_target({
            .format = cfg.hdr_color_format,
            .type = rhi::image_type::image_2d,
            .width = cfg.render_target_width,
            .height = cfg.render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled,
                                    rhi::image_usage::transfer_src),
            .name = "HDR Color Target",
        });

        auto tonemap_target = graph_builder.create_render_target({
            .format = cfg.tonemapped_color_format,
            .type = rhi::image_type::image_2d,
            .width = cfg.render_target_width,
            .height = cfg.render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled,
                                    rhi::image_usage::transfer_src, rhi::image_usage::transfer_dst),
            .name = "Tonemapped Color Target",
        });

        auto [object_buffer] = create_per_frame_upload_pass(graph_builder, device, inputs, 1024 * 256);
        auto [depth_buffer, encoded_normals] = create_depth_prepass(graph_builder, device, cfg, object_buffer);

        handles.hdr_color = hdr_color_buffer;
        handles.tonemapped_color = tonemap_target;
        handles.depth = depth_buffer;

        return handles;
    }
} // namespace tempest::graphics