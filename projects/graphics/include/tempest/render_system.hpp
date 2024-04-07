#ifndef tempest_graphcis_render_system_hpp
#define tempest_graphcis_render_system_hpp

#include "render_device.hpp"
#include "render_graph.hpp"
#include "window.hpp"

#include <tempest/memory.hpp>
#include <tempest/registry.hpp>
#include <tempest/vertex.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace tempest::graphics
{
    class render_system
    {
        struct gpu_object_data
        {
            math::mat4<float> model;
            math::mat4<float> inv_tranpose_model;

            std::uint32_t mesh_id;
            std::uint32_t material_id;
            std::uint32_t parent_id;
            std::uint32_t self_id;
        };

        enum class gpu_material_type : std::uint32_t
        {
            PBR_OPAQUE = 0,
            PBR_BLEND = 1,
            PBR_MASK = 2,
        };

        struct gpu_material_data
        {
            static constexpr std::int16_t INVALID_TEXTURE_ID = -1;

            math::vec4<float> base_color_factor;
            math::vec4<float> emissive_factor;

            float normal_scale;
            float metallic_factor;
            float roughness_factor;
            float alpha_cutoff;
            float reflectance;

            std::int16_t base_color_texture_id;
            std::int16_t normal_texture_id;
            std::int16_t metallic_roughness_texture_id;
            std::int16_t emissive_texture_id;
            std::int16_t occlusion_texture_id;

            gpu_material_type material_type;
        };

        struct gpu_camera_data
        {
            math::mat4<float> proj;
            math::mat4<float> inv_proj;
            math::mat4<float> view;
            math::mat4<float> inv_view;
            math::vec3<float> position;
        };

        enum class gpu_light_type : std::uint32_t
        {
            DIRECTIONAL = 0,
            POINT = 1,
            SPOT = 2
        };

        struct gpu_light
        {
            math::vec4<float> color;
            math::vec4<float> position;
            math::vec3<float> direction;
            math::vec3<float> attenuation;
            gpu_light_type light_type;
        };

        struct gpu_scene_data
        {
            gpu_camera_data camera;
            math::vec2<float> screen_size;
            math::vec3<float> ambient_light;
            gpu_light sun;
        };

        struct hi_z_data
        {
            math::vec2<std::uint32_t> size;
            std::uint32_t mip_count; 
        };

      public:
        render_system(ecs::registry& entities);

        void register_window(iwindow& win);
        void unregister_window(iwindow& win);

        void on_initialize();
        void after_initialize();
        void render();
        void on_close();

        std::vector<mesh_layout> load_mesh(std::span<core::mesh> meshes);
        void load_textures(std::span<texture_data_descriptor> texture_sources, bool generate_mip_maps);
        void load_material(graphics::material_payload& material);

        [[nodiscard]] inline std::uint32_t mesh_count() const noexcept
        {
            return static_cast<std::uint32_t>(_meshes.size());
        }

        [[nodiscard]] inline std::uint32_t material_count() const noexcept
        {
            return static_cast<std::uint32_t>(_materials.size());
        }

        [[nodiscard]] inline std::uint32_t object_count() const noexcept
        {
            return _object_count;
        }

        [[nodiscard]] inline std::uint32_t texture_count() const noexcept
        {
            return static_cast<std::uint32_t>(_images.size());
        }

        inline void allocate_entities(std::uint32_t count)
        {
            _object_count += count;
        }

      private:
        core::heap_allocator _allocator;
        ecs::registry* _registry;

        std::unique_ptr<render_context> _context;
        render_device* _device;
        std::unique_ptr<render_graph> _graph;
        std::unordered_map<iwindow*, swapchain_resource_handle> _swapchains;

        std::vector<image_resource_handle> _images;
        std::vector<buffer_resource_handle> _buffers;
        std::vector<graphics_pipeline_resource_handle> _graphics_pipelines;
        std::vector<compute_pipeline_resource_handle> _compute_pipelines;
        std::vector<sampler_resource_handle> _samplers;

        buffer_resource_handle _vertex_pull_buffer;
        buffer_resource_handle _mesh_layout_buffer;
        buffer_resource_handle _scene_buffer;
        buffer_resource_handle _materials_buffer;
        buffer_resource_handle _instance_buffer;
        buffer_resource_handle _object_buffer;
        buffer_resource_handle _indirect_buffer;
        buffer_resource_handle _hi_z_buffer_constants;

        std::uint32_t _mesh_bytes{0};

        std::vector<mesh_layout> _meshes;
        std::vector<gpu_material_data> _materials;
        std::vector<gpu_object_data> _objects;
        std::vector<indexed_indirect_command> _indirect_draw_commands;
        std::vector<std::uint32_t> _instances;
        std::uint32_t _object_count{0};

        sampler_resource_handle _linear_sampler;
        sampler_resource_handle _closest_sampler;

        graphics_pipeline_resource_handle _pbr_opaque_pipeline;
        graphics_pipeline_resource_handle _pbr_transparencies_pipeline;
        graphics_pipeline_resource_handle _z_prepass_pipeline;
        compute_pipeline_resource_handle _hzb_build_pipeline;

        std::size_t _opaque_object_count{0};
        std::size_t _mask_object_count{0};
        std::size_t _blend_object_count{0};

        graph_pass_handle _pbr_opaque_pass;
        graph_pass_handle _z_prepass_pass;

        gpu_scene_data _scene_data;
        hi_z_data _hi_z_data;
        ecs::entity _camera_entity{ecs::tombstone};

        std::size_t _last_updated_frame{0};

        graphics_pipeline_resource_handle create_pbr_pipeline(bool enable_blend);
        graphics_pipeline_resource_handle create_z_prepass_pipeline();
        compute_pipeline_resource_handle create_hzb_build_pipeline();
    };
} // namespace tempest::graphics

#endif // tempest_graphcis_render_system_hpp