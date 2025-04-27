#include <tempest/pipelines/pbr_pipeline.hpp>

#include <tempest/files.hpp>
#include <tempest/logger.hpp>

namespace tempest::graphics
{
    namespace
    {
        auto log = logger::logger_factory::create({.prefix = {"pbr_pipeline"}});

        namespace zprepass
        {
            rhi::descriptor_binding_layout scene_constants_binding_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::UNIFORM_BUFFER,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::VERTEX),
            };

            rhi::descriptor_binding_layout vertex_pull_buffer_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::STORAGE_BUFFER,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::VERTEX),
            };

            rhi::descriptor_binding_layout mesh_buffer_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::STORAGE_BUFFER,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::VERTEX),
            };

            rhi::descriptor_binding_layout object_buffer_layout = {
                .binding_index = 3,
                .type = rhi::descriptor_type::STORAGE_BUFFER,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::VERTEX),
            };

            rhi::descriptor_binding_layout instance_buffer_layout = {
                .binding_index = 4,
                .type = rhi::descriptor_type::STORAGE_BUFFER,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::VERTEX),
            };

            rhi::descriptor_binding_layout material_buffer_layout = {
                .binding_index = 5,
                .type = rhi::descriptor_type::STORAGE_BUFFER,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::FRAGMENT),
            };

            rhi::descriptor_binding_layout linear_sampler_layout = {
                .binding_index = 15,
                .type = rhi::descriptor_type::SAMPLER,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::FRAGMENT),
            };

            rhi::descriptor_binding_layout bindless_textures_layout = {
                .binding_index = 16,
                .type = rhi::descriptor_type::SAMPLED_IMAGE,
                .count = 512,
                .stages = tempest::make_enum_mask(rhi::shader_stage::FRAGMENT),
                .flags = tempest::make_enum_mask(rhi::descriptor_binding_flags::PARTIALLY_BOUND),
            };
        } // namespace zprepass
    } // namespace

    pbr_pipeline::pbr_pipeline(uint32_t width, uint32_t height)
        : _render_target_width{width}, _render_target_height{height}
    {
    }

    void pbr_pipeline::initialize(renderer& parent, rhi::device& dev)
    {
        _initialize_z_prepass(parent, dev);
        _initialize_render_targets(parent, dev);
    }

    render_pipeline::render_result pbr_pipeline::render([[maybe_unused]] renderer& parent, rhi::device& dev,
                                                        const render_state& rs) const
    {
        auto& work_queue = dev.get_primary_work_queue();
        auto cmds = work_queue.get_next_command_list();
        work_queue.begin_command_list(cmds, true);

        rhi::work_queue::image_barrier prepare_color_buffer = {
            .image = _render_targets.color,
            .old_layout = rhi::image_layout::UNDEFINED,
            .new_layout = rhi::image_layout::COLOR_ATTACHMENT,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::BLIT),
            .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_READ),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::COLOR_ATTACHMENT_OUTPUT),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::COLOR_ATTACHMENT_WRITE),
        };

        work_queue.transition_image(cmds, tempest::span(&prepare_color_buffer, 1));

        rhi::work_queue::render_pass_info render_pass_info = {
            .color_attachments = {},
            .depth_attachment = none(),
            .stencil_attachment = none(),
            .x = 0,
            .y = 0,
            .width = _render_target_width,
            .height = _render_target_height,
            .layers = 1,
        };

        render_pass_info.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = _render_targets.color,
            .layout = rhi::image_layout::COLOR_ATTACHMENT,
            .clear_color = {0.0f, 0.0f, 1.0f, 1.0f},
            .load_op = rhi::work_queue::load_op::CLEAR,
            .store_op = rhi::work_queue::store_op::STORE,
        });

        work_queue.begin_rendering(cmds, render_pass_info);
        work_queue.end_rendering(cmds);

        rhi::work_queue::image_barrier color_to_transfer_dst = {
            .image = _render_targets.color,
            .old_layout = rhi::image_layout::COLOR_ATTACHMENT,
            .new_layout = rhi::image_layout::TRANSFER_SRC,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::COLOR_ATTACHMENT_OUTPUT),
            .src_access = tempest::make_enum_mask(rhi::memory_access::COLOR_ATTACHMENT_WRITE),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::BLIT),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_READ),
        };

        rhi::work_queue::image_barrier sc_to_transfer_dst = {
            .image = rs.swapchain_image,
            .old_layout = rhi::image_layout::UNDEFINED,
            .new_layout = rhi::image_layout::TRANSFER_DST,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::ALL_TRANSFER),
            .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::BLIT),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
        };

        {
            array barriers = {
                color_to_transfer_dst,
                sc_to_transfer_dst,
            };

            work_queue.transition_image(cmds, barriers);
        }

        work_queue.blit(cmds, _render_targets.color, rs.swapchain_image);

        rhi::work_queue::image_barrier sc_to_present = {
            .image = rs.swapchain_image,
            .old_layout = rhi::image_layout::TRANSFER_DST,
            .new_layout = rhi::image_layout::PRESENT,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::BLIT),
            .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::BOTTOM),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::NONE),
        };

        work_queue.transition_image(cmds, tempest::span(&sc_to_present, 1));

        work_queue.end_command_list(cmds);

        rhi::work_queue::submit_info submit_info;
        submit_info.command_lists.push_back(cmds);
        submit_info.wait_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = rs.start_sem,
            .value = 0,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::ALL_TRANSFER),
        });
        submit_info.signal_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = rs.end_sem,
            .value = 1,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::BOTTOM),
        });

        work_queue.submit(tempest::span(&submit_info, 1), rs.end_fence);

        rhi::work_queue::present_info present_info;
        present_info.swapchain_images.push_back(rhi::work_queue::swapchain_image_present_info{
            .render_surface = rs.surface,
            .image_index = rs.image_index,
        });
        present_info.wait_semaphores.push_back(rs.end_sem);

        auto present_result = work_queue.present(present_info);
        if (present_result == rhi::work_queue::present_result::OUT_OF_DATE ||
            present_result == rhi::work_queue::present_result::SUBOPTIMAL)
        {
            return render_result::REQUEST_RECREATE_SWAPCHAIN;
        }
        else if (present_result == rhi::work_queue::present_result::ERROR)
        {
            return render_result::FAILURE;
        }
        return render_result::SUCCESS;
    }

    void pbr_pipeline::destroy([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        dev.destroy_graphics_pipeline(_z_prepass.pipeline);
        dev.destroy_image(_render_targets.depth);
        dev.destroy_image(_render_targets.color);
        dev.destroy_image(_render_targets.positions);
        dev.destroy_image(_render_targets.encoded_normals);
    }

    void pbr_pipeline::_initialize_z_prepass([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        tempest::vector<rhi::descriptor_binding_layout> bindings;
        bindings.reserve(8);

        bindings.push_back(zprepass::scene_constants_binding_layout);
        bindings.push_back(zprepass::vertex_pull_buffer_layout);
        bindings.push_back(zprepass::mesh_buffer_layout);
        bindings.push_back(zprepass::object_buffer_layout);
        bindings.push_back(zprepass::instance_buffer_layout);
        bindings.push_back(zprepass::material_buffer_layout);
        bindings.push_back(zprepass::linear_sampler_layout);
        bindings.push_back(zprepass::bindless_textures_layout);

        auto layout = dev.create_descriptor_set_layout(bindings);

        tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::DESCRIPTOR_SET_LAYOUT>> layouts;
        layouts.push_back(layout);

        auto pipeline_layout = dev.create_pipeline_layout({
            .descriptor_set_layouts = tempest::move(layouts),
            .push_constants = {},
        });

        _z_prepass.desc_set_0_layout = layout;
        _z_prepass.layout = pipeline_layout;

        auto vert_source = core::read_bytes("assets/shaders/zprepass.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/zprepass.frag.spv");

        assert(vert_source.size() > 0);
        assert(frag_source.size() > 0);

        vector<rhi::image_format> color_formats(2);
        color_formats[0] = encoded_normals_format;
        color_formats[1] = positions_format;

        // No blend on slim gbuffer
        vector<rhi::color_blend_attachment> blending(2);
        // Normals
        blending[0].blend_enable = false;
        // Positions
        blending[1].blend_enable = false;

        rhi::graphics_pipeline_desc z_prepass_desc = {
            .color_attachment_formats = tempest::move(color_formats),
            .depth_attachment_format = depth_format,
            .stencil_attachment_format = none(),
            .vertex_shader = tempest::move(vert_source),
            .tessellation_control_shader = {},
            .tessellation_evaluation_shader = {},
            .geometry_shader = {},
            .fragment_shader = tempest::move(frag_source),
            .input_assembly =
                {
                    .topology = rhi::primitive_topology::TRIANGLE_LIST,
                },
            .tessellation = none(),
            .multisample =
                {
                    .sample_count = rhi::image_sample_count::SAMPLE_COUNT_1,
                    .sample_shading = none(),
                    .alpha_to_coverage = false,
                    .alpha_to_one = false,
                },
            .rasterization =
                {
                    .depth_clamp_enable = false,
                    .rasterizer_discard_enable = false,
                    .polygon_mode = rhi::polygon_mode::FILL,
                    .cull_mode = make_enum_mask(rhi::cull_mode::BACK),
                    .vertex_winding = rhi::vertex_winding::COUNTER_CLOCKWISE,
                    .depth_bias = none(),
                    .line_width = 1.0f,
                },
            .depth_stencil =
                {
                    .depth =
                        rhi::depth_test{
                            .write_enable = true,
                            .compare_op = rhi::compare_op::GREATER_EQUAL,
                            .depth_bounds_test_enable = false,
                            .min_depth_bounds = 0.0f,
                            .max_depth_bounds = 1.0f,
                        },
                    .stencil = none(),
                },
            .color_blend =
                {
                    .attachments = tempest::move(blending),
                    .blend_constants = {},
                },
            .layout = _z_prepass.layout,
        };

        _z_prepass.pipeline = dev.create_graphics_pipeline(tempest::move(z_prepass_desc));
    }

    void pbr_pipeline::_initialize_render_targets([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        rhi::image_desc depth_image_desc = {
            .format = depth_format,
            .type = rhi::image_type::IMAGE_2D,
            .width = _render_target_width,
            .height = _render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::SAMPLE_COUNT_1,
            .tiling = rhi::image_tiling_type::OPTIMAL,
            .location = rhi::memory_location::DEVICE,
            .usage = make_enum_mask(rhi::image_usage::DEPTH_ATTACHMENT, rhi::image_usage::SAMPLED,
                                    rhi::image_usage::TRANSFER_SRC),
            .name = "Depth Texture",
        };

        rhi::image_desc color_image_desc = {
            .format = color_format,
            .type = rhi::image_type::IMAGE_2D,
            .width = _render_target_width,
            .height = _render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::SAMPLE_COUNT_1,
            .tiling = rhi::image_tiling_type::OPTIMAL,
            .location = rhi::memory_location::DEVICE,
            .usage = make_enum_mask(rhi::image_usage::COLOR_ATTACHMENT, rhi::image_usage::SAMPLED,
                                    rhi::image_usage::TRANSFER_SRC),
            .name = "Color Texture",
        };

        rhi::image_desc encoded_normals_image_desc = {
            .format = encoded_normals_format,
            .type = rhi::image_type::IMAGE_2D,
            .width = _render_target_width,
            .height = _render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::SAMPLE_COUNT_1,
            .tiling = rhi::image_tiling_type::OPTIMAL,
            .location = rhi::memory_location::DEVICE,
            .usage = make_enum_mask(rhi::image_usage::COLOR_ATTACHMENT, rhi::image_usage::SAMPLED),
            .name = "Encoded Normals Texture",
        };

        rhi::image_desc positions_image_desc = {
            .format = positions_format,
            .type = rhi::image_type::IMAGE_2D,
            .width = _render_target_width,
            .height = _render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::SAMPLE_COUNT_1,
            .tiling = rhi::image_tiling_type::OPTIMAL,
            .location = rhi::memory_location::DEVICE,
            .usage = make_enum_mask(rhi::image_usage::COLOR_ATTACHMENT, rhi::image_usage::SAMPLED),
            .name = "Positions Texture",
        };

        _render_targets.depth = dev.create_image(depth_image_desc);
        _render_targets.color = dev.create_image(color_image_desc);
        _render_targets.encoded_normals = dev.create_image(encoded_normals_image_desc);
        _render_targets.positions = dev.create_image(positions_image_desc);
    }
} // namespace tempest::graphics