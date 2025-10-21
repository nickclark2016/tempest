#ifndef tempest_graphics_pbr_pipeline_hpp
#define tempest_graphics_pbr_pipeline_hpp

#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/flat_map.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/guid.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/int.hpp>
#include <tempest/mat4.hpp>
#include <tempest/material.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/rhi.hpp>
#include <tempest/shelf_pack.hpp>
#include <tempest/sparse.hpp>
#include <tempest/texture.hpp>
#include <tempest/vec4.hpp>
#include <tempest/vector.hpp>
#include <tempest/vertex.hpp>

namespace tempest::graphics
{
    namespace gpu
    {
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

        struct camera
        {
            math::mat4<float> proj;
            math::mat4<float> inv_proj;
            math::mat4<float> view;
            math::mat4<float> inv_view;
            math::vec3<float> position;
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
            light_type type;
            uint32_t shadow_map_count;
            uint32_t enabled;
        };

        struct shadow_map_parameter
        {
            math::mat4<float> light_proj_matrix;
            math::vec4<float> shadow_map_region; // x, y, w, h (normalized)
            float cascade_split_far;
        };

        struct scene_constants
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
        
        struct hi_z
        {
            math::vec2<uint32_t> size;
            uint32_t mip_count;
        };

        struct lighting_cluster_bounds
        {
            math::vec4<float> min_bounds;
            math::vec4<float> max_bounds;
        };

        struct light_grid_range
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

        struct object_data
        {
            math::mat4<float> model;
            math::mat4<float> inv_tranpose_model;

            uint32_t mesh_id;
            uint32_t material_id;
            uint32_t parent_id;
            uint32_t self_id;
        };

        struct shadow_map_cascade_info
        {
            static constexpr size_t max_cascades = 6;

            inplace_vector<math::mat4<float>, max_cascades> frustum_view_projections;
            inplace_vector<float, max_cascades> cascade_distances;
        };
    } // namespace gpu

    class pbr_pipeline : public render_pipeline
    {
      public:
        pbr_pipeline(uint32_t width, uint32_t height, ecs::archetype_registry& entity_registry);
        pbr_pipeline(const pbr_pipeline&) = delete;
        pbr_pipeline(pbr_pipeline&&) = delete;
        ~pbr_pipeline() override = default;
        pbr_pipeline& operator=(const pbr_pipeline&) = delete;
        pbr_pipeline& operator=(pbr_pipeline&&) = delete;

        void initialize(renderer& parent, rhi::device& dev) override;
        render_result render(renderer& parent, rhi::device& dev, const render_state& rs) override;
        void destroy(renderer& parent, rhi::device& dev) override;

        void set_viewport(uint32_t width, uint32_t height) override;

        void upload_objects_sync(rhi::device& dev, span<const ecs::archetype_entity> entities,
                                 const core::mesh_registry& meshes, const core::texture_registry& textures,
                                 const core::material_registry& materials) override;

        void set_skybox_texture(rhi::device& dev, const guid& texture_id,
                                const core::texture_registry& texture_registry);

        render_target_info get_render_target() const override
        {
            return {
                .image = _render_targets.final_color,
                .layout = _final_color_layout,
            };
        }

      private:
        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> desc_set_0_layout;
            uint64_t last_binding_update_frame = 0;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        } _z_prepass = {};

        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> build_cluster_desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> build_cluster_desc_set_0_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> build_cluster_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> build_clusters;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> fill_cluster_desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> fill_cluster_desc_set_0_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> fill_cluster_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> fill_clusters;

            uint64_t last_binding_update_frame = 0;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> light_cluster_buffer =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> light_cluster_range_buffer =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> global_light_index_count_buffer =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> global_light_index_list_buffer =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>::null_handle;

