#ifndef tempest_graphics_pbr_frame_graph_hpp
#define tempest_graphics_pbr_frame_graph_hpp

#include <tempest/archetype.hpp>
#include <tempest/frame_graph.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/vec4.hpp>

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

        optional<graph_builder&> get_builder() noexcept;

        void compile(queue_configuration cfg);

        void execute();

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
        };

        struct depth_prepass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> depth;
            graph_resource_handle<rhi::rhi_handle_type::image> encoded_normals;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        };

        struct ssao_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> ssao_output;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> ssao_noise_image;
        };

        struct ssao_blur_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> ssao_blurred_output;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        };

        struct shadow_map_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> shadow_map_megatexture;
            graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_data;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> directional_shadow_pipeline;
        };

        struct light_clustering_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_cluster_bounds;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline;
        };

        struct light_culling_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_grid;
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_grid_ranges;
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_indices;
            graph_resource_handle<rhi::rhi_handle_type::buffer> light_index_count;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline;
        };

        struct pbr_opaque_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> hdr_color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        };

        struct mboit_gather_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> transparency_accumulation;
            graph_resource_handle<rhi::rhi_handle_type::image> moments_buffer;
            graph_resource_handle<rhi::rhi_handle_type::image> zeroth_moment_buffer;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        };

        struct mboit_resolve_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> transparency_accumulation;
            graph_resource_handle<rhi::rhi_handle_type::image> moments_buffer;
            graph_resource_handle<rhi::rhi_handle_type::image> zeroth_moment_buffer;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        };

        struct mboit_blend_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> hdr_color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        };

        struct tonemapping_pass_outputs
        {
            graph_resource_handle<rhi::rhi_handle_type::image> tonemapped_color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        };

        struct
        {
            frame_upload_pass_outputs upload_pass;
            depth_prepass_outputs depth_prepass;
            ssao_pass_outputs ssao;
            ssao_blur_pass_outputs ssao_blur;
            light_clustering_pass_outputs light_clustering;
            light_culling_pass_outputs light_culling;
            shadow_map_pass_outputs shadow_map;
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

            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> vertex_pull_buffer;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> mesh_buffer;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> material_buffer;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_sampler;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_with_aniso_sampler;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_sampler;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_with_aniso_sampler;

            vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::image>> bindless_textures;
        } _global_resources;

        void _create_global_resources();
        void _release_global_resources();

        frame_upload_pass_outputs _add_frame_upload_pass(graph_builder& builder);
        void _release_frame_upload_pass(frame_upload_pass_outputs& outputs);

        depth_prepass_outputs _add_depth_prepass(graph_builder& builder);
        void _release_depth_prepass(depth_prepass_outputs& outputs);

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
        static void _ssao_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                    graph_resource_handle<rhi::rhi_handle_type::buffer> descriptors);
        static void _ssao_blur_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self);
        static void _light_clustering_pass_task(compute_task_execution_context& ctx, pbr_frame_graph* self);
        static void _light_culling_pass_task(compute_task_execution_context& ctx, pbr_frame_graph* self);
        static void _shadow_map_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                          graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors);
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

        struct alignas(16) light
        {
            math::vec4<float> color_intensity;
            math::vec4<float> position_falloff;
            math::vec4<float> direction_angle;
            array<uint32_t, 6> shadow_map_indices;
            light_type type;
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

        struct alignas(16) scene_constants
        {
            static constexpr size_t ssao_kernel_size = 64;

            camera cam;
            math::vec2<float> screen_size;
            math::vec3<float> ambient_light_color;
            light sun;
            math::vec4<uint32_t> light_grid_count_and_size; // x = light grid count, y = light grid size (in tiles), z =
                                                            // padding, w = pixel width
            math::vec2<float> light_grid_z_bounds;          // x = min light grid bounds, y = max light grid bounds (z)
            float ssao_strength = 2.0f;
            uint32_t point_light_count{};
            array<math::vec4<float>, ssao_kernel_size> ssao_sample_kernel;
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

        struct directional_shadow_pass_constants
        {
            math::mat4<float> light_vp;
        };
    };
} // namespace tempest::graphics

#endif // tempest_graphics_pbr_frame_graph_hpp
