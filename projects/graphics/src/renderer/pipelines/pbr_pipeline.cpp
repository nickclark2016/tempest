#include <tempest/pipelines/pbr_pipeline.hpp>

#include <tempest/files.hpp>
#include <tempest/logger.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/transform_component.hpp>

#include <cassert>
#include <cstring>

namespace tempest::graphics
{
    namespace
    {
        auto log = logger::logger_factory::create({.prefix = {"pbr_pipeline"}});

        rhi::image_format convert_format(core::texture_format fmt)
        {
            switch (fmt)
            {
            case core::texture_format::rgba8_srgb:
                return rhi::image_format::rgba8_srgb;
            case core::texture_format::rgba8_unorm:
                return rhi::image_format::rgba8_unorm;
            case core::texture_format::rgba16_unorm:
                return rhi::image_format::rgba16_unorm;
            case core::texture_format::rgba32_float:
                return rhi::image_format::rgba32_float;
            default:
                log->error("Unsupported texture format");
            }

            unreachable();
        }

        rhi::work_queue::buffer_barrier pre_host_write_buffer_barrier(
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer, size_t offset, size_t range)
        {
            return {
                .buffer = buffer,
                .src_stages =
                    tempest::make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader,
                                            rhi::pipeline_stage::compute_shader),
                .src_access =
                    tempest::make_enum_mask(rhi::memory_access::memory_read, rhi::memory_access::memory_write),
                .dst_stages = make_enum_mask(rhi::pipeline_stage::all_transfer),
                .dst_access = make_enum_mask(rhi::memory_access::transfer_read),
                .offset = offset,
                .size = range,
            };
        }