            size_t light_cluster_buffer_size = 0;
            size_t light_cluster_range_buffer_size = 0;
            size_t global_light_index_count_buffer_size = 0;
            size_t global_light_index_list_buffer_size = 0;
        } _forward_light_clustering = {};

        inline static constexpr auto num_clusters_x = 16u;
        inline static constexpr auto num_clusters_y = 9u;
        inline static constexpr auto num_clusters_z = 24u;
        inline static constexpr auto max_lights_per_cluster = 128u;

        struct
        {
            uint64_t last_binding_update_frame = 0;

            vector<math::vec4<float>> noise_kernel;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> scene_constants;
            size_t scene_constant_bytes_per_frame = 0;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> noise_texture =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::image>::null_handle;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> ssao_target;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> ssao_blur_target;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> ssao_desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> ssao_desc_set_0_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> ssao_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> ssao_pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> ssao_blur_desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> ssao_blur_desc_set_0_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> ssao_blur_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> ssao_blur_pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> clamped_linear_no_aniso_sampler =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> clamped_point_no_aniso_sampler =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle;
        } _ssao = {};

        struct
        {
            shelf_pack_allocator image_region_allocator{{1024 * 16, 1024 * 16},
                                                        {
                                                            .alignment = {32, 32},
                                                            .column_count = 4,
                                                        }};

            vector<gpu::shadow_map_parameter> shadow_map_use_params;

            uint64_t last_binding_update_frame = 0;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> directional_desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> directional_desc_set_0_layout =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> directional_layout =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> directional_pipeline =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline>::null_handle;
        } _shadows = {};

        struct
        {
            uint64_t last_binding_update_frame = 0;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> camera_payload;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> desc_set_0_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> hdri_texture =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::image>::null_handle;

            size_t camera_bytes_per_frame = 0;
        } _skybox = {};

        struct
        {
            uint64_t last_binding_update_frame = 0;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> desc_set_0_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> desc_set_1 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> desc_set_1_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        } _pbr_opaque = {};

        struct
        {
            uint64_t last_binding_update_frame = 0;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> oit_gather_desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> oit_gather_desc_set_0_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> oit_gather_desc_set_1 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> oit_gather_desc_set_1_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> oit_gather_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> oit_gather_pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> oit_resolve_desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> oit_resolve_desc_set_0_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> oit_resolve_desc_set_1 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> oit_resolve_desc_set_1_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> oit_resolve_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> oit_resolve_pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> oit_blend_desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> oit_blend_desc_set_0_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> oit_blend_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> oit_blend_pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> moments_target;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> zeroth_moment_target;
        } _pbr_transparencies = {};

        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> desc_set_0 =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> desc_set_0_layout;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        } _tonemapping = {};

        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> depth;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> hdr_color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> final_color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> encoded_normals;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> transparency_accumulator;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> shadow_megatexture;

            size_t frame_built;
        } _render_targets = {};

        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> staging;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> vertices;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> mesh_layouts;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> objects;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> materials;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> instances;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> scene_constants;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> indirect_commands;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> point_and_spot_lights;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> shadows;

            size_t object_bytes_per_frame = 0;
            size_t instance_bytes_per_frame = 0;
            size_t scene_constants_bytes_per_frame = 0;
            size_t lights_bytes_per_frame = 0;
            size_t shadow_bytes_per_frame = 0;
        } _gpu_buffers = {};

        struct
        {
            uint32_t staging_bytes_writen = 0;
            uint32_t staging_bytes_available = 0;
            uint32_t vertex_bytes_written = 0;
            uint32_t mesh_layout_bytes_written = 0;
        } _gpu_resource_usages = {};

        struct draw_batch_key
        {
            alpha_behavior alpha_type;
            bool double_sided;

            constexpr auto operator<=>(const draw_batch_key&) const noexcept = default;
        };

        struct draw_batch_payload
        {
            vector<gpu::indexed_indirect_command> commands;
            size_t indirect_command_offset;
            ecs::basic_sparse_map<ecs::basic_archetype_registry::entity_type, gpu::object_data> objects;
        };

        [[maybe_unused]] struct
        {
            uint32_t indirect_command_bytes_per_frame = 0;
            flat_map<draw_batch_key, draw_batch_payload> draw_batches;
            vector<mesh_layout> meshes;
            ecs::basic_sparse_map<ecs::basic_archetype_registry::entity_type, gpu::light> point_and_spot_lights;
            ecs::basic_sparse_map<ecs::basic_archetype_registry::entity_type, gpu::light> dir_lights;
        } _cpu_buffers = {};

        [[maybe_unused]] struct
        {
            float radius = 0.5f;
            float bias = 0.025f;
        } _ssao_constants;

        struct
        {
            uint64_t last_updated_frame_index;

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
            vector<gpu::material_data> materials;
        } _materials = {};

        struct
        {
            flat_unordered_map<guid, size_t> mesh_to_index;
            vector<mesh_layout> meshes;
        } _meshes = {};

        static constexpr rhi::image_format depth_format = rhi::image_format::d32_float;
        static constexpr rhi::image_format hdr_color_format = rhi::image_format::rgba16_float;
        static constexpr rhi::image_format final_color_format = rhi::image_format::rgba8_srgb;
        static constexpr rhi::image_format encoded_normals_format = rhi::image_format::rg16_float;
        static constexpr rhi::image_format transparency_accumulator_format = rhi::image_format::rgba16_float;
        static constexpr rhi::image_format ssao_format = rhi::image_format::r16_float;
        static constexpr rhi::image_format shadow_megatexture_format = rhi::image_format::d24_unorm;

        uint32_t _render_target_width;
        uint32_t _render_target_height;
        bool _render_target_requires_reconstruction = true;
        rhi::image_layout _final_color_layout;

        uint32_t _object_count = 0;

        gpu::scene_constants _scene{};
        [[maybe_unused]] ecs::archetype_entity _camera{};

        size_t _frame_number = 0;
        size_t _frame_in_flight = 0;
        ecs::archetype_registry* _entity_registry = nullptr;

        void _initialize_z_prepass(renderer& parent, rhi::device& dev);
        void _initialize_clustering(renderer& parent, rhi::device& dev);
        void _initialize_pbr_opaque(renderer& parent, rhi::device& dev);
        void _initialize_pbr_mboit(renderer& parent, rhi::device& dev);
        void _initialize_shadows(renderer& parent, rhi::device& dev);
        void _initialize_ssao(renderer& parent, rhi::device& dev);
        void _initialize_skybox(renderer& parent, rhi::device& dev);
        void _initialize_tonemap(renderer& parent, rhi::device& dev);
        void _initialize_samplers(renderer& parent, rhi::device& dev);

        void _initialize_render_targets(rhi::device& dev);
        void _initialize_gpu_buffers(rhi::device& dev);

        void _upload_per_frame_data(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                                    rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands,
                                    const gpu::camera& camera);
        void _prepare_draw_batches(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                                   rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands);
        void _draw_z_prepass(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                             rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands);
        void _draw_shadow_pass(
            renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
            rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands,
            const flat_unordered_map<ecs::archetype_entity, gpu::shadow_map_cascade_info> light_map_cascades);
        void _draw_light_clusters(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                                  rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands,
                                  const math::mat4<float>& inv_proj);
        void _draw_ssao_pass(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                             rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands,
                             const gpu::camera& cam);
        void _draw_skybox_pass(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                               rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands,
                               const gpu::camera& camera);
        void _draw_pbr_opaque_pass(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                                   rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands);
        void _draw_pbr_mboit_pass(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                                  rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands);
        void _draw_tonemap_pass(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                                rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands);

        void _load_meshes(rhi::device& dev, span<const guid> mesh_ids, const core::mesh_registry& mesh_registry);
        void _load_textures(rhi::device& dev, span<const guid> texture_ids,
                            const core::texture_registry& texture_registry, bool generate_mip_maps);
        void _load_materials(rhi::device& dev, span<const guid> material_ids,
                             const core::material_registry& material_registry);

        uint32_t _acquire_next_object() noexcept;

        optional<gpu::light> _get_light_data(ecs::archetype_entity entity) const;

        void _reconstruct_render_targets(rhi::device& dev);
        void _construct_pbr_mboit_images(rhi::device& dev);
        void _construct_ssao_images(rhi::device& dev);
    };
} // namespace tempest::graphics

#endif // tempest_graphics_pbr_pipeline_hpp
