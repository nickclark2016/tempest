#ifndef tempest_graphics_pbr_frame_graph_hpp
#define tempest_graphics_pbr_frame_graph_hpp

#include <tempest/archetype.hpp>
#include <tempest/flat_map.hpp>
#include <tempest/frame_graph.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/material.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/shelf_pack.hpp>
#include <tempest/texture.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vec4.hpp>
#include <tempest/vertex.hpp>

namespace tempest::graphics
{
    struct pbr_frame_graph_config
    {
        uint32_t render_target_width;
        uint32_t render_target_height;

        rhi::image_format hdr_color_format;
        rhi::image_format depth_format;
        rhi::image_format tonemapped_color_format;

        uint32_t vertex_data_buffer_size;
        uint32_t max_mesh_count;
        uint32_t max_material_count;
        uint32_t staging_buffer_size_per_frame;
        uint32_t max_object_count;
        uint32_t max_lights;
        uint32_t max_bindless_textures;

        float max_anisotropy;

        struct
        {
            uint32_t cluster_count_x;
            uint32_t cluster_count_y;
            uint32_t cluster_count_z;
            uint32_t max_lights_per_cluster;
        } light_clustering;

        struct
        {
            uint32_t shadow_map_width;
            uint32_t shadow_map_height;
            uint32_t max_shadow_casting_lights;
        } shadows;
    };

    struct pbr_frame_graph_inputs
    {
        ecs::archetype_registry* entity_registry = nullptr;
    };

    class pbr_frame_graph
    {
      public:
        pbr_frame_graph(rhi::device& device, pbr_frame_graph_config cfg, pbr_frame_graph_inputs inputs);

        pbr_frame_graph(const pbr_frame_graph&) = delete;
        pbr_frame_graph(pbr_frame_graph&&) noexcept = delete;

        ~pbr_frame_graph();

        pbr_frame_graph& operator=(const pbr_frame_graph&) = delete;
        pbr_frame_graph& operator=(pbr_frame_graph&&) noexcept = delete;

        optional<graph_builder&> get_builder() & noexcept;

        graph_resource_handle<rhi::rhi_handle_type::image> get_tonemapped_color_handle() const noexcept
        {
            return _pass_output_resource_handles.tonemapping.tonemapped_color;
        }

        rhi::image_format get_tonemapped_color_format() const noexcept
        {
            return _cfg.tonemapped_color_format;
        }

        void compile(queue_configuration cfg);

        void execute();

        void upload_objects_sync(span<const ecs::archetype_entity> entities, const core::mesh_registry& meshes,
                                 const core::texture_registry& textures, const core::material_registry& materials);

      private:
        rhi::device* _device;
        pbr_frame_graph_config _cfg;
        pbr_frame_graph_inputs _inputs;

        optional<graph_builder> _builder;
        optional<graph_executor> _executor;

        void _initialize();

        struct frame_upload_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::buffer> scene_constants;
            graph_resource_handle<rhi::rhi_handle_type::buffer> draw_commands;
        };

        struct depth_prepass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> depth;
            graph_resource_handle<rhi::rhi_handle_type::image> encoded_normals;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> scene_descriptor_layout =
                rhi::null_handle;
        };

        struct hierarchical_z_buffer_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> hzb;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
        };

        struct ssao_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> ssao_output;
            graph_resource_handle<rhi::rhi_handle_type::buffer> ssao_constants_buffer;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> ssao_noise_image = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> descriptor_layout = rhi::null_handle;
        };

        struct ssao_blur_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> ssao_blurred_output;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
        };

        struct shadow_map_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> shadow_map_megatexture;
            graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_data;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> directional_shadow_pipeline =
                rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> directional_shadow_pipeline_layout =
                rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> scene_descriptor_layout =
                rhi::null_handle;
        };

        struct light_clustering_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_cluster_bounds;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> descriptor_layout = rhi::null_handle;
        };

        struct light_culling_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_grid;
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_grid_ranges;
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_indices;
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_index_count;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> descriptor_layout = rhi::null_handle;
        };

        struct pbr_opaque_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> hdr_color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> scene_descriptor_layout =
                rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> shadow_and_lighting_descriptor_layout =
                rhi::null_handle;
        };

        struct mboit_gather_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> transparency_accumulation;
            graph_resource_handle<rhi::rhi_handle_type::image> moments_buffer;
            graph_resource_handle<rhi::rhi_handle_type::image> zeroth_moment_buffer;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> scene_descriptor_layout =
                rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> shadow_and_lighting_descriptor_layout =
                rhi::null_handle;
        };

        struct mboit_resolve_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> transparency_accumulation;
            graph_resource_handle<rhi::rhi_handle_type::image> moments_buffer;
            graph_resource_handle<rhi::rhi_handle_type::image> zeroth_moment_buffer;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> scene_descriptor_layout =
                rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> shadow_and_lighting_descriptor_layout =
                rhi::null_handle;
        };

        struct mboit_blend_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> hdr_color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
        };

        struct tonemapping_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> tonemapped_color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
        };

        struct skybox_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> hdr_color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout = rhi::null_handle;
        };

        struct
        {
            frame_upload_pass_outputs upload_pass;
            depth_prepass_outputs depth_prepass;
            hierarchical_z_buffer_pass_outputs hierarchical_z_buffer;
            ssao_pass_outputs ssao;
            ssao_blur_pass_outputs ssao_blur;
            light_clustering_pass_outputs light_clustering;
            light_culling_pass_outputs light_culling;
            shadow_map_pass_outputs shadow_map;
            skybox_pass_outputs skybox;
            pbr_opaque_pass_outputs pbr_opaque;
            mboit_gather_pass_outputs mboit_gather;
            mboit_resolve_pass_outputs mboit_resolve;
            mboit_blend_pass_outputs mboit_blend;
            tonemapping_pass_outputs tonemapping;
        } _pass_output_resource_handles;

        struct
        {
            graph_resource_handle<rhi::rhi_handle_type::buffer> graph_vertex_pull_buffer;
            graph_resource_handle<rhi::rhi_handle_type::buffer> graph_mesh_buffer;
            graph_resource_handle<rhi::rhi_handle_type::buffer> graph_material_buffer;
            graph_resource_handle<rhi::rhi_handle_type::buffer> graph_instance_buffer;
            graph_resource_handle<rhi::rhi_handle_type::buffer> graph_object_buffer;
            graph_resource_handle<rhi::rhi_handle_type::buffer> graph_light_buffer;
            graph_resource_handle<rhi::rhi_handle_type::buffer> graph_per_frame_staging_buffer;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> vertex_pull_buffer = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> mesh_buffer = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> material_buffer = rhi::null_handle;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_sampler = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_with_aniso_sampler = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_sampler = rhi::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_with_aniso_sampler = rhi::null_handle;

            struct
            {
                uint64_t vertex_bytes_written = 0;
                uint64_t mesh_layout_bytes_written = 0;
                uint64_t material_bytes_written = 0;
                uint32_t loaded_object_count = 0;
                size_t staging_buffer_bytes_written = 0;
            } utilization;
        } _global_resources;

        void _create_global_resources();
        void _release_global_resources();

        frame_upload_pass_outputs _add_frame_upload_pass(graph_builder& builder);
        void _release_frame_upload_pass(frame_upload_pass_outputs& outputs);

        depth_prepass_outputs _add_depth_prepass(graph_builder& builder);
        void _release_depth_prepass(depth_prepass_outputs& outputs);

        hierarchical_z_buffer_pass_outputs _add_hierarchical_z_buffer_pass(graph_builder& builder);
        void _release_hierarchical_z_buffer_pass(hierarchical_z_buffer_pass_outputs& outputs);

        ssao_pass_outputs _add_ssao_pass(graph_builder& builder);
        void _release_ssao_pass(ssao_pass_outputs& outputs);

        ssao_blur_pass_outputs _add_ssao_blur_pass(graph_builder& builder);
        void _release_ssao_blur_pass(ssao_blur_pass_outputs& outputs);

        light_clustering_pass_outputs _add_light_clustering_pass(graph_builder& builder);
        void _release_light_clustering_pass(light_clustering_pass_outputs& outputs);

        light_culling_pass_outputs _add_light_culling_pass(graph_builder& builder);
        void _release_light_culling_pass(light_culling_pass_outputs& outputs);

        shadow_map_pass_outputs _add_shadow_map_pass(graph_builder& builder);
        void _release_shadow_map_pass(shadow_map_pass_outputs& outputs);

        skybox_pass_outputs _add_skybox_pass(graph_builder& builder);
        void _release_skybox_pass(skybox_pass_outputs& outputs);

        pbr_opaque_pass_outputs _add_pbr_opaque_pass(graph_builder& builder);
        void _release_pbr_opaque_pass(pbr_opaque_pass_outputs& outputs);

        mboit_gather_pass_outputs _add_mboit_gather_pass(graph_builder& builder);
        void _release_mboit_gather_pass(mboit_gather_pass_outputs& outputs);

        mboit_resolve_pass_outputs _add_mboit_resolve_pass(graph_builder& builder);
        void _release_mboit_resolve_pass(mboit_resolve_pass_outputs& outputs);

        mboit_blend_pass_outputs _add_mboit_blend_pass(graph_builder& builder);
        void _release_mboit_blend_pass(mboit_blend_pass_outputs& outputs);

        tonemapping_pass_outputs _add_tonemapping_pass(graph_builder& builder);
        void _release_tonemapping_pass(tonemapping_pass_outputs& outputs);

        static void _upload_pass_task(transfer_task_execution_context& ctx, pbr_frame_graph* self);
        static void _depth_prepass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                        graph_resource_handle<rhi::rhi_handle_type::buffer> descriptors);
        static void _hierarchical_z_buffer_pass_task(compute_task_execution_context& ctx, pbr_frame_graph* self);
        static void _ssao_upload_task(transfer_task_execution_context& ctx, pbr_frame_graph* self);
        static void _ssao_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                    graph_resource_handle<rhi::rhi_handle_type::buffer> descriptors);
        static void _ssao_blur_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self);
        static void _light_clustering_pass_task(compute_task_execution_context& ctx, pbr_frame_graph* self);
        static void _light_culling_pass_task(compute_task_execution_context& ctx, pbr_frame_graph* self);
        static void _shadow_upload_pass_task(transfer_task_execution_context& ctx, pbr_frame_graph* self);
        static void _shadow_map_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                          graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors);
        static void _skybox_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self);
        static void _pbr_opaque_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                          graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors,
                                          graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_descriptors);
        static void _mboit_gather_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                            graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors,
                                            graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_descriptors);
        static void _mboit_resolve_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                             graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors,
                                             graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_descriptors);
        static void _mboit_blend_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self);
        static void _tonemapping_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self);

        void _load_meshes(span<const guid> mesh_ids, const core::mesh_registry& mesh_registry);
        void _load_textures(span<const guid> texture_ids, const core::texture_registry& texture_registry,
                            bool generate_mip_maps);
        void _load_materials(span<const guid> material_ids, const core::material_registry& material_registry);

        enum class material_type : uint32_t
        {
            opaque = 0,
            mask = 1,
            blend = 2,
            transmissive = 3,
        };

        struct material_data
        {
            static constexpr int16_t invalid_texture_id = -1;

            math::vec4<float> base_color_factor;
            math::vec4<float> emissive_factor;
            math::vec4<float> attenuation_color;

            float normal_scale;
            float metallic_factor;
            float roughness_factor;
            float alpha_cutoff;
            float reflectance;
            float transmission_factor;
            float thickness_factor;
            float attenuation_distance;

            int16_t base_color_texture_id = invalid_texture_id;
            int16_t normal_texture_id = invalid_texture_id;
            int16_t metallic_roughness_texture_id = invalid_texture_id;
            int16_t emissive_texture_id = invalid_texture_id;
            int16_t occlusion_texture_id = invalid_texture_id;
            int16_t transmission_texture_id = invalid_texture_id;
            int16_t thickness_texture_id = invalid_texture_id;

            material_type type;
        };

        struct mesh_layout
        {
            uint32_t mesh_start_offset;
            uint32_t positions_offset;
            uint32_t interleave_offset;
            uint32_t interleave_stride;
            uint32_t uvs_offset;
            uint32_t normals_offset;
            uint32_t tangents_offset = numeric_limits<uint32_t>::max();
            uint32_t color_offset = numeric_limits<uint32_t>::max();
            uint32_t index_offset;
            uint32_t index_count;
        };

        struct object_data
        {
            math::mat4<float> model;
            math::mat4<float> inv_tranpose_model;

            uint32_t mesh_id;
            uint32_t material_id;
            uint32_t parent_id;
            uint32_t self_id;
        };

        enum class light_type : uint32_t
        {
            directional = 0,
            point = 1,
        };

        struct light
        {
            math::vec4<float> color_intensity;
            math::vec4<float> position_falloff;
            math::vec4<float> direction_angle;
            array<uint32_t, 6> shadow_map_indices;
            alignas(16) light_type type;
            uint32_t shadow_map_count;
            uint32_t enabled;
        };

        struct camera
        {
            math::mat4<float> proj;
            math::mat4<float> inv_proj;
            math::mat4<float> view;
            math::mat4<float> inv_view;
            math::vec3<float> position;
        };

        struct scene_constants
        {
            camera cam;
            alignas(16) math::vec2<float> screen_size;
            alignas(16) math::vec3<float> ambient_light_color;
            alignas(16) light sun;
            alignas(16) math::vec4<uint32_t> light_grid_count_and_size; // x = light grid count, y = light grid size (in
                                                                        // tiles), z = padding, w = pixel width
            alignas(
                16) math::vec2<float> light_grid_z_bounds; // x = min light grid bounds, y = max light grid bounds (z)
            float ssao_strength = 2.0f;
            uint32_t point_light_count{};
        };

        struct lighting_cluster_bounds
        {
            math::vec4<float> min_bounds;
            math::vec4<float> max_bounds;
        };

        struct alignas(16) light_grid_range
        {
            uint32_t offset;
            uint32_t range;
        };

        struct ssao_constants
        {
            static constexpr size_t ssao_kernel_size = 64;

            array<math::vec4<float>, ssao_kernel_size> ssao_sample_kernel;
            math::vec2<float> noise_scale;
            float radius;
            float bias;
        };

        struct indirect_command
        {
            uint32_t vertex_count;
            uint32_t instance_count;
            uint32_t first_vertex;
            uint32_t first_instance;
        };

        struct indexed_indirect_command
        {
            uint32_t index_count;
            uint32_t instance_count;
            uint32_t first_index;
            int32_t vertex_offset;
            uint32_t first_instance;
        };

        struct cluster_grid_create_info
        {
            math::mat4<float> inv_proj;
            math::vec4<float> screen_bounds;
            math::vec4<uint32_t> workgroup_count_tile_size_px;
        };

        struct light_culling_info
        {
            math::mat4<float> inv_proj;
            math::vec4<float> screen_bounds;
            math::vec4<uint32_t> workgroup_count_tile_size_px;
            uint32_t light_count;
        };

        struct shadow_map_parameter
        {
            math::mat4<float> light_proj_matrix;
            math::vec4<float> shadow_map_region; // x, y, w, h (normalized)
            float cascade_split_far;
        };

        struct shadow_map_cascade_info
        {
            static constexpr size_t max_cascade_count = 6;
            inplace_vector<math::mat4<float>, max_cascade_count> frustum_view_projections;
            inplace_vector<float, max_cascade_count> cascade_distances;
        };

        struct directional_shadow_pass_constants
        {
            math::mat4<float> light_vp;
        };

        struct hi_z_constants
        {
            math::vec2<uint32_t> screen_size;
            uint32_t num_levels;
        };

        struct draw_batch_key
        {
            alpha_behavior alpha_type;
            bool double_sided;

            constexpr auto operator<=>(const draw_batch_key&) const noexcept = default;
        };

        struct draw_batch_payload
        {
            vector<indexed_indirect_command> commands;
            size_t indirect_command_offset;
            ecs::basic_sparse_map<ecs::basic_archetype_registry::entity_type, object_data> objects;
        };

        struct
        {
            flat_unordered_map<guid, size_t> image_to_index;
            vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::image>> images;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_sampler;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_sampler;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_sampler_no_aniso;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_sampler_no_aniso;
        } _bindless_textures = {};

        struct
        {
            flat_unordered_map<guid, size_t> material_to_index;
            vector<material_data> materials;
        } _materials = {};

        struct
        {
            flat_unordered_map<guid, size_t> mesh_to_index;
            vector<mesh_layout> meshes;
        } _meshes = {};

        struct
        {
            flat_map<draw_batch_key, draw_batch_payload> draw_batches;
        } _drawables = {};

        struct
        {
            vector<math::vec4<float>> noise_kernel;
            math::vec2<float> noise_scale;
            float radius;
            float bias;
        } _ssao_data = {};

        struct
        {
            vector<shadow_map_parameter> shadow_map_parameters;
            optional<shelf_pack_allocator> shelf_pack;
            flat_unordered_map<ecs::archetype_entity, shadow_map_cascade_info> light_shadow_data;
        } _shadow_data = {};

        struct
        {
            math::vec3<float> ambient_scene_light;
            camera primary_camera;
            light primary_sun;
            ecs::basic_sparse_map<ecs::archetype_entity, light> point_lights;
            ecs::basic_sparse_map<ecs::archetype_entity, light> dir_lights;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> skybox_texture = rhi::null_handle;
        } _scene_data = {};

        static shadow_map_cascade_info _calculate_shadow_map_cascades(const shadow_map_component& shadows,
                                                                      const ecs::transform_component& light_transform,
                                                                      const camera_component& camera_data,
                                                                      const math::mat4<float>& view_matrix);
    };
} // namespace tempest::graphics

#endif // tempest_graphics_pbr_frame_graph_hpp