        rhi::work_queue::buffer_barrier pre_staging_to_dst_buffer_barrier(
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer, size_t offset, size_t range)
        {
            return {
                .buffer = buffer,
                .src_stages =
                    make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader,
                                   rhi::pipeline_stage::compute_shader, rhi::pipeline_stage::indirect_command),
                .src_access = make_enum_mask(rhi::memory_access::memory_read, rhi::memory_access::memory_write),
                .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::copy),
                .dst_access = tempest::make_enum_mask(rhi::memory_access::transfer_write),
                .offset = offset,
                .size = range,
            };
        }

        rhi::work_queue::buffer_barrier post_staging_to_dst_buffer_barrier(
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer, size_t offset, size_t range)
        {
            return {
                .buffer = buffer,
                .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::copy),
                .src_access = tempest::make_enum_mask(rhi::memory_access::transfer_write),
                .dst_stages =
                    tempest::make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader,
                                            rhi::pipeline_stage::compute_shader, rhi::pipeline_stage::indirect_command),
                .dst_access =
                    tempest::make_enum_mask(rhi::memory_access::memory_read, rhi::memory_access::memory_write),
                .offset = offset,
                .size = range,
            };
        }

        namespace zprepass
        {
            struct scene_constants
            {
                gpu::camera camera;
                math::vec2<float> screen_size;
            };

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

        namespace shadows
        {
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

            rhi::push_constant_range light_matrix_pc_range = {
                .offset = 0,
                .range = 64, // float4x4
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex),
            };
        } // namespace shadows

        namespace clusters
        {
            struct cluster_grid_create_info
            {
                math::mat4<float> inv_proj;
                math::vec4<float> screen_bounds;
                math::vec4<uint32_t> workgroup_count_tile_size_px;
            };

            struct cull_lights_pcs
            {
                cluster_grid_create_info grid_ci;
                uint32_t light_count;
            };

            rhi::push_constant_range build_cluster_grid_pc_range = {
                .offset = 0,
                .range = static_cast<uint32_t>(sizeof(cluster_grid_create_info)),
                .stages = tempest::make_enum_mask(rhi::shader_stage::compute),
            };

            rhi::push_constant_range cull_lights_pc_range = {
                .offset = 0,
                .range = static_cast<uint32_t>(sizeof(cull_lights_pcs)),
                .stages = tempest::make_enum_mask(rhi::shader_stage::compute),
            };

            rhi::descriptor_binding_layout build_cluster_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::compute),
            };

            rhi::descriptor_binding_layout scene_constants_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::dynamic_constant_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::compute),
            };

            rhi::descriptor_binding_layout cull_cluster_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::compute),
            };

            rhi::descriptor_binding_layout lights_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::compute),
            };

            rhi::descriptor_binding_layout global_light_index_list_layout = {
                .binding_index = 3,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::compute),
            };

            rhi::descriptor_binding_layout light_grid_layout = {
                .binding_index = 4,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::compute),
            };

            rhi::descriptor_binding_layout global_index_count = {
                .binding_index = 5,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::compute),
            };
        } // namespace clusters

        namespace pbr
        {
            // Set 0
            rhi::descriptor_binding_layout scene_constants_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::dynamic_constant_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout vertex_pull_buffer_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout mesh_buffer_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout object_buffer_layout = {
                .binding_index = 3,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout instance_buffer_layout = {
                .binding_index = 4,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout material_buffer_layout = {
                .binding_index = 5,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout ao_image_layout = {
                .binding_index = 6,
                .type = rhi::descriptor_type::sampled_image,
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

            // Set 1
            rhi::descriptor_binding_layout lights_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout light_params_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout shadow_map_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::sampled_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout light_grid_layout = {
                .binding_index = 3,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout global_index_count = {
                .binding_index = 4,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };
        } // namespace pbr

        namespace pbr_transparencies
        {
            // set 0
            rhi::descriptor_binding_layout scene_constants_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::dynamic_constant_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout vertex_pull_buffer_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout mesh_buffer_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout object_buffer_layout = {
                .binding_index = 3,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout instance_buffer_layout = {
                .binding_index = 4,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout material_buffer_layout = {
                .binding_index = 5,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout oit_image_layout = {
                .binding_index = 6,
                .type = rhi::descriptor_type::storage_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout oit_zeroth_image_layout = {
                .binding_index = 7,
                .type = rhi::descriptor_type::storage_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout ao_image_layout = {
                .binding_index = 8,
                .type = rhi::descriptor_type::sampled_image,
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

            // Set 1
            rhi::descriptor_binding_layout lights_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout light_params_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout shadow_map_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::sampled_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout light_grid_layout = {
                .binding_index = 3,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout global_index_count = {
                .binding_index = 4,
                .type = rhi::descriptor_type::structured_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout blend_moments_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::storage_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout blend_moments_zeroth_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::storage_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout blend_transparency_accumulator_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::sampled_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout blend_linear_sampler_layout = {
                .binding_index = 3,
                .type = rhi::descriptor_type::sampler,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };
        } // namespace pbr_transparencies

        namespace ssao
        {
            struct scene_constants
            {
                math::mat4<float> projection;
                math::mat4<float> inv_projection;
                math::mat4<float> view;
                math::mat4<float> inv_view;
                array<math::vec4<float>, 64> kernel;
                math::vec2<float> noise_scale;
                float radius;
                float bias;
            };

            // SSAO layouts
            rhi::descriptor_binding_layout scene_constants_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::dynamic_constant_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout depth_buffer_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::sampled_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout normal_buffer_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::sampled_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout noise_buffer_layout = {
                .binding_index = 3,
                .type = rhi::descriptor_type::sampled_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout linear_sampler_layout = {
                .binding_index = 4,
                .type = rhi::descriptor_type::sampler,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout point_sampler_layout = {
                .binding_index = 5,
                .type = rhi::descriptor_type::sampler,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            // Blur layouts
            rhi::descriptor_binding_layout ssao_input_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::sampled_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout blur_point_sampler_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::sampler,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };
        } // namespace ssao

        namespace skybox
        {
            rhi::descriptor_binding_layout scene_constants_layout = {
                .binding_index = 0,
                .type = rhi::descriptor_type::dynamic_constant_buffer,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout skybox_texture_layout = {
                .binding_index = 1,
                .type = rhi::descriptor_type::sampled_image,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };

            rhi::descriptor_binding_layout linear_sampler_layout = {
                .binding_index = 2,
                .type = rhi::descriptor_type::sampler,
                .count = 1,
                .stages = tempest::make_enum_mask(rhi::shader_stage::fragment),
            };
        } // namespace skybox
    } // namespace

    pbr_pipeline::pbr_pipeline(uint32_t width, uint32_t height, ecs::archetype_registry& entity_registry)
        : _render_target_width{width}, _render_target_height{height}, _entity_registry{&entity_registry}
    {
    }

    void pbr_pipeline::initialize(renderer& parent, rhi::device& dev)
    {
        _initialize_z_prepass(parent, dev);
        _initialize_clustering(parent, dev);
        _initialize_pbr_opaque(parent, dev);
        _initialize_pbr_transparent(parent, dev);
        _initialize_shadows(parent, dev);
        _initialize_ssao(parent, dev);
        _initialize_skybox(parent, dev);
        _initialize_render_targets(parent, dev);
        _initialize_gpu_buffers(parent, dev);
        _initialize_samplers(parent, dev);
    }

    render_pipeline::render_result pbr_pipeline::render([[maybe_unused]] renderer& parent, rhi::device& dev,
                                                        const render_state& rs)
    {
        _gpu_resource_usages.staging_bytes_writen = 0;

        _entity_registry->each(
            [&]([[maybe_unused]] const camera_component& camera, ecs::self_component self) { _camera = self.entity; });

        auto& work_queue = dev.get_primary_work_queue();
        auto cmds = work_queue.get_next_command_list();
        work_queue.begin_command_list(cmds, true);

        _prepare_draw_batches(parent, dev, rs, work_queue, cmds);
        _draw_clear_pass(parent, dev, rs, work_queue, cmds);
        _draw_z_prepass(parent, dev, rs, work_queue, cmds);
        _draw_shadow_pass(parent, dev, rs, work_queue, cmds);

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

        work_queue.blit(cmds, _render_targets.color, rhi::image_layout::transfer_src, 0, rs.swapchain_image,
                        rhi::image_layout::transfer_dst, 0);

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

        ++_frame_number;
        _frame_in_flight = _frame_number % dev.frames_in_flight();

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

        // Destroy PBR Opaque
        dev.destroy_graphics_pipeline(_pbr_opaque.pipeline);

        // Destroy PBR Transparent
        dev.destroy_graphics_pipeline(_pbr_transparencies.oit_gather_pipeline);
        dev.destroy_graphics_pipeline(_pbr_transparencies.oit_resolve_pipeline);
        dev.destroy_graphics_pipeline(_pbr_transparencies.oit_blend_pipeline);

        // Destroy light culling
        dev.destroy_compute_pipeline(_forward_light_clustering.build_clusters);
        dev.destroy_compute_pipeline(_forward_light_clustering.fill_clusters);

        // Destroy z prepass
        dev.destroy_descriptor_set(_z_prepass.desc_set_0);
        dev.destroy_graphics_pipeline(_z_prepass.pipeline);
        dev.destroy_buffer(_z_prepass.scene_constants);

        // Destroy shadows
        dev.destroy_graphics_pipeline(_shadows.directional_pipeline);
        dev.destroy_descriptor_set(_shadows.directional_desc_set_0);

        // Destroy SSAO
        dev.destroy_graphics_pipeline(_ssao.ssao_pipeline);
        dev.destroy_graphics_pipeline(_ssao.ssao_blur_pipeline);
        dev.destroy_buffer(_ssao.scene_constants);

        // Destroy Skybox
        dev.destroy_graphics_pipeline(_skybox.pipeline);

        // Destroy render targets
        dev.destroy_image(_render_targets.depth);
        dev.destroy_image(_render_targets.color);
        dev.destroy_image(_render_targets.encoded_normals);
        dev.destroy_image(_render_targets.transparency_accumulator);
        dev.destroy_image(_render_targets.shadow_megatexture);

        // Destroy samplers
        dev.destroy_sampler(_bindless_textures.linear_sampler);
        dev.destroy_sampler(_bindless_textures.point_sampler);
        dev.destroy_sampler(_bindless_textures.linear_sampler_no_aniso);
        dev.destroy_sampler(_bindless_textures.point_sampler_no_aniso);
    }

    void pbr_pipeline::upload_objects_sync(rhi::device& dev, span<const ecs::archetype_entity> entities,
                                           const core::mesh_registry& meshes, const core::texture_registry& textures,
                                           const core::material_registry& materials)
    {
        vector<guid> mesh_guids;
        vector<guid> texture_guids;
        vector<guid> material_guids;

        for (const auto entity : entities)
        {
            const auto hierarchy_view = ecs::archetype_entity_hierarchy_view(*_entity_registry, entity);
            for (const auto e : hierarchy_view)
            {
                const auto mesh_component = _entity_registry->try_get<core::mesh_component>(e);
                const auto material_component = _entity_registry->try_get<core::material_component>(e);

                // Both are needed to render the object
                if (mesh_component == nullptr || material_component == nullptr)
                {
                    continue;
                }

                // Make sure the GUIDs are both valid
                const auto mesh_opt = meshes.find(mesh_component->mesh_id);
                const auto material_opt = materials.find(material_component->material_id);

                if (!mesh_opt.has_value() || !material_opt.has_value())
                {
                    continue;
                }

                // Add the mesh and material GUIDs to the vectors
                mesh_guids.push_back(mesh_component->mesh_id);
                material_guids.push_back(material_component->material_id);

                const auto& material = *material_opt;

                if (const auto base_color = material.get_texture(core::material::base_color_texture_name))
                {
                    texture_guids.push_back(*base_color);
                }

                if (const auto mr_texture = material.get_texture(core::material::metallic_roughness_texture_name))
                {
                    texture_guids.push_back(*mr_texture);
                }

                if (const auto normal_texture = material.get_texture(core::material::normal_texture_name))
                {
                    texture_guids.push_back(*normal_texture);
                }

                if (const auto occlusion_texture = material.get_texture(core::material::occlusion_texture_name))
                {
                    texture_guids.push_back(*occlusion_texture);
                }

                if (const auto emissive_texture = material.get_texture(core::material::emissive_texture_name))
                {
                    texture_guids.push_back(*emissive_texture);
                }

                if (const auto transmissive_texture = material.get_texture(core::material::transmissive_texture_name))
                {
                    texture_guids.push_back(*transmissive_texture);
                }

                if (const auto volume_thickness_texture =
                        material.get_texture(core::material::volume_thickness_texture_name))
                {
                    texture_guids.push_back(*volume_thickness_texture);
                }
            }
        }

        // Meshs and textures need to be uploaded before materials, since materials relies on textures being written to
        // the CPU buffers
        _load_meshes(dev, mesh_guids, meshes);
        _load_textures(dev, texture_guids, textures, true);
        _load_materials(dev, material_guids, materials);

        // Build the render components
        for (const auto entity : entities)
        {
            const auto hierarchy_view = ecs::archetype_entity_hierarchy_view(*_entity_registry, entity);
            for (const auto e : hierarchy_view)
            {
                const auto mesh_component = _entity_registry->try_get<core::mesh_component>(e);
                const auto material_component = _entity_registry->try_get<core::material_component>(e);
                // Both are needed to render the object
                if (mesh_component == nullptr || material_component == nullptr)
                {
                    continue;
                }

                // Make sure the GUIDs are both valid
                const auto mesh_opt = meshes.find(mesh_component->mesh_id);
                const auto material_opt = materials.find(material_component->material_id);
                if (!mesh_opt.has_value() || !material_opt.has_value())
                {
                    continue;
                }

                // Build the renderable component
                const auto mesh_index = _meshes.mesh_to_index[mesh_component->mesh_id];
                const auto material_index = _materials.material_to_index[material_component->material_id];
                const auto is_double_side = material_opt->get_bool(core::material::double_sided_name).value_or(false);

                // Check if there is an existing renderable component
                const auto rc = _entity_registry->try_get<renderable_component>(e);
                const auto object_id = rc ? rc->object_id : _acquire_next_object();

                // Create the renderable component
                const auto renderable = renderable_component{
                    .mesh_id = static_cast<uint32_t>(mesh_index),
                    .material_id = static_cast<uint32_t>(material_index),
                    .object_id = object_id,
                    .double_sided = is_double_side,
                };

                _entity_registry->assign_or_replace(e, renderable);

                // If the object has no transform, assign the default transform
                if (!_entity_registry->has<ecs::transform_component>(e))
                {
                    _entity_registry->assign_or_replace(e, ecs::transform_component{});
                }
            }
        }
    }

    void pbr_pipeline::_load_meshes(rhi::device& dev, span<const guid> mesh_ids,
                                    const core::mesh_registry& mesh_registry)
    {
        flat_unordered_map<guid, mesh_layout> result;

        uint32_t bytes_written = 0;
        uint32_t vertex_bytes_required = 0;
        uint32_t layout_bytes_required = 0;

        for (const auto& mesh_id : mesh_ids)
        {
            optional<const core::mesh&> mesh_opt = mesh_registry.find(mesh_id);
            assert(mesh_opt.has_value());

            auto& mesh = *mesh_opt;

            // Compute vertex size in bytes
            auto vertex_size = sizeof(float) * 3    // position
                               + sizeof(float) * 3  // normal
                               + sizeof(float) * 2  // uv
                               + sizeof(float) * 4; // tangent
            if (mesh.has_colors)
            {
                vertex_size += sizeof(float) * 4; // color
            }

            vertex_bytes_required +=
                static_cast<uint32_t>(vertex_size * mesh.vertices.size() + sizeof(uint32_t) * mesh.indices.size());
            layout_bytes_required += static_cast<uint32_t>(sizeof(mesh_layout));
        }

        const auto total_bytes_required = vertex_bytes_required + layout_bytes_required;

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
            std::memcpy(dst + bytes_written, mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());

            bytes_written += static_cast<uint32_t>(sizeof(uint32_t) * mesh.indices.size());
        }

        // Write the layouts
        for (auto&& [guid, layout] : result)
        {
            std::memcpy(dst + bytes_written, &layout, sizeof(mesh_layout));
            bytes_written += static_cast<uint32_t>(sizeof(mesh_layout));

            _meshes.mesh_to_index.insert({guid, _meshes.meshes.size()});
            _meshes.meshes.push_back(layout);
        }

        // Flush the staging buffer
        dev.unmap_buffer(staging);
        dev.flush_buffers(span(&staging, 1));

        // Upload the staging buffer to the GPU
        auto& work_queue = dev.get_primary_work_queue();
        auto cmd_buf = work_queue.get_next_command_list();

        work_queue.begin_command_list(cmd_buf, true);
        work_queue.copy(cmd_buf, staging, _gpu_buffers.vertices, 0, _gpu_resource_usages.vertex_bytes_written,
                        vertex_bytes_required);
        work_queue.copy(cmd_buf, staging, _gpu_buffers.mesh_layouts, vertex_bytes_required,
                        _gpu_resource_usages.mesh_layout_bytes_written, layout_bytes_required);
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
        _gpu_resource_usages.mesh_layout_bytes_written += layout_bytes_required;
    }

    void pbr_pipeline::_load_textures([[maybe_unused]] rhi::device& dev, span<const guid> texture_ids,
                                      const core::texture_registry& texture_registry, bool generate_mip_maps)
    {
        // Ensure we aren't uploading existing textures
        vector<guid> next_texture_ids;
        for (const auto& tex_guid : texture_ids)
        {
            if (_bindless_textures.image_to_index.find(tex_guid) != _bindless_textures.image_to_index.end() ||
                tempest::find(next_texture_ids.begin(), next_texture_ids.end(), tex_guid) != next_texture_ids.end())
            {
                continue;
            }
            next_texture_ids.push_back(tex_guid);
        }

        // Create the images
        vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::image>> images;

        for (const auto& tex_guid : next_texture_ids)
        {
            auto texture_opt = texture_registry.get_texture(tex_guid);
            assert(texture_opt.has_value());

            const auto& texture = *texture_opt;
            const auto mip_count = generate_mip_maps ? bit_width(min(texture.width, texture.height))
                                                     : static_cast<std::uint32_t>(texture.mips.size());

            rhi::image_desc image_desc = {
                .format = convert_format(texture.format),
                .type = rhi::image_type::image_2d,
                .width = texture.width,
                .height = texture.height,
                .depth = 1,
                .array_layers = 1,
                .mip_levels = mip_count,
                .sample_count = rhi::image_sample_count::sample_count_1,
                .tiling = rhi::image_tiling_type::optimal,
                .location = rhi::memory_location::device,
                .usage = make_enum_mask(rhi::image_usage::sampled, rhi::image_usage::transfer_dst,
                                        rhi::image_usage::transfer_src),
                .name = texture.name,
            };

            auto image = dev.create_image(image_desc);
            images.push_back(image);
        }

        // Set up the staging buffer
        constexpr auto staging_buffer_size = 1024u * 1024 * 128; // 128 MB
        auto staging = dev.create_buffer({
            .size = staging_buffer_size,
            .location = rhi::memory_location::host,
            .usage = make_enum_mask(rhi::buffer_usage::transfer_src),
            .access_type = rhi::host_access_type::incoherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "Staging Buffer",
        });

        auto staging_ptr = dev.map_buffer(staging);

        // Get the command buffer ready
        auto& work_queue = dev.get_primary_work_queue();
        auto cmd_buf = work_queue.get_next_command_list();
        work_queue.begin_command_list(cmd_buf, true);

        uint32_t images_written = 0;
        size_t staging_bytes_written = 0;

        for (const auto& tex_guid : next_texture_ids)
        {
            auto texture_opt = texture_registry.get_texture(tex_guid);
            const auto& texture = *texture_opt;

            auto image = images[images_written];

            // Change to a general image layout to be prepared for the copy
            rhi::work_queue::image_barrier image_barrier = {
                .image = image,
                .old_layout = rhi::image_layout::undefined,
                .new_layout = rhi::image_layout::general,
                .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::all_transfer),
                .src_access = tempest::make_enum_mask(rhi::memory_access::none),
                .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::copy),
                .dst_access = tempest::make_enum_mask(rhi::memory_access::transfer_write),
            };

            work_queue.transition_image(cmd_buf, span(&image_barrier, 1));

            uint32_t mips_written = 0;

            for (const auto& mip : texture.mips)
            {
                // Ensure there is enough space in the staging buffer
                const auto bytes_in_mip = mip.data.size();
                const auto bytes_required = staging_bytes_written + bytes_in_mip;

                if (bytes_required > staging_buffer_size)
                {
                    dev.unmap_buffer(staging);
                    dev.flush_buffers(span(&staging, 1));

                    work_queue.end_command_list(cmd_buf);
                    auto finished = dev.create_fence({.signaled = false});

                    rhi::work_queue::submit_info submit_info = {};
                    submit_info.command_lists.push_back(cmd_buf);

                    work_queue.submit(span(&submit_info, 1), finished);

                    dev.wait(span(&finished, 1));

                    dev.destroy_fence(finished);

                    // Start a new command buffer
                    cmd_buf = work_queue.get_next_command_list();
                    work_queue.begin_command_list(cmd_buf, true);

                    staging_bytes_written = 0;
                }

                // Copy the mip data to the staging buffer
                std::memcpy(staging_ptr + staging_bytes_written, mip.data.data(), bytes_in_mip);

                work_queue.copy(cmd_buf, staging, image, rhi::image_layout::general,
                                static_cast<uint32_t>(staging_bytes_written), mips_written++);

                staging_bytes_written += bytes_in_mip;
            }

            ++images_written;
        }

        // Make sure to clean up and submit the final commands
        if (staging_bytes_written > 0)
        {
            dev.unmap_buffer(staging);
            dev.flush_buffers(span(&staging, 1));
            work_queue.end_command_list(cmd_buf);

            rhi::work_queue::submit_info submit_info = {};
            submit_info.command_lists.push_back(cmd_buf);
            auto finished = dev.create_fence({.signaled = false});

            work_queue.submit(span(&submit_info, 1), finished);
            dev.wait(span(&finished, 1));
            dev.destroy_fence(finished);
            dev.destroy_buffer(staging);
        }

        // Build out the image mips
        if (generate_mip_maps)
        {
            auto commands = work_queue.get_next_command_list();
            work_queue.begin_command_list(commands, true);

            auto image_index = 0;
            for (const auto& tex_guid : next_texture_ids)
            {
                auto texture_opt = texture_registry.get_texture(tex_guid);
                const auto& texture = *texture_opt;
                const auto& image = images[image_index];

                // Generate mip maps from the number of mips specified in the image source to the number of mips
                // requested for creation
                const auto max_mip_count = static_cast<uint32_t>(bit_width(min(texture.width, texture.height)));
                const auto mip_to_build_from = static_cast<std::uint32_t>(texture.mips.size()) - 1;
                const auto num_mips_to_generate = max_mip_count - mip_to_build_from;

                work_queue.generate_mip_chain(commands, image, rhi::image_layout::general, mip_to_build_from,
                                              num_mips_to_generate);

                // Transition the image to a shader read layout
                rhi::work_queue::image_barrier image_barrier = {
                    .image = image,
                    .old_layout = rhi::image_layout::general,
                    .new_layout = rhi::image_layout::shader_read_only,
                    .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::all_transfer),
                    .src_access =
                        tempest::make_enum_mask(rhi::memory_access::transfer_read, rhi::memory_access::transfer_write),
                    .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::vertex_shader,
                                                          rhi::pipeline_stage::fragment_shader,
                                                          rhi::pipeline_stage::compute_shader),
                    .dst_access = tempest::make_enum_mask(rhi::memory_access::shader_read),
                };

                work_queue.transition_image(commands, span(&image_barrier, 1));
            }

            work_queue.end_command_list(commands);
            rhi::work_queue::submit_info submit_info = {};
            submit_info.command_lists.push_back(commands);
            auto finished = dev.create_fence({.signaled = false});
            work_queue.submit(span(&submit_info, 1), finished);
            dev.wait(span(&finished, 1));
            dev.destroy_fence(finished);
        }

        size_t image_index = 0;
        for (const auto& guid : next_texture_ids)
        {
            _bindless_textures.image_to_index.insert({guid, _bindless_textures.images.size()});
            _bindless_textures.images.push_back(images[image_index++]);
        }

        _bindless_textures.last_updated_frame_index = _frame_number;
    }

    void pbr_pipeline::_load_materials([[maybe_unused]] rhi::device& dev,
                                       [[maybe_unused]] span<const guid> material_ids,
                                       [[maybe_unused]] const core::material_registry& material_registry)
    {
        for (const auto& guid : material_ids)
        {
            if (_materials.material_to_index.find(guid) != _materials.material_to_index.end())
            {
                continue;
            }

            const auto material_opt = material_registry.find(guid);
            if (!material_opt.has_value())
            {
                continue;
            }

            const auto& material = *material_opt;

            const auto base_color_factor =
                material.get_vec4(core::material::base_color_factor_name).value_or(math::vec4<float>{1.0f});
            const auto emissive_factor =
                material.get_vec3(core::material::emissive_factor_name).value_or(math::vec3<float>{0.0f});
            const auto normal_scale = material.get_scalar(core::material::normal_scale_name).value_or(1.0f);
            const auto metallic_factor = material.get_scalar(core::material::metallic_factor_name).value_or(1.0f);
            const auto roughness_factor = material.get_scalar(core::material::roughness_factor_name).value_or(1.0f);
            const auto alpha_cutoff = material.get_scalar(core::material::alpha_cutoff_name).value_or(0.0f);
            const auto transmissive_factor =
                material.get_scalar(core::material::transmissive_factor_name).value_or(0.0f);
            const auto thickness_factor =
                material.get_scalar(core::material::volume_thickness_factor_name).value_or(0.0f);
            const auto attenuation_distance =
                material.get_scalar(core::material::volume_attenuation_distance_name).value_or(0.0f);
            const auto attenuation_color =
                material.get_vec3(core::material::volume_attenuation_color_name).value_or(math::vec3<float>{0.0f});

            const auto material_type = [&material]() -> gpu::material_type {
                auto material_type_str = material.get_string(core::material::alpha_mode_name).value_or("OPAQUE");
                if (material_type_str == "OPAQUE")
                {
                    return gpu::material_type::opaque;
                }
                else if (material_type_str == "MASK")
                {
                    return gpu::material_type::mask;
                }
                else if (material_type_str == "BLEND")
                {
                    return gpu::material_type::blend;
                }
                else if (material_type_str == "TRANSMISSIVE")
                {
                    return gpu::material_type::transmissive;
                }
                else
                {
                    return gpu::material_type::opaque;
                }
            }();

            auto gpu_material = gpu::material_data{
                .base_color_factor = base_color_factor,
                .emissive_factor = math::vec4(emissive_factor.x, emissive_factor.y, emissive_factor.z, 1.0f),
                .attenuation_color = math::vec4(attenuation_color.x, attenuation_color.y, attenuation_color.z, 1.0f),
                .normal_scale = normal_scale,
                .metallic_factor = metallic_factor,
                .roughness_factor = roughness_factor,
                .alpha_cutoff = alpha_cutoff,
                .reflectance = 0.0f,
                .transmission_factor = transmissive_factor,
                .thickness_factor = thickness_factor,
                .attenuation_distance = attenuation_distance,
                .type = material_type,
            };

            if (const auto albedo_map = material.get_texture(core::material::base_color_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*albedo_map];
                gpu_material.base_color_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.base_color_texture_id = gpu::material_data::invalid_texture_id;
            }

            if (const auto metallic_map = material.get_texture(core::material::metallic_roughness_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*metallic_map];
                gpu_material.metallic_roughness_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.metallic_roughness_texture_id = gpu::material_data::invalid_texture_id;
            }

            if (const auto normal_map = material.get_texture(core::material::normal_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*normal_map];
                gpu_material.normal_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.normal_texture_id = gpu::material_data::invalid_texture_id;
            }

            if (const auto occlusion_map = material.get_texture(core::material::occlusion_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*occlusion_map];
                gpu_material.occlusion_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.occlusion_texture_id = gpu::material_data::invalid_texture_id;
            }

            if (const auto emissive_map = material.get_texture(core::material::emissive_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*emissive_map];
                gpu_material.emissive_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.emissive_texture_id = gpu::material_data::invalid_texture_id;
            }

            if (const auto transmissive_map = material.get_texture(core::material::transmissive_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*transmissive_map];
                gpu_material.transmission_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.transmission_texture_id = gpu::material_data::invalid_texture_id;
            }

            if (const auto thickness_map = material.get_texture(core::material::volume_thickness_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*thickness_map];
                gpu_material.thickness_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.thickness_texture_id = gpu::material_data::invalid_texture_id;
            }

            _materials.material_to_index.insert({guid, _materials.materials.size()});
            _materials.materials.push_back(gpu_material);
        }
    }

    uint32_t pbr_pipeline::_acquire_next_object() noexcept
    {
        return _object_count++;
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

        vector<rhi::image_format> color_formats(1);
        color_formats[0] = encoded_normals_format;

        // No blend on slim gbuffer
        vector<rhi::color_blend_attachment> blending(1);
        // Normals
        blending[0].blend_enable = false;

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
        _z_prepass.scene_constant_bytes_per_frame =
            math::round_to_next_multiple(sizeof(zprepass::scene_constants), 256);
        _z_prepass.scene_constants = dev.create_buffer({
            .size = static_cast<uint32_t>(_z_prepass.scene_constant_bytes_per_frame * dev.frames_in_flight()),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::constant, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::random,
            .name = "Z Prepass Scene Constants",
        });
    }

    void pbr_pipeline::_initialize_clustering([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        // Set up pipeline layout for build pass
        tempest::vector<rhi::descriptor_binding_layout> bindings;
        bindings.push_back(clusters::build_cluster_layout);

        auto build_layout = dev.create_descriptor_set_layout(bindings);

        tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> layouts;
        layouts.push_back(build_layout);

        tempest::vector<rhi::push_constant_range> push_constants;
        push_constants.push_back(clusters::build_cluster_grid_pc_range);

        auto build_pipeline_layout = dev.create_pipeline_layout({
            .descriptor_set_layouts = tempest::move(layouts),
            .push_constants = tempest::move(push_constants),
        });

        _forward_light_clustering.build_cluster_desc_set_0_layout = build_layout;
        _forward_light_clustering.build_cluster_layout = build_pipeline_layout;

        // Set up build pipeline
        auto build_source = core::read_bytes("assets/shaders/build_cluster_grid.comp.spv");

        auto build_pipeline = dev.create_compute_pipeline({
            .compute_shader = tempest::move(build_source),
            .layout = _forward_light_clustering.build_cluster_layout,
            .name = "Build Cluster Pipeline",
        });

        _forward_light_clustering.build_clusters = build_pipeline;

        bindings.clear();
        layouts.clear();
        push_constants.clear();

        // Set up pipeline layout for cull pass

        bindings.push_back(clusters::scene_constants_layout);
        bindings.push_back(clusters::cull_cluster_layout);
        bindings.push_back(clusters::lights_layout);
        bindings.push_back(clusters::global_light_index_list_layout);
        bindings.push_back(clusters::light_grid_layout);
        bindings.push_back(clusters::global_index_count);

        auto cull_layout = dev.create_descriptor_set_layout(bindings);
        layouts.push_back(cull_layout);

        push_constants.push_back(clusters::cull_lights_pc_range);

        auto cull_pipeline_layout = dev.create_pipeline_layout({
            .descriptor_set_layouts = tempest::move(layouts),
            .push_constants = tempest::move(push_constants),
        });

        _forward_light_clustering.fill_cluster_desc_set_0_layout = cull_layout;
        _forward_light_clustering.fill_cluster_layout = cull_pipeline_layout;

        // Set up cull pipeline
        auto cull_source = core::read_bytes("assets/shaders/cull_lights.comp.spv");
        auto cull_pipeline = dev.create_compute_pipeline({
            .compute_shader = tempest::move(cull_source),
            .layout = _forward_light_clustering.fill_cluster_layout,
            .name = "Cull Cluster Pipeline",
        });

        _forward_light_clustering.fill_clusters = cull_pipeline;
    }

    void pbr_pipeline::_initialize_pbr_opaque([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        tempest::vector<rhi::descriptor_binding_layout> bindings;
        bindings.reserve(8);
        bindings.push_back(pbr::scene_constants_layout);
        bindings.push_back(pbr::vertex_pull_buffer_layout);
        bindings.push_back(pbr::mesh_buffer_layout);
        bindings.push_back(pbr::object_buffer_layout);
        bindings.push_back(pbr::instance_buffer_layout);
        bindings.push_back(pbr::material_buffer_layout);
        bindings.push_back(pbr::ao_image_layout);
        bindings.push_back(pbr::linear_sampler_layout);
        bindings.push_back(pbr::bindless_textures_layout);
        auto set0_layout = dev.create_descriptor_set_layout(bindings);

        bindings.clear();
        bindings.push_back(pbr::lights_layout);
        bindings.push_back(pbr::light_params_layout);
        bindings.push_back(pbr::shadow_map_layout);
        bindings.push_back(pbr::light_grid_layout);
        bindings.push_back(pbr::global_index_count);
        auto set1_layout = dev.create_descriptor_set_layout(bindings);

        tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> layouts;
        layouts.push_back(set0_layout);
        layouts.push_back(set1_layout);
        auto pipeline_layout = dev.create_pipeline_layout({
            .descriptor_set_layouts = tempest::move(layouts),
            .push_constants = {},
        });
        _pbr_opaque.desc_set_0_layout = set0_layout;
        _pbr_opaque.desc_set_1_layout = set1_layout;
        _pbr_opaque.layout = pipeline_layout;

        auto vert_source = core::read_bytes("assets/shaders/pbr.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/pbr.frag.spv");

        vector<rhi::image_format> color_formats;
        color_formats.push_back(color_format);

        vector<rhi::color_blend_attachment> blending;
        blending.push_back(rhi::color_blend_attachment{
            .blend_enable = false,
            .src_color_blend_factor = rhi::blend_factor::src_alpha,
            .dst_color_blend_factor = rhi::blend_factor::one_minus_src_alpha,
            .color_blend_op = rhi::blend_op::add,
            .src_alpha_blend_factor = rhi::blend_factor::one,
            .dst_alpha_blend_factor = rhi::blend_factor::one_minus_src_alpha,
            .alpha_blend_op = rhi::blend_op::add,
        });

        rhi::graphics_pipeline_desc pipeline_desc = {
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
            .layout = _pbr_opaque.layout,
            .name = "PBR Opaque Pipeline",
        };

        _pbr_opaque.pipeline = dev.create_graphics_pipeline(tempest::move(pipeline_desc));
    }

    void pbr_pipeline::_initialize_pbr_transparent([[maybe_unused]] renderer& parent, [[maybe_unused]] rhi::device& dev)
    {
        // Set up the gather pass
        {
            vector<rhi::descriptor_binding_layout> set_0_bindings;
            set_0_bindings.push_back(pbr_transparencies::scene_constants_layout);
            set_0_bindings.push_back(pbr_transparencies::vertex_pull_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::mesh_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::object_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::instance_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::material_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::oit_image_layout);
            set_0_bindings.push_back(pbr_transparencies::oit_zeroth_image_layout);
            set_0_bindings.push_back(pbr_transparencies::ao_image_layout);
            set_0_bindings.push_back(pbr_transparencies::linear_sampler_layout);
            set_0_bindings.push_back(pbr_transparencies::bindless_textures_layout);

            auto set_0_layout = dev.create_descriptor_set_layout(set_0_bindings);

            vector<rhi::descriptor_binding_layout> set_1_bindings;
            set_1_bindings.push_back(pbr_transparencies::lights_layout);
            set_1_bindings.push_back(pbr_transparencies::light_params_layout);
            set_1_bindings.push_back(pbr_transparencies::shadow_map_layout);
            set_1_bindings.push_back(pbr_transparencies::light_grid_layout);
            set_1_bindings.push_back(pbr_transparencies::global_index_count);

            auto set_1_layout = dev.create_descriptor_set_layout(set_1_bindings);

            tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> layouts;
            layouts.push_back(set_0_layout);
            layouts.push_back(set_1_layout);

            auto pipeline_layout = dev.create_pipeline_layout({
                .descriptor_set_layouts = tempest::move(layouts),
                .push_constants = {},
            });

            _pbr_transparencies.oit_gather_desc_set_0_layout = set_0_layout;
            _pbr_transparencies.oit_gather_desc_set_1_layout = set_1_layout;
            _pbr_transparencies.oit_gather_layout = pipeline_layout;

            auto vert_source = core::read_bytes("assets/shaders/pbr_oit_gather.vert.spv");
            auto frag_source = core::read_bytes("assets/shaders/pbr_oit_gather.frag.spv");

            vector<rhi::image_format> color_formats;
            color_formats.push_back(transparency_accumulator_format);

            vector<rhi::color_blend_attachment> blending;
            blending.push_back(rhi::color_blend_attachment{
                .blend_enable = false,
                .src_color_blend_factor = rhi::blend_factor::src_alpha,
                .dst_color_blend_factor = rhi::blend_factor::one_minus_src_alpha,
                .color_blend_op = rhi::blend_op::add,
                .src_alpha_blend_factor = rhi::blend_factor::one,
                .dst_alpha_blend_factor = rhi::blend_factor::one_minus_src_alpha,
                .alpha_blend_op = rhi::blend_op::add,
            });

            rhi::graphics_pipeline_desc desc = {
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
                                .write_enable = false,
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
                .layout = pipeline_layout,
                .name = "PBR MBOIT Gather Pipeline",
            };

            _pbr_transparencies.oit_gather_pipeline = dev.create_graphics_pipeline(tempest::move(desc));
        }

        // Set up resolve pass
        {
            vector<rhi::descriptor_binding_layout> set_0_bindings;
            set_0_bindings.push_back(pbr_transparencies::scene_constants_layout);
            set_0_bindings.push_back(pbr_transparencies::vertex_pull_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::mesh_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::object_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::instance_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::material_buffer_layout);
            set_0_bindings.push_back(pbr_transparencies::oit_image_layout);
            set_0_bindings.push_back(pbr_transparencies::oit_zeroth_image_layout);
            set_0_bindings.push_back(pbr_transparencies::ao_image_layout);
            set_0_bindings.push_back(pbr_transparencies::linear_sampler_layout);
            set_0_bindings.push_back(pbr_transparencies::bindless_textures_layout);

            auto set_0_layout = dev.create_descriptor_set_layout(set_0_bindings);

            vector<rhi::descriptor_binding_layout> set_1_bindings;
            set_1_bindings.push_back(pbr_transparencies::lights_layout);
            set_1_bindings.push_back(pbr_transparencies::light_params_layout);
            set_1_bindings.push_back(pbr_transparencies::shadow_map_layout);
            set_1_bindings.push_back(pbr_transparencies::light_grid_layout);
            set_1_bindings.push_back(pbr_transparencies::global_index_count);

            auto set_1_layout = dev.create_descriptor_set_layout(set_1_bindings);

            tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> layouts;
            layouts.push_back(set_0_layout);
            layouts.push_back(set_1_layout);

            auto pipeline_layout = dev.create_pipeline_layout({
                .descriptor_set_layouts = tempest::move(layouts),
                .push_constants = {},
            });

            _pbr_transparencies.oit_resolve_desc_set_0_layout = set_0_layout;
            _pbr_transparencies.oit_resolve_desc_set_1_layout = set_1_layout;
            _pbr_transparencies.oit_resolve_layout = pipeline_layout;

            auto vert_source = core::read_bytes("assets/shaders/pbr_oit_resolve.vert.spv");
            auto frag_source = core::read_bytes("assets/shaders/pbr_oit_resolve.frag.spv");

            vector<rhi::image_format> color_formats;
            color_formats.push_back(transparency_accumulator_format);

            vector<rhi::color_blend_attachment> blending;
            blending.push_back(rhi::color_blend_attachment{
                .blend_enable = true,
                .src_color_blend_factor = rhi::blend_factor::one,
                .dst_color_blend_factor = rhi::blend_factor::one,
                .color_blend_op = rhi::blend_op::add,
                .src_alpha_blend_factor = rhi::blend_factor::one,
                .dst_alpha_blend_factor = rhi::blend_factor::one,
                .alpha_blend_op = rhi::blend_op::add,
            });

            rhi::graphics_pipeline_desc desc = {
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
                                .write_enable = false,
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
                .layout = pipeline_layout,
                .name = "PBR MBOIT Resolve Pipeline",
            };

            _pbr_transparencies.oit_resolve_pipeline = dev.create_graphics_pipeline(tempest::move(desc));
        }

        // Set up blend pass
        {
            vector<rhi::descriptor_binding_layout> set_0_bindings;
            set_0_bindings.push_back(pbr_transparencies::blend_moments_layout);
            set_0_bindings.push_back(pbr_transparencies::blend_moments_zeroth_layout);
            set_0_bindings.push_back(pbr_transparencies::blend_transparency_accumulator_layout);
            set_0_bindings.push_back(pbr_transparencies::blend_linear_sampler_layout);

            auto set_0_layout = dev.create_descriptor_set_layout(set_0_bindings);

            tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> layouts;
            layouts.push_back(set_0_layout);

            auto pipeline_layout = dev.create_pipeline_layout({
                .descriptor_set_layouts = tempest::move(layouts),
                .push_constants = {},
            });

            _pbr_transparencies.oit_blend_desc_set_0_layout = set_0_layout;
            _pbr_transparencies.oit_blend_layout = pipeline_layout;

            auto vert_source = core::read_bytes("assets/shaders/pbr_oit_blend.vert.spv");
            auto frag_source = core::read_bytes("assets/shaders/pbr_oit_blend.frag.spv");

            vector<rhi::image_format> color_formats;
            color_formats.push_back(transparency_accumulator_format);

            vector<rhi::color_blend_attachment> blending;
            blending.push_back(rhi::color_blend_attachment{
                .blend_enable = true,
                .src_color_blend_factor = rhi::blend_factor::src_alpha,
                .dst_color_blend_factor = rhi::blend_factor::one_minus_src_alpha,
                .color_blend_op = rhi::blend_op::add,
                .src_alpha_blend_factor = rhi::blend_factor::one,
                .dst_alpha_blend_factor = rhi::blend_factor::one_minus_src_alpha,
                .alpha_blend_op = rhi::blend_op::add,
            });

            rhi::graphics_pipeline_desc desc = {
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
                        .depth = none(),
                        .stencil = none(),
                    },
                .color_blend =
                    {
                        .attachments = tempest::move(blending),
                        .blend_constants = {},
                    },
                .layout = pipeline_layout,
                .name = "PBR MBOIT Blend Pipeline",
            };

            _pbr_transparencies.oit_blend_pipeline = dev.create_graphics_pipeline(tempest::move(desc));
        }
    }

    void pbr_pipeline::_initialize_shadows([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        vector<rhi::descriptor_binding_layout> bindings;
        bindings.reserve(8);
        bindings.push_back(shadows::vertex_pull_buffer_layout);
        bindings.push_back(shadows::mesh_buffer_layout);
        bindings.push_back(shadows::object_buffer_layout);
        bindings.push_back(shadows::instance_buffer_layout);
        bindings.push_back(shadows::material_buffer_layout);
        bindings.push_back(shadows::linear_sampler_layout);
        bindings.push_back(shadows::bindless_textures_layout);

        const auto set_0_layout = dev.create_descriptor_set_layout(bindings);
        _shadows.directional_desc_set_0_layout = set_0_layout;

        vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> desc_set_layouts;
        desc_set_layouts.push_back(set_0_layout);

        vector<rhi::push_constant_range> push_constants;
        push_constants.push_back(shadows::light_matrix_pc_range);

        const auto shadow_pipeline_layout = dev.create_pipeline_layout({
            .descriptor_set_layouts = std::move(desc_set_layouts),
            .push_constants = tempest::move(push_constants),
        });
        _shadows.directional_layout = shadow_pipeline_layout;

        auto vert_source = core::read_bytes("assets/shaders/directional_shadow_map.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/directional_shadow_map.frag.spv");

        rhi::graphics_pipeline_desc desc = {
            .color_attachment_formats = {},
            .depth_attachment_format = pbr_pipeline::shadow_megatexture_format,
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
                            .compare_op = rhi::compare_op::less,
                            .depth_bounds_test_enable = false,
                            .min_depth_bounds = 0.0f,
                            .max_depth_bounds = 1.0f,
                        },
                    .stencil = none(),
                },
            .color_blend =
                {
                    .attachments = {},
                    .blend_constants = {},
                },
            .layout = shadow_pipeline_layout,
            .name = "Shadow Pipeline",
        };

        _shadows.directional_pipeline = dev.create_graphics_pipeline(tempest::move(desc));
    }

    void pbr_pipeline::_initialize_ssao([[maybe_unused]] renderer& parent, [[maybe_unused]] rhi::device& dev)
    {
        // Set up ssao pipeline
        {
            vector<rhi::descriptor_binding_layout> set_0_bindings;
            set_0_bindings.push_back(ssao::scene_constants_layout);
            set_0_bindings.push_back(ssao::depth_buffer_layout);
            set_0_bindings.push_back(ssao::normal_buffer_layout);
            set_0_bindings.push_back(ssao::noise_buffer_layout);
            set_0_bindings.push_back(ssao::linear_sampler_layout);
            set_0_bindings.push_back(ssao::point_sampler_layout);

            auto set_0_layout = dev.create_descriptor_set_layout(set_0_bindings);

            tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> layouts;
            layouts.push_back(set_0_layout);

            auto pipeline_layout = dev.create_pipeline_layout({
                .descriptor_set_layouts = tempest::move(layouts),
                .push_constants = {},
            });

            _ssao.ssao_desc_set_0_layout = set_0_layout;
            _ssao.ssao_layout = pipeline_layout;

            auto vert_source = core::read_bytes("assets/shaders/ssao.vert.spv");
            auto frag_source = core::read_bytes("assets/shaders/ssao.frag.spv");

            vector<rhi::image_format> color_formats;
            color_formats.push_back(ssao_format);

            vector<rhi::color_blend_attachment> blending;
            blending.push_back(rhi::color_blend_attachment{
                .blend_enable = false,
                .src_color_blend_factor = rhi::blend_factor::zero,
                .dst_color_blend_factor = rhi::blend_factor::zero,
                .color_blend_op = rhi::blend_op::add,
                .src_alpha_blend_factor = rhi::blend_factor::zero,
                .dst_alpha_blend_factor = rhi::blend_factor::zero,
                .alpha_blend_op = rhi::blend_op::add,
            });

            rhi::graphics_pipeline_desc desc = {
                .color_attachment_formats = tempest::move(color_formats),
                .depth_attachment_format = none(),
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
                        .depth = none(),
                        .stencil = none(),
                    },
                .color_blend =
                    {
                        .attachments = tempest::move(blending),
                        .blend_constants = {},
                    },
                .layout = pipeline_layout,
                .name = "SSAO Pipeline",
            };

            _ssao.ssao_pipeline = dev.create_graphics_pipeline(tempest::move(desc));

            _ssao.scene_constants = dev.create_buffer({
                .size = static_cast<uint32_t>(math::round_to_next_multiple(sizeof(ssao::scene_constants), 256)),
                .location = rhi::memory_location::automatic,
                .usage = make_enum_mask(rhi::buffer_usage::constant, rhi::buffer_usage::transfer_dst),
                .access_type = rhi::host_access_type::incoherent,
                .access_pattern = rhi::host_access_pattern::sequential,
                .name = "SSAO Scene Constants",
            });
        }

        // Set up ssao blur pipeline
        {
            vector<rhi::descriptor_binding_layout> set_0_bindings;
            set_0_bindings.push_back(ssao::ssao_input_layout);
            set_0_bindings.push_back(ssao::blur_point_sampler_layout);

            auto set_0_layout = dev.create_descriptor_set_layout(set_0_bindings);

            tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> layouts;
            layouts.push_back(set_0_layout);

            auto pipeline_layout = dev.create_pipeline_layout({
                .descriptor_set_layouts = tempest::move(layouts),
                .push_constants = {},
            });

            _ssao.ssao_blur_desc_set_0_layout = set_0_layout;
            _ssao.ssao_blur_layout = pipeline_layout;

            auto vert_source = core::read_bytes("assets/shaders/ssao_blur.vert.spv");
            auto frag_source = core::read_bytes("assets/shaders/ssao_blur.frag.spv");

            vector<rhi::image_format> color_formats;
            color_formats.push_back(ssao_format);

            vector<rhi::color_blend_attachment> blending;
            blending.push_back(rhi::color_blend_attachment{
                .blend_enable = false,
                .src_color_blend_factor = rhi::blend_factor::zero,
                .dst_color_blend_factor = rhi::blend_factor::zero,
                .color_blend_op = rhi::blend_op::add,
                .src_alpha_blend_factor = rhi::blend_factor::zero,
                .dst_alpha_blend_factor = rhi::blend_factor::zero,
                .alpha_blend_op = rhi::blend_op::add,
            });

            rhi::graphics_pipeline_desc desc = {
                .color_attachment_formats = tempest::move(color_formats),
                .depth_attachment_format = none(),
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
                        .depth = none(),
                        .stencil = none(),
                    },
                .color_blend =
                    {
                        .attachments = tempest::move(blending),
                        .blend_constants = {},
                    },
                .layout = pipeline_layout,
                .name = "SSAO Blur Pipeline",
            };

            _ssao.ssao_blur_pipeline = dev.create_graphics_pipeline(tempest::move(desc));
        }
    }

    void pbr_pipeline::_initialize_skybox([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        vector<rhi::descriptor_binding_layout> bindings;
        bindings.push_back(skybox::scene_constants_layout);
        bindings.push_back(skybox::skybox_texture_layout);
        bindings.push_back(skybox::linear_sampler_layout);

        auto set_0_layout = dev.create_descriptor_set_layout(bindings);

        vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>> layouts;
        layouts.push_back(set_0_layout);

        auto pipeline_layout = dev.create_pipeline_layout({
            .descriptor_set_layouts = tempest::move(layouts),
            .push_constants = {},
        });

        _skybox.desc_set_0_layout = set_0_layout;
        _skybox.layout = pipeline_layout;

        auto vert_source = core::read_bytes("assets/shaders/skybox.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/skybox.frag.spv");

        vector<rhi::image_format> color_formats;
        color_formats.push_back(color_format);

        vector<rhi::color_blend_attachment> blending;
        blending.push_back(rhi::color_blend_attachment{
            .blend_enable = false,
            .src_color_blend_factor = rhi::blend_factor::zero,
            .dst_color_blend_factor = rhi::blend_factor::zero,
            .color_blend_op = rhi::blend_op::add,
            .src_alpha_blend_factor = rhi::blend_factor::zero,
            .dst_alpha_blend_factor = rhi::blend_factor::zero,
            .alpha_blend_op = rhi::blend_op::add,
        });

        rhi::graphics_pipeline_desc desc = {
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
                    .cull_mode = make_enum_mask(rhi::cull_mode::front),
                    .vertex_winding = rhi::vertex_winding::counter_clockwise,
                    .depth_bias = none(),
                    .line_width = 1.0f,
                },
            .depth_stencil =
                {
                    .depth =
                        rhi::depth_test{
                            .write_enable = false,
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
            .layout = pipeline_layout,
            .name = "Skybox Pipeline",
        };

        _skybox.pipeline = dev.create_graphics_pipeline(tempest::move(desc));
    }

    void pbr_pipeline::_initialize_samplers([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        const auto linear_with_aniso = rhi::sampler_desc{
            .mag = rhi::filter::linear,
            .min = rhi::filter::linear,
            .mipmap = rhi::mipmap_mode::linear,
            .address_u = rhi::address_mode::repeat,
            .address_v = rhi::address_mode::repeat,
            .address_w = rhi::address_mode::repeat,
            .mip_lod_bias = 0.0f,
            .min_lod = 0.0f,
            .max_lod = 32.0f,
            .max_anisotropy = 16.0f,
            .compare = none(),
            .name = "Linear Anisotropic Sampler",
        };

        _bindless_textures.linear_sampler = dev.create_sampler(linear_with_aniso);

        const auto linear = rhi::sampler_desc{
            .mag = rhi::filter::linear,
            .min = rhi::filter::linear,
            .mipmap = rhi::mipmap_mode::linear,
            .address_u = rhi::address_mode::repeat,
            .address_v = rhi::address_mode::repeat,
            .address_w = rhi::address_mode::repeat,
            .mip_lod_bias = 0.0f,
            .min_lod = 0.0f,
            .max_lod = 32.0f,
            .max_anisotropy = none(),
            .compare = none(),
            .name = "Linear Sampler",
        };

        _bindless_textures.linear_sampler_no_aniso = dev.create_sampler(linear);

        const auto point_with_aniso = rhi::sampler_desc{
            .mag = rhi::filter::nearest,
            .min = rhi::filter::nearest,
            .mipmap = rhi::mipmap_mode::nearest,
            .address_u = rhi::address_mode::repeat,
            .address_v = rhi::address_mode::repeat,
            .address_w = rhi::address_mode::repeat,
            .mip_lod_bias = 0.0f,
            .min_lod = 0.0f,
            .max_lod = 32.0f,
            .max_anisotropy = 16.0f,
            .compare = none(),
            .name = "Point Anisotropic Sampler",
        };

        _bindless_textures.point_sampler = dev.create_sampler(point_with_aniso);

        const auto point = rhi::sampler_desc{
            .mag = rhi::filter::nearest,
            .min = rhi::filter::nearest,
            .mipmap = rhi::mipmap_mode::nearest,
            .address_u = rhi::address_mode::repeat,
            .address_v = rhi::address_mode::repeat,
            .address_w = rhi::address_mode::repeat,
            .mip_lod_bias = 0.0f,
            .min_lod = 0.0f,
            .max_lod = 32.0f,
            .max_anisotropy = none(),
            .compare = none(),
            .name = "Point Sampler",
        };

        _bindless_textures.point_sampler_no_aniso = dev.create_sampler(point);
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

        rhi::image_desc transparency_accumulation_image_desc = {
            .format = transparency_accumulator_format,
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
            .name = "Transparency Accumulation Texture",
        };

        rhi::image_desc shadow_megatexture_image_desc = {
            .format = shadow_megatexture_format,
            .type = rhi::image_type::image_2d,
            .width = _shadows.image_region_allocator.extent().x,
            .height = _shadows.image_region_allocator.extent().y,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::depth_attachment, rhi::image_usage::sampled),
            .name = "Shadow Megatexture",
        };

        _render_targets.depth = dev.create_image(depth_image_desc);
        _render_targets.color = dev.create_image(color_image_desc);
        _render_targets.encoded_normals = dev.create_image(encoded_normals_image_desc);
        _render_targets.transparency_accumulator = dev.create_image(transparency_accumulation_image_desc);
        _render_targets.shadow_megatexture = dev.create_image(shadow_megatexture_image_desc);
    }

    void pbr_pipeline::_initialize_gpu_buffers([[maybe_unused]] renderer& parent, rhi::device& dev)
    {
        constexpr auto staging_size = math::round_to_next_multiple(64 * 1024 * 1024, 256);

        auto staging = dev.create_buffer({
            .size = staging_size * dev.frames_in_flight(),
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
        constexpr auto vertex_buffer_size = math::round_to_next_multiple(256 * 1024 * 1024, 256);

        auto vertex_buffer = dev.create_buffer({
            .size = vertex_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst,
                                    rhi::buffer_usage::index),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Vertex Buffer",
        });

        _gpu_resource_usages.vertex_bytes_written = 0;
        _gpu_buffers.vertices = vertex_buffer;

        // Set up mesh layout buffer
        constexpr auto mesh_layout_buffer_size = math::round_to_next_multiple(sizeof(mesh_layout) * 64 * 1024, 256);

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
        _gpu_buffers.scene_constants_bytes_per_frame = math::round_to_next_multiple(sizeof(gpu::scene_data), 256);
        const auto scene_buffer_size = _gpu_buffers.scene_constants_bytes_per_frame * dev.frames_in_flight();

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
        constexpr auto material_buffer_size = math::round_to_next_multiple(sizeof(gpu::material_data) * 64 * 1024, 256);

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
        _gpu_buffers.instance_bytes_per_frame = math::round_to_next_multiple(sizeof(uint32_t) * 64 * 1024, 256);
        const auto instance_buffer_size = _gpu_buffers.instance_bytes_per_frame * dev.frames_in_flight();

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
        _gpu_buffers.object_bytes_per_frame = math::round_to_next_multiple(sizeof(gpu::object_data) * 64 * 1024, 256);
        const auto object_buffer_size = _gpu_buffers.object_bytes_per_frame * dev.frames_in_flight();

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
        _cpu_buffers.indirect_command_bytes_per_frame =
            static_cast<uint32_t>(sizeof(gpu::indexed_indirect_command) * 64 * 1024);
        const auto indirect_command_buffer_size =
            _cpu_buffers.indirect_command_bytes_per_frame * dev.frames_in_flight();

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

    void pbr_pipeline::_prepare_draw_batches(
        [[maybe_unused]] renderer& parent, [[maybe_unused]] rhi::device& dev, [[maybe_unused]] const render_state& rs,
        [[maybe_unused]] rhi::work_queue& queue,
        [[maybe_unused]] rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands)
    {
        for (auto&& [_, draw_batch] : _cpu_buffers.draw_batches)
        {
            draw_batch.commands.clear();
        }

        _entity_registry->each([&](renderable_component renderable, ecs::self_component self) {
            auto object_payload = gpu::object_data{
                .model = math::mat4<float>(1.0f),
                .inv_tranpose_model = math::mat4<float>(1.0f),
                .mesh_id = static_cast<uint32_t>(renderable.mesh_id),
                .material_id = static_cast<uint32_t>(renderable.material_id),
                .parent_id = ~0u,
                .self_id = static_cast<uint32_t>(renderable.object_id),
            };

            auto ancestor_view = ecs::archetype_entity_ancestor_view(*_entity_registry, self.entity);
            for (auto ancestor : ancestor_view)
            {
                if (auto tx = _entity_registry->try_get<ecs::transform_component>(ancestor))
                {
                    object_payload.model = tx->matrix() * object_payload.model;
                }
            }

            object_payload.inv_tranpose_model = math::transpose(math::inverse(object_payload.model));

            auto alpha = static_cast<alpha_behavior>(_materials.materials[renderable.material_id].type);

            auto key = draw_batch_key{
                .alpha_type = alpha,
                .double_sided = renderable.double_sided,
            };

            auto& batch = _cpu_buffers.draw_batches[key];
            const auto& mesh = _meshes.meshes[renderable.mesh_id];

            if (batch.objects.find(self.entity) == batch.objects.end())
            {
                batch.objects.insert(self.entity, object_payload);
            }
            else
            {
                batch.objects[self.entity] = object_payload;
            }

            // Insert the command into the batch
            batch.commands.push_back({
                .index_count = mesh.index_count,
                .instance_count = 1,
                .first_index = (mesh.mesh_start_offset + mesh.index_offset) / static_cast<uint32_t>(sizeof(uint32_t)),
                .vertex_offset = 0,
                .first_instance = static_cast<uint32_t>(batch.objects.index_of(self.entity)),

            });
        });

        auto instance_written_count = 0u;
        for (auto [_, batch] : _cpu_buffers.draw_batches)
        {
            for (auto& cmd : batch.commands)
            {
                cmd.first_instance += instance_written_count;
            }

            instance_written_count += static_cast<uint32_t>(batch.objects.size());
        }

        // Upload to the GPU
        const auto staging_buffer = _gpu_buffers.staging;
        const auto staging_offset =
            _gpu_resource_usages.staging_bytes_writen + _gpu_resource_usages.staging_bytes_available * _frame_in_flight;
        auto staging_buffer_bytes = dev.map_buffer(staging_buffer) + staging_offset;
        size_t local_bytes_written = 0u;

        // Set up a barrier before writing to the staging buffer
        const array pre_staging_writes = {
            pre_host_write_buffer_barrier(_gpu_buffers.staging, staging_offset,
                                          _gpu_resource_usages.staging_bytes_available),
        };

        queue.pipeline_barriers(commands, {}, pre_staging_writes);

        size_t object_data_bytes_written = 0;
        size_t instance_data_bytes_written = 0;
        size_t indirect_command_bytes_written = 0;

        // Write object data
        for (auto&& [_, batch] : _cpu_buffers.draw_batches)
        {
            std::memcpy(staging_buffer_bytes + local_bytes_written, batch.objects.values(),
                        batch.objects.size() * sizeof(gpu::object_data));
            local_bytes_written += batch.objects.size() * sizeof(gpu::object_data);
            object_data_bytes_written += batch.objects.size() * sizeof(gpu::object_data);
        }

        // Write instance data
        uint32_t instances_written = 0;
        for (auto&& [_, batch] : _cpu_buffers.draw_batches)
        {
            vector<uint32_t> instance_indices(batch.objects.size());
            std::iota(instance_indices.begin(), instance_indices.end(), instances_written);

            std::memcpy(staging_buffer_bytes + local_bytes_written, instance_indices.data(),
                        batch.objects.size() * sizeof(uint32_t));
            local_bytes_written += batch.objects.size() * sizeof(uint32_t);
            instance_data_bytes_written += batch.objects.size() * sizeof(uint32_t);
            instances_written += static_cast<uint32_t>(batch.objects.size());
        }

        // Write indirect command data
        for (auto&& [_, batch] : _cpu_buffers.draw_batches)
        {
            std::memcpy(staging_buffer_bytes + local_bytes_written, batch.commands.data(),
                        batch.commands.size() * sizeof(gpu::indexed_indirect_command));
            local_bytes_written += batch.commands.size() * sizeof(gpu::indexed_indirect_command);
            indirect_command_bytes_written += batch.commands.size() * sizeof(gpu::indexed_indirect_command);
        }

        // Put barriers on the objects, instances, and commands to ensure host write visibility
        const array pre_staging_uploads = {
            pre_staging_to_dst_buffer_barrier(_gpu_buffers.objects,
                                              _gpu_buffers.object_bytes_per_frame * _frame_in_flight,
                                              _gpu_buffers.object_bytes_per_frame),
            pre_staging_to_dst_buffer_barrier(_gpu_buffers.instances,
                                              _gpu_buffers.instance_bytes_per_frame * _frame_in_flight,
                                              _gpu_buffers.instance_bytes_per_frame),
            pre_staging_to_dst_buffer_barrier(_gpu_buffers.indirect_commands,
                                              _cpu_buffers.indirect_command_bytes_per_frame * _frame_in_flight,
                                              _cpu_buffers.indirect_command_bytes_per_frame),
        };

        queue.pipeline_barriers(commands, {}, pre_staging_uploads);

        // Copy the data from the staging buffer to the GPU buffers
        const auto object_data_byte_offset = _gpu_resource_usages.staging_bytes_writen;
        const auto instance_data_byte_offset = object_data_byte_offset + object_data_bytes_written;
        const auto indirect_command_data_byte_offset = instance_data_byte_offset + instance_data_bytes_written;

        queue.copy(commands, staging_buffer, _gpu_buffers.objects, object_data_byte_offset,
                   _gpu_buffers.object_bytes_per_frame * _frame_in_flight, object_data_bytes_written);
        queue.copy(commands, staging_buffer, _gpu_buffers.instances, instance_data_byte_offset,
                   _gpu_buffers.instance_bytes_per_frame * _frame_in_flight, instance_data_bytes_written);
        queue.copy(commands, staging_buffer, _gpu_buffers.indirect_commands, indirect_command_data_byte_offset,
                   _cpu_buffers.indirect_command_bytes_per_frame * _frame_in_flight, indirect_command_bytes_written);

        // Set up a barrier after writing to the GPU buffers
        const array post_staging_uploads = {
            post_staging_to_dst_buffer_barrier(_gpu_buffers.objects,
                                               _gpu_buffers.object_bytes_per_frame * _frame_in_flight,
                                               _gpu_buffers.object_bytes_per_frame),
            post_staging_to_dst_buffer_barrier(_gpu_buffers.instances,
                                               _gpu_buffers.instance_bytes_per_frame * _frame_in_flight,
                                               _gpu_buffers.instance_bytes_per_frame),
            post_staging_to_dst_buffer_barrier(_gpu_buffers.indirect_commands,
                                               _cpu_buffers.indirect_command_bytes_per_frame * _frame_in_flight,
                                               _cpu_buffers.indirect_command_bytes_per_frame),
        };
        queue.pipeline_barriers(commands, {}, post_staging_uploads);

        // Update the bytes written
        _gpu_resource_usages.staging_bytes_writen += static_cast<uint32_t>(local_bytes_written);
    }

    void pbr_pipeline::_draw_z_prepass([[maybe_unused]] renderer& parent, [[maybe_unused]] rhi::device& dev,
                                       [[maybe_unused]] const render_state& rs, rhi::work_queue& queue,
                                       rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands)
    {
        // Prepare the descriptor sets
        // If the buffers changed or the bindless texture array changed, rewrite the descriptor sets
        if (_z_prepass.last_binding_update_frame >= _frame_number ||
            _bindless_textures.last_updated_frame_index >= _frame_number)
        {
            auto ds_desc = rhi::descriptor_set_desc{};

            // Scene Constants
            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 0,
                .type = rhi::descriptor_type::dynamic_constant_buffer,
                .offset = 0,
                .size = static_cast<uint32_t>(math::round_to_next_multiple(sizeof(zprepass::scene_constants), 256)),
                .buffer = _z_prepass.scene_constants,
            });

            // Vertex + Index Buffer
            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 1,
                .type = rhi::descriptor_type::structured_buffer,
                .offset = 0,
                .size = _gpu_resource_usages.vertex_bytes_written,
                .buffer = _gpu_buffers.vertices,
            });

            // Meshes
            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 2,
                .type = rhi::descriptor_type::structured_buffer,
                .offset = 0,
                .size = static_cast<uint32_t>(_meshes.meshes.size() * sizeof(mesh_layout)),
                .buffer = _gpu_buffers.mesh_layouts,
            });

            // Objects
            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 3,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .offset = 0,
                .size =
                    static_cast<uint32_t>(math::round_to_next_multiple(_object_count * sizeof(gpu::object_data), 256)),
                .buffer = _gpu_buffers.objects,
            });

            // Instances
            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 4,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .offset = 0,
                .size = static_cast<uint32_t>(math::round_to_next_multiple(_object_count * sizeof(uint32_t), 256)),
                .buffer = _gpu_buffers.instances,
            });

            // Materials
            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 5,
                .type = rhi::descriptor_type::structured_buffer,
                .offset = 0,
                .size = static_cast<uint32_t>(_materials.materials.size() * sizeof(gpu::material_data)),
                .buffer = _gpu_buffers.materials,
            });

            // Linear Sampler
            auto linear_sampler_desc = rhi::sampler_binding_descriptor{
                .index = 15,
                .samplers = {},
            };
            linear_sampler_desc.samplers.push_back(_bindless_textures.linear_sampler_no_aniso);

            ds_desc.samplers.push_back(tempest::move(linear_sampler_desc));

            // Images
            auto bindless_textures_desc = rhi::image_binding_descriptor{
                .index = 16,
                .type = rhi::descriptor_type::sampled_image,
                .array_offset = 0,
                .images = {},
            };

            for (const auto& img : _bindless_textures.images)
            {
                // TODO: Figure out what happens if there are "gaps" in the array
                bindless_textures_desc.images.push_back(rhi::image_binding_info{
                    .image = img,
                    .layout = rhi::image_layout::shader_read_only,
                });
            }

            ds_desc.images.push_back(tempest::move(bindless_textures_desc));
            ds_desc.layout = _z_prepass.desc_set_0_layout;

            // Create the descriptor set
            auto desc_set = dev.create_descriptor_set(ds_desc);
            dev.destroy_descriptor_set(_z_prepass.desc_set_0);
            _z_prepass.desc_set_0 = desc_set;
        }

        // Build out the camera data
        const auto byte_offset = _z_prepass.scene_constant_bytes_per_frame * _frame_in_flight;
        rhi::work_queue::buffer_barrier pre_scene_constants_upload = {
            .buffer = _z_prepass.scene_constants,
            .src_stages = make_enum_mask(rhi::pipeline_stage::all_transfer, rhi::pipeline_stage::fragment_shader),
            .src_access = make_enum_mask(rhi::memory_access::memory_read, rhi::memory_access::memory_write),
            .dst_stages = make_enum_mask(rhi::pipeline_stage::host),
            .dst_access = make_enum_mask(rhi::memory_access::host_write),
            .src_queue = nullptr,
            .dst_queue = nullptr,
            .offset = byte_offset,
            .size = _z_prepass.scene_constant_bytes_per_frame,
        };

        queue.pipeline_barriers(commands, {}, {&pre_scene_constants_upload, 1});

        const auto& camera_data = _entity_registry->get<camera_component>(_camera);
        const auto& camera_transform = _entity_registry->get<ecs::transform_component>(_camera);

        const auto quat_rot = math::quat(camera_transform.rotation());
        const auto f = math::extract_forward(quat_rot);
        const auto u = math::extract_up(quat_rot);

        const auto camera_view = math::look_at(camera_transform.position(), camera_transform.position() + f, u);
        const auto camera_projection = math::perspective(
            camera_data.aspect_ratio, camera_data.vertical_fov / camera_data.aspect_ratio, camera_data.near_plane);

        const auto scene_constants = zprepass::scene_constants{
            .camera =
                {
                    .proj = camera_projection,
                    .inv_proj = math::inverse(camera_projection),
                    .view = camera_view,
                    .inv_view = math::inverse(camera_view),
                    .position = camera_transform.position(),
                },
            .screen_size =
                math::vec2{static_cast<float>(_render_target_width), static_cast<float>(_render_target_height)},
        };

        auto scene_constants_buffer_bytes = dev.map_buffer(_z_prepass.scene_constants);
        std::memcpy(scene_constants_buffer_bytes + byte_offset, &scene_constants, sizeof(zprepass::scene_constants));
        dev.unmap_buffer(_z_prepass.scene_constants);

        // Barrier to wait for transfer operations to finish
        rhi::work_queue::buffer_barrier post_scene_constants_upload = {
            .buffer = _z_prepass.scene_constants,
            .src_stages = make_enum_mask(rhi::pipeline_stage::host),
            .src_access = make_enum_mask(rhi::memory_access::host_write),
            .dst_stages = make_enum_mask(rhi::pipeline_stage::vertex_shader),
            .dst_access = make_enum_mask(rhi::memory_access::constant_buffer_read),
            .src_queue = nullptr,
            .dst_queue = nullptr,
            .offset = byte_offset,
            .size = _z_prepass.scene_constant_bytes_per_frame,
        };

        queue.pipeline_barriers(commands, {}, {&post_scene_constants_upload, 1});

        // Barrier to wait for the encoded normals buffer to be done any previous operations
        rhi::work_queue::image_barrier undefined_to_encoded_normals_attachment = {
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
            undefined_to_encoded_normals_attachment,
            undefined_to_depth_attachment,
        };

        queue.transition_image(commands, barriers);

        rhi::work_queue::render_pass_info rpi = {
            .color_attachments = {},
            .depth_attachment =
                rhi::work_queue::depth_attachment_info{
                    .image = _render_targets.depth,
                    .layout = rhi::image_layout::depth,
                    .clear_depth = 0.0f,
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

        // Set up the draw state
        queue.bind(commands, _z_prepass.pipeline);
        queue.bind_index_buffer(commands, _gpu_buffers.vertices, 0, rhi::index_format::uint32);
        queue.set_cull_mode(commands, enum_mask(rhi::cull_mode::back));
        queue.set_viewport(commands, 0.0f, 0.0f, static_cast<float>(_render_target_width),
                           static_cast<float>(_render_target_height), 0.0f, 1.0f, 0, true);
        queue.set_scissor_region(commands, 0, 0, _render_target_width, _render_target_height, 0);

        // Set up dynamic offsets
        const auto scene_constants_offset =
            static_cast<uint32_t>(_z_prepass.scene_constant_bytes_per_frame * _frame_in_flight);
        const auto instance_offset = static_cast<uint32_t>(_gpu_buffers.instance_bytes_per_frame * _frame_in_flight);
        const auto object_offset = static_cast<uint32_t>(_gpu_buffers.object_bytes_per_frame * _frame_in_flight);

        array dynamic_offsets = {
            scene_constants_offset,
            object_offset,
            instance_offset,
        };

        queue.bind(commands, _z_prepass.layout, rhi::bind_point::graphics, 0, {&_z_prepass.desc_set_0, 1},
                   dynamic_offsets);

        const auto indirect_command_offset = _cpu_buffers.indirect_command_bytes_per_frame * _frame_in_flight;

        for (auto [key, batch] : _cpu_buffers.draw_batches)
        {
            if (key.alpha_type == alpha_behavior::OPAQUE || key.alpha_type == alpha_behavior::MASK)
            {
                queue.draw(commands, _gpu_buffers.indirect_commands, static_cast<uint32_t>(indirect_command_offset),
                           static_cast<uint32_t>(batch.objects.size()), sizeof(gpu::indexed_indirect_command));
            }
        }

        queue.end_rendering(commands);
    }

    void pbr_pipeline::_draw_shadow_pass([[maybe_unused]] renderer& parent, rhi::device& dev,
                                         [[maybe_unused]] const render_state& rs,
                                         rhi::work_queue& queue,
                                         rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands)
    {
        if (_shadows.last_binding_update_frame >= _frame_number ||
            _bindless_textures.last_updated_frame_index >= _frame_number)
        {
            auto ds_desc = rhi::descriptor_set_desc{};

            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 1,
                .type = rhi::descriptor_type::structured_buffer,
                .offset = 0,
                .size = static_cast<uint32_t>(_gpu_resource_usages.vertex_bytes_written),
                .buffer = _gpu_buffers.vertices,
            });

            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 2,
                .type = rhi::descriptor_type::structured_buffer,
                .offset = 0,
                .size = static_cast<uint32_t>(_meshes.meshes.size() * sizeof(mesh_layout)),
                .buffer = _gpu_buffers.mesh_layouts,
            });

            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 3,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .offset = 0,
                .size =
                    static_cast<uint32_t>(math::round_to_next_multiple(_object_count * sizeof(gpu::object_data), 256)),
                .buffer = _gpu_buffers.objects,
            });

            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 4,
                .type = rhi::descriptor_type::dynamic_structured_buffer,
                .offset = 0,
                .size = static_cast<uint32_t>(math::round_to_next_multiple(_object_count * sizeof(uint32_t), 256)),
                .buffer = _gpu_buffers.instances,
            });

            ds_desc.buffers.push_back(rhi::buffer_binding_descriptor{
                .index = 5,
                .type = rhi::descriptor_type::structured_buffer,
                .offset = 0,
                .size = static_cast<uint32_t>(_materials.materials.size() * sizeof(gpu::material_data)),
                .buffer = _gpu_buffers.materials,
            });

            vector<rhi::image_binding_info> images;
            for (const auto& img : _bindless_textures.images)
            {
                images.push_back(rhi::image_binding_info{
                    .image = img,
                    .layout = rhi::image_layout::shader_read_only,
                });
            }

            ds_desc.images.push_back(rhi::image_binding_descriptor{
                .index = 16,
                .type = rhi::descriptor_type::sampled_image,
                .array_offset = 0,
                .images = tempest::move(images),
            });

            vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>> samplers;
            samplers.push_back(_bindless_textures.linear_sampler_no_aniso);
            ds_desc.samplers.push_back({
                .index = 15,
                .samplers = tempest::move(samplers),
            });

            ds_desc.layout = _shadows.directional_desc_set_0_layout;

            dev.destroy_descriptor_set(_shadows.directional_desc_set_0);
            _shadows.directional_desc_set_0 = dev.create_descriptor_set(ds_desc);
        }

        queue.bind(commands, _shadows.directional_pipeline);

        const auto instance_offset = static_cast<uint32_t>(_gpu_buffers.instance_bytes_per_frame * _frame_in_flight);
        const auto object_offset = static_cast<uint32_t>(_gpu_buffers.object_bytes_per_frame * _frame_in_flight);

        const array dynamic_offsets = {
            object_offset,
            instance_offset,
        };

        queue.bind(commands, _shadows.directional_layout, rhi::bind_point::graphics, 0,
                   {&_shadows.directional_desc_set_0, 1}, dynamic_offsets);

        auto shadow_maps_written = 0u;
        _entity_registry->each(
            [&]([[maybe_unused]] const directional_light_component& light, shadow_map_component shadows) {
                for (auto i = 0u; i < shadows.cascade_count; ++i)
                {
                    const auto& params = _shadows.shadow_map_build_params[shadow_maps_written];
                    const auto& region = params.shadow_map_region;

                    queue.set_scissor_region(commands, region.x, region.y, region.z, region.w);
                    queue.set_viewport(commands, static_cast<float>(region.x), static_cast<float>(region.y),
                                       static_cast<float>(region.z), static_cast<float>(region.w), 0.0f, 1.0f, false);
                    queue.set_cull_mode(commands, enum_mask(rhi::cull_mode::back));

                    // Set up push constants
                    // Draw
                }
            });
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