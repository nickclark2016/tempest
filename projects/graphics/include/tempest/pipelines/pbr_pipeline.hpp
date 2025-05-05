#ifndef tempest_graphics_pbr_pipeline_hpp
#define tempest_graphics_pbr_pipeline_hpp

#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/flat_map.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/mat4.hpp>
#include <tempest/material.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/rhi.hpp>
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
            static constexpr int16_t INVALID_TEXTURE_ID = -1;

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

            int16_t base_color_texture_id = INVALID_TEXTURE_ID;
            int16_t normal_texture_id = INVALID_TEXTURE_ID;
            int16_t metallic_roughness_texture_id = INVALID_TEXTURE_ID;
            int16_t emissive_texture_id = INVALID_TEXTURE_ID;
            int16_t occlusion_texture_id = INVALID_TEXTURE_ID;
            int16_t transmission_texture_id = INVALID_TEXTURE_ID;
            int16_t thickness_texture_id = INVALID_TEXTURE_ID;

            material_type type;
        };

        struct camera
        {
            math::mat4<float> proj;
            math::mat4<float> inv_proj;
            math::mat4<float> prev_proj;
            math::mat4<float> view;
            math::mat4<float> inv_view;
            math::mat4<float> prev_view;
            math::vec3<float> position;
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

        struct alignas(16) shadow_map_parameter
        {
            math::mat4<float> light_proj_matrix;
            math::vec4<float> shadow_map_region; // x, y, w, h (normalized)
            float cascade_split_far;
        };

        struct alignas(16) scene_data
        {
            camera cam;
            math::vec4<float> screen_size;
            math::vec4<float> ambient_light_color_intensity;
            light sun;
            math::vec4<float> light_grid_count_and_size; // x = light grid count, y = light grid size (in tiles), z =
                                                         // padding, w = pixel width
            math::vec2<float> light_grid_z_bounds;       // x = min light grid bounds, y = max light grid bounds (z)
            float ssao_strength = 2.0f;
            uint32_t point_light_count{};
        };

        struct hi_z
        {
            math::vec2<uint32_t> size;
            uint32_t mip_count;
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
            math::mat4<float> prev_model;

            uint32_t mesh_id;
            uint32_t material_id;
            uint32_t parent_id;
            uint32_t self_id;
        };
    } // namespace gpu

    class pbr_pipeline : public render_pipeline
    {
      public:
        pbr_pipeline(uint32_t width, uint32_t height);
        pbr_pipeline(const pbr_pipeline&) = delete;
        pbr_pipeline(pbr_pipeline&&) = delete;
        ~pbr_pipeline() override = default;
        pbr_pipeline& operator=(const pbr_pipeline&) = delete;
        pbr_pipeline& operator=(pbr_pipeline&&) = delete;

        void initialize(renderer& parent, rhi::device& dev) override;
        render_result render(renderer& parent, rhi::device& dev, const render_state& rs) const override;
        void destroy(renderer& parent, rhi::device& dev) override;

        flat_unordered_map<guid, mesh_layout> load_meshes(rhi::device& dev, span<const guid> mesh_ids,
                                                          const core::mesh_registry& mesh_registry);
        void load_textures(rhi::device& dev, span<const guid> texture_ids,
                           const core::texture_registry& texture_registry, bool generate_mip_maps);
        void load_materials(rhi::device& dev, span<const guid> material_ids,
                            const core::material_registry& material_registry);

      private:
        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> desc_set_0_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        } _z_prepass = {};

        [[maybe_unused]] struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> build_cluster_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> build_clusters;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> fill_cluster_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> fill_clusters;
        } _forward_light_clustering = {};

        [[maybe_unused]] struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        } _skybox_pass = {};

        [[maybe_unused]] struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
        } _pbr_opaque = {};

        [[maybe_unused]] struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> oit_gather_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> oit_gather_pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> oit_resolve_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> oit_resolve_pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> oit_blend_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> oit_blend_pipeline;
        } _pbr_transparencies = {};

        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> depth;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> encoded_normals;
        } _render_targets = {};

        [[maybe_unused]] struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> staging;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> vertices;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> mesh_layouts;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> objects;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> materials;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> instances;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> scene_constants;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> indirect_commands;
        } _gpu_buffers = {};

        struct
        {
            uint32_t staging_bytes_writen;
            uint32_t staging_bytes_available;
            uint32_t vertex_bytes_written;
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
            size_t index_buffer_offset;
            ecs::basic_sparse_map<ecs::basic_archetype_registry::entity_type, gpu::object_data> objects;
        };

        [[maybe_unused]] struct
        {
            flat_map<draw_batch_key, draw_batch_payload> draw_batches;
            vector<mesh_layout> meshes;
        } _cpu_buffers = {};

        [[maybe_unused]] struct
        {
            float radius = 0.5f;
            float bias = 0.025f;
        } _ssao_constants;

        [[maybe_unused]] struct
        {
            vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::image>> images;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_sampler;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_sampler;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_sampler_no_aniso;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_sampler_no_aniso;
        } _bindless_textures = {};

        static constexpr rhi::image_format depth_format = rhi::image_format::d32_float;
        static constexpr rhi::image_format color_format = rhi::image_format::rgba8_srgb;
        static constexpr rhi::image_format encoded_normals_format = rhi::image_format::rg16_float;
        static constexpr rhi::image_format positions_format = rhi::image_format::rgba16_float;

        uint32_t _render_target_width;
        uint32_t _render_target_height;

        [[maybe_unused]] gpu::scene_data _scene{};
        [[maybe_unused]] ecs::archetype_entity _camera{};

        void _initialize_z_prepass(renderer& parent, rhi::device& dev);
        void _initialize_render_targets(renderer& parent, rhi::device& dev);
        void _initialize_gpu_buffers(renderer& parent, rhi::device& dev);

        void _draw_z_prepass(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                             rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands) const;
        void _draw_clear_pass(renderer& parent, rhi::device& dev, const render_state& rs, rhi::work_queue& queue,
                              rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands) const;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_pbr_pipeline_hpp
