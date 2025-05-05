#include <tempest/pipelines/pbr_pipeline.hpp>

#include <tempest/files.hpp>
#include <tempest/logger.hpp>

#include <cassert>
#include <cstring>

namespace tempest::graphics
{
    namespace
    {
        auto log = logger::logger_factory::create({.prefix = {"pbr_pipeline"}});

        namespace zprepass
        {
            rhi::descriptor_binding_layout scene_constants_binding_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::dynamic_constant_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex),
            };

            rhi::descriptor_binding_layout vertex_pull_buffer_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex),
            };

            rhi::descriptor_binding_layout mesh_buffer_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex),
            };

            rhi::descriptor_binding_layout object_buffer_layout = {
                .binding_index = 3,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex),
            };

            rhi::descriptor_binding_layout instance_buffer_layout = {
                .binding_index = 4,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex),
            };

            rhi::descriptor_binding_layout material_buffer_layout = {
                .binding_index = 5,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout linear_sampler_layout = {
                .binding_index = 15,
                .type = rhi::descriptor_type::sampler,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout bindless_textures_layout = {
                .binding_index = 16,
                .type = rhi::descriptor_type::sampled_image,
                .count = 512,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
                .flags = tempest::make_enum_mask(rhi::descriptor_binding_flags::partially_bound),
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
        _initialize_gpu_buffers(parent, dev);
    }

    render_pipeline::render_result pbr_pipeline::render([[maybe_unused]] renderer& parent, rhi::device& dev,
                                                        const render_state& rs) const
    {
        auto& work_queue = dev.get_primary_work_queue();
        auto cmds = work_queue.get_next_command_list();
        work_queue.begin_command_list(cmds, true);

        _draw_clear_pass(parent, dev, rs, work_queue, cmds);
        _draw_z_prepass(parent, dev, rs, work_queue, cmds);

        rhi::work_queue::image_barrier color_to_transfer_dst = {
            .image = _render_targets.color,
            .old_layout = rhi::image_layout::color_attachment,
            .new_layout = rhi::image_layout::transfer_src,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::color_attachment_output),
            .src_access = tempest::make_enum_mask(rhi::memory_access::color_attachment_write),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::blit),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::transfer_read),
        };

        rhi::work_queue::image_barrier sc_to_transfer_dst = {
            .image = rs.swapchain_image,
            .old_layout = rhi::image_layout::undefined,
            .new_layout = rhi::image_layout::transfer_dst,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::all_transfer),
            .src_access = tempest::make_enum_mask(rhi::memory_access::transfer_write),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::blit),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::transfer_write),
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
            .old_layout = rhi::image_layout::transfer_dst,
            .new_layout = rhi::image_layout::present,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::blit),
            .src_access = tempest::make_enum_mask(rhi::memory_access::transfer_write),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::bottom),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::none),
        };

        work_queue.transition_image(cmds, tempest::span(&sc_to_present, 1));

        work_queue.end_command_list(cmds);

        rhi::work_queue::submit_info submit_info;
        submit_info.command_lists.push_back(cmds);
        submit_info.wait_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = rs.start_sem,
            .value = 0,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::all_transfer),
        });
        submit_info.signal_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = rs.end_sem,
            .value = 1,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::bottom),
        });

        work_queue.submit(tempest::span(&submit_info, 1), rs.end_fence);

        rhi::work_queue::present_info present_info;
        present_info.swapchain_images.push_back(rhi::work_queue::swapchain_image_present_info{
            .render_surface = rs.surface,
            .image_index = rs.image_index,
        });
        present_info.wait_semaphores.push_back(rs.end_sem);

        auto present_result = work_queue.present(present_info);
        if (present_result == rhi::work_queue::present_result::out_of_date ||
            present_result == rhi::work_queue::present_result::suboptimal)
        {
            return render_result::REQUEST_RECREATE_SWAPCHAIN;
        }
        else if (present_result == rhi::work_queue::present_result::error)
        {
            return render_result::FAILURE;
        }
        return render_result::SUCCESS;
    }

    void pbr_pipeline::destroy([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        // Destroy GPU buffers
        dev.destroy_buffer(_gpu_buffers.staging);
        dev.destroy_buffer(_gpu_buffers.vertices);
        dev.destroy_buffer(_gpu_buffers.mesh_layouts);
        dev.destroy_buffer(_gpu_buffers.scene_constants);
        dev.destroy_buffer(_gpu_buffers.materials);
        dev.destroy_buffer(_gpu_buffers.instances);
        dev.destroy_buffer(_gpu_buffers.objects);
        dev.destroy_buffer(_gpu_buffers.indirect_commands);

        dev.destroy_graphics_pipeline(_z_prepass.pipeline);
        dev.destroy_image(_render_targets.depth);
        dev.destroy_image(_render_targets.color);
        dev.destroy_image(_render_targets.encoded_normals);
    }

    flat_unordered_map<guid, mesh_layout> pbr_pipeline::load_meshes(rhi::device& dev, span<const guid> mesh_ids,
                                                                    const core::mesh_registry& mesh_registry)
    {
        flat_unordered_map<guid, mesh_layout> result;

        uint32_t bytes_written = 0;
        uint32_t total_bytes_required = 0;

        for (const auto& mesh_id : mesh_ids)
        {
            optional<const core::mesh&> mesh_opt = mesh_registry.find(mesh_id);
            assert(mesh_opt.has_value());

            auto& mesh = *mesh_opt;

            // Compute vertex size in bytes
            auto vertex_size = sizeof(float) * 3    // position
                               + sizeof(float) * 3  // normal
                               + sizeof(float) * 2  // uv
                               + sizeof(float) * 3; // tangent
            if (mesh.has_colors)
            {
                vertex_size += sizeof(float) * 4; // color
            }

            auto mesh_size = vertex_size * mesh.vertices.size() + sizeof(uint32_t) * mesh.indices.size();
            total_bytes_required += static_cast<uint32_t>(mesh_size);
        }

        auto staging = dev.create_buffer({
            .size = total_bytes_required,
            .location = rhi::memory_location::host,
            .usage = make_enum_mask(rhi::buffer_usage::transfer_src),
            .access_type = rhi::host_access_type::incoherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "Staging Buffer",
        });

        auto dst = dev.map_buffer(staging);

        for (const auto& mesh_id : mesh_ids)
        {
            optional<const core::mesh&> mesh_opt = mesh_registry.find(mesh_id);
            const auto& mesh = *mesh_opt;

            // Region 0
            // - Positions (3 floats)
            // Region 1
            // - Normals (3 floats)
            // - UVs (2 floats)
            // - Tangents (3 floats)
            // - Colors (4 floats, optional)

            graphics::mesh_layout layout = {
                .mesh_start_offset = static_cast<uint32_t>(bytes_written),
                .positions_offset = 0,
                .interleave_offset = 3 * static_cast<uint32_t>(sizeof(float) * mesh.vertices.size()),
                .interleave_stride = 0,
                .uvs_offset = 0,
                .normals_offset = static_cast<uint32_t>(2 * sizeof(float)),
                .tangents_offset = static_cast<uint32_t>(5 * sizeof(float)),
                .index_offset = 0,
                .index_count = 0,
            };

            auto last_offset = 9 * sizeof(float);

            if (mesh.has_colors)
            {
                layout.color_offset = static_cast<uint32_t>(last_offset);
                last_offset += sizeof(float) * 4;
            }

            layout.interleave_stride = static_cast<uint32_t>(last_offset);
            layout.index_offset =
                layout.interleave_offset + layout.interleave_stride * static_cast<uint32_t>(mesh.vertices.size());
            layout.index_count = static_cast<uint32_t>(mesh.indices.size());

            result.insert({mesh_id, layout});

            // Position attribute
            size_t vertices_written = 0;
            for (const auto& vertex : mesh.vertices)
            {
                std::memcpy(dst + bytes_written + vertices_written * 3 * sizeof(float), &vertex.position,
                            sizeof(float) * 3);

                ++vertices_written;
            }

            bytes_written += layout.interleave_offset;

            // Interleaved, non-position attributes
            vertices_written = 0;
            for (const auto& vertex : mesh.vertices)
            {
                std::memcpy(dst + bytes_written + layout.uvs_offset + vertices_written * layout.interleave_stride,
                            &vertex.uv, 2 * sizeof(float));
                std::memcpy(dst + bytes_written + layout.normals_offset + vertices_written * layout.interleave_stride,
                            &vertex.normal, 3 * sizeof(float));
                std::memcpy(dst + bytes_written + layout.tangents_offset + vertices_written * layout.interleave_stride,
                            &vertex.tangent, 3 * sizeof(float));

                if (mesh.has_colors)
                {
                    std::memcpy(dst + bytes_written + layout.color_offset + vertices_written * layout.interleave_stride,
                                &vertex.color, 4 * sizeof(float));
                }

                ++vertices_written;
            }

            bytes_written += layout.interleave_stride * static_cast<uint32_t>(mesh.vertices.size());

            // Indices
            std::memcpy(dst + bytes_written + layout.index_offset, mesh.indices.data(),
                        sizeof(uint32_t) * mesh.indices.size());

            bytes_written += static_cast<uint32_t>(sizeof(uint32_t) * mesh.indices.size());
        }

        // Flush the staging buffer
        dev.unmap_buffer(staging);
        dev.flush_buffers(span(&staging, 1));

        // Upload the staging buffer to the GPU
        auto& work_queue = dev.get_primary_work_queue();
        auto cmd_buf = work_queue.get_next_command_list();

        work_queue.begin_command_list(cmd_buf, true);
        work_queue.copy(cmd_buf, staging, _gpu_buffers.vertices, 0, _gpu_resource_usages.vertex_bytes_written,
                        total_bytes_required);
        work_queue.end_command_list(cmd_buf);

        rhi::work_queue::submit_info submit_info;
        submit_info.command_lists.push_back(cmd_buf);

        // Get a fence for the copy operation
        auto complete_fence = dev.create_fence({
            .signaled = false,
        });

        // Submit
        work_queue.submit(span(&submit_info, 1), complete_fence);

        // Wait for the copy operation to complete
        dev.wait(span(&complete_fence, 1));

        // Clean up the resources
        dev.destroy_buffer(staging);
        dev.destroy_fence(complete_fence);

        _gpu_resource_usages.vertex_bytes_written += total_bytes_required;

        return result;
    }

    void pbr_pipeline::load_textures([[maybe_unused]] rhi::device& dev, span<const guid> texture_ids,
                                     const core::texture_registry& texture_registry, bool generate_mip_maps)
    {
        vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::image>> images;

        for (const auto& tex_guid : texture_ids)
        {
            auto texture_opt = texture_registry.get_texture(tex_guid);
            assert(texture_opt.has_value());

            const auto& texture = *texture_opt;
            [[maybe_unused]] const auto mip_count = generate_mip_maps ? bit_width(min(texture.width, texture.height))
                                                                      : static_cast<std::uint32_t>(texture.mips.size());

            rhi::image_desc image_desc = {

            };
        }
    }

    void pbr_pipeline::load_materials([[maybe_unused]] rhi::device& dev, [[maybe_unused]] span<const guid> material_ids,
        [[maybe_unused]] const core::material_registry& material_registry)
    {
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

        tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> layouts;
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
                    .topology = rhi::primitive_topology::triangle_list,
                },
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
                    .attachments = tempest::move(blending),
                    .blend_constants = {},
                },
            .layout = _z_prepass.layout,
            .name = "Z Prepass Pipeline",
        };

        _z_prepass.pipeline = dev.create_graphics_pipeline(tempest::move(z_prepass_desc));
    }

    void pbr_pipeline::_initialize_render_targets([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        rhi::image_desc depth_image_desc = {
            .format = depth_format,
            .type = rhi::image_type::image_2d,
            .width = _render_target_width,
            .height = _render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::depth_attachment, rhi::image_usage::sampled,
                                    rhi::image_usage::transfer_src),
            .name = "Depth Texture",
        };

        rhi::image_desc color_image_desc = {
            .format = color_format,
            .type = rhi::image_type::image_2d,
            .width = _render_target_width,
            .height = _render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled,
                                    rhi::image_usage::transfer_src),
            .name = "Color Texture",
        };

        rhi::image_desc encoded_normals_image_desc = {
            .format = encoded_normals_format,
            .type = rhi::image_type::image_2d,
            .width = _render_target_width,
            .height = _render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled),
            .name = "Encoded Normals Texture",
        };

        rhi::image_desc positions_image_desc = {
            .format = positions_format,
            .type = rhi::image_type::image_2d,
            .width = _render_target_width,
            .height = _render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled),
            .name = "Positions Texture",
        };

        _render_targets.depth = dev.create_image(depth_image_desc);
        _render_targets.color = dev.create_image(color_image_desc);
        _render_targets.encoded_normals = dev.create_image(encoded_normals_image_desc);
    }

    void pbr_pipeline::_initialize_gpu_buffers([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        constexpr auto staging_size = 64 * 1024 * 1024;

        auto staging = dev.create_buffer({
            .size = staging_size,
            .location = rhi::memory_location::host,
            .usage = make_enum_mask(rhi::buffer_usage::transfer_src),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "Staging Buffer",
        });

        _gpu_resource_usages.staging_bytes_writen = 0;
        _gpu_resource_usages.staging_bytes_available = staging_size;
        _gpu_buffers.staging = staging;

        // Set up vertex buffer
        constexpr auto vertex_buffer_size = 256 * 1024 * 1024;

        auto vertex_buffer = dev.create_buffer({
            .size = vertex_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Vertex Buffer",
        });

        _gpu_resource_usages.vertex_bytes_written = 0;
        _gpu_buffers.vertices = vertex_buffer;

        // Set up mesh layout buffer
        constexpr auto mesh_layout_buffer_size = sizeof(mesh_layout) * 64 * 1024;

        auto mesh_layout_buffer = dev.create_buffer({
            .size = mesh_layout_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Mesh Layout Buffer",
        });

        _gpu_buffers.mesh_layouts = mesh_layout_buffer;

        // Set up scene buffer
        const auto scene_buffer_size = sizeof(gpu::scene_data) * dev.frames_in_flight();

        auto scene_buffer = dev.create_buffer({
            .size = scene_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::constant, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Scene Buffer",
        });

        _gpu_buffers.scene_constants = scene_buffer;

        // Set up material buffer
        constexpr auto material_buffer_size = sizeof(gpu::material_data) * 64 * 1024;

        auto material_buffer = dev.create_buffer({
            .size = material_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Material Buffer",
        });

        _gpu_buffers.materials = material_buffer;

        // Set up instance buffer
        const auto instance_buffer_size = sizeof(uint32_t) * 64 * 1024 * dev.frames_in_flight();

        auto instance_buffer = dev.create_buffer({
            .size = instance_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Instance Buffer",
        });

        _gpu_buffers.instances = instance_buffer;

        // Set up object buffer
        const auto object_buffer_size = sizeof(gpu::object_data) * 64 * 1024 * dev.frames_in_flight();

        auto object_buffer = dev.create_buffer({
            .size = object_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Object Buffer",
        });

        _gpu_buffers.objects = object_buffer;

        // Set up indirect command buffer
        const auto indirect_command_buffer_size = sizeof(gpu::indirect_command) * 64 * 1024 * dev.frames_in_flight();

        auto indirect_command_buffer = dev.create_buffer({
            .size = indirect_command_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::indirect, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Indirect Command Buffer",
        });

        _gpu_buffers.indirect_commands = indirect_command_buffer;
    }

    void pbr_pipeline::_draw_z_prepass([[maybe_unused]] renderer& parent, [[maybe_unused]] rhi::device& dev,
                                       [[maybe_unused]] const render_state& rs, rhi::work_queue& queue,
                                       rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands) const
    {
        // Barrier to wait for the encoded normals buffer to be done any previous operations
        rhi::work_queue::image_barrier undefined_to_color_attachment = {
            .image = _render_targets.encoded_normals,
            .old_layout = rhi::image_layout::undefined,
            .new_layout = rhi::image_layout::color_attachment,
            .src_stages =
                tempest::make_enum_mask(rhi::pipeline_stage::fragment_shader, rhi::pipeline_stage::compute_shader,
                                        rhi::pipeline_stage::color_attachment_output),
            .src_access = tempest::make_enum_mask(
                rhi::memory_access::shader_sampled_read, rhi::memory_access::shader_storage_read,
                rhi::memory_access::color_attachment_read, rhi::memory_access::color_attachment_write),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::color_attachment_output),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::color_attachment_write),
        };

        // Barrier to wait for the depth buffer to be done any previous operations
        rhi::work_queue::image_barrier undefined_to_depth_attachment = {
            .image = _render_targets.depth,
            .old_layout = rhi::image_layout::undefined,
            .new_layout = rhi::image_layout::depth,
            .src_stages = make_enum_mask(rhi::pipeline_stage::late_fragment_tests),
            .src_access = make_enum_mask(rhi::memory_access::depth_stencil_attachment_read,
                                         rhi::memory_access::depth_stencil_attachment_write),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::early_fragment_tests),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::depth_stencil_attachment_read,
                                                  rhi::memory_access::depth_stencil_attachment_write),
        };

        array barriers = {
            undefined_to_color_attachment,
            undefined_to_depth_attachment,
        };

        queue.transition_image(commands, barriers);

        rhi::work_queue::render_pass_info rpi = {
            .color_attachments = {},
            .depth_attachment =
                rhi::work_queue::depth_attachment_info{
                    .image = _render_targets.depth,
                    .layout = rhi::image_layout::depth,
                    .clear_depth = 1.0f,
                    .load_op = rhi::work_queue::load_op::clear,
                    .store_op = rhi::work_queue::store_op::store,
                },
            .stencil_attachment = none(),
            .x = 0,
            .y = 0,
            .width = _render_target_width,
            .height = _render_target_height,
            .layers = 1,
            .name = "Z Prepass",
        };

        rpi.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = _render_targets.encoded_normals,
            .layout = rhi::image_layout::color_attachment,
            .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        });

        queue.begin_rendering(commands, rpi);

        queue.end_rendering(commands);
    }

    void pbr_pipeline::_draw_clear_pass([[maybe_unused]] renderer& parent, [[maybe_unused]] rhi::device& dev,
                                        [[maybe_unused]] const render_state& rs, rhi::work_queue& queue,
                                        rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands) const
    {
        // Barrier to wait for the color buffer to be done any previous operations
        rhi::work_queue::image_barrier undefined_to_color_attachment = {
            .image = _render_targets.color,
            .old_layout = rhi::image_layout::undefined,
            .new_layout = rhi::image_layout::color_attachment,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::blit),
            .src_access = tempest::make_enum_mask(rhi::memory_access::transfer_read),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::color_attachment_output),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::color_attachment_write),
        };

        queue.transition_image(commands, tempest::span(&undefined_to_color_attachment, 1));

        rhi::work_queue::render_pass_info render_pass_info = {
            .color_attachments = {},
            .depth_attachment = none(),
            .stencil_attachment = none(),
            .x = 0,
            .y = 0,
            .width = _render_target_width,
            .height = _render_target_height,
            .layers = 1,
            .name = "Clear Color",
        };

        render_pass_info.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = _render_targets.color,
            .layout = rhi::image_layout::color_attachment,
            .clear_color = {0.0f, 0.0f, 1.0f, 1.0f},
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        });

        queue.begin_rendering(commands, render_pass_info);
        queue.end_rendering(commands);
    }
} // namespace tempest::graphics