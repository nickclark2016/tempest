#ifndef tempest_graphcis_render_system_hpp
#define tempest_graphcis_render_system_hpp

#include "imgui_context.hpp"
#include "render_device.hpp"
#include "render_graph.hpp"
#include "window.hpp"

#include <tempest/flat_map.hpp>
#include <tempest/material.hpp>
#include <tempest/memory.hpp>
#include <tempest/registry.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

#include <string>
#include <vector>

namespace tempest::graphics
{
    enum class anti_aliasing_mode
    {
        NONE,
        MSAA,
        TAA
    };

    struct render_system_settings
    {
        bool should_show_settings{false};
        bool enable_imgui{false};
        bool enable_profiling{false};
        anti_aliasing_mode aa_mode{anti_aliasing_mode::TAA};
    };

    class render_system
    {
        struct gpu_object_data
        {
            math::mat4<float> model;
            math::mat4<float> inv_tranpose_model;
            math::mat4<float> prev_model;

            std::uint32_t mesh_id;
            std::uint32_t material_id;
            std::uint32_t parent_id;
            std::uint32_t self_id;
        };

        enum class gpu_material_type : std::uint32_t
        {
            PBR_OPAQUE = 0,
            PBR_MASK = 1,
            PBR_BLEND = 2,
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
            math::mat4<float> prev_proj;
            math::mat4<float> view;
            math::mat4<float> inv_view;
            math::mat4<float> prev_view;
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
            math::vec4<float> jitter;
            gpu_light sun;
        };

        struct hi_z_data
        {
            math::vec2<std::uint32_t> size;
            std::uint32_t mip_count;
        };

        struct draw_batch_key
        {
            alpha_behavior alpha_type;

            constexpr auto operator<=>(const draw_batch_key&) const noexcept = default;
        };

        struct draw_batch_payload
        {
            graphics_pipeline_resource_handle pipeline;
            vector<indexed_indirect_command> commands;
            ecs::sparse_map<gpu_object_data> objects;
        };

      public:
        render_system(ecs::registry& entities, const render_system_settings& settings = {});

        void register_window(iwindow& win);
        void unregister_window(iwindow& win);

        void on_initialize();
        void after_initialize();
        void render();
        void on_close();

        void update_settings(const render_system_settings& settings);

        [[nodiscard]] inline const render_system_settings& settings() const noexcept
        {
            return _settings;
        }

        flat_unordered_map<guid, mesh_layout> load_meshes(span<const guid> mesh_ids, core::mesh_registry& mesh_registry);
        void load_textures(span<const guid> texture_ids, const core::texture_registry& texture_registry,
                           bool generate_mip_maps);
        void load_materials(span<const guid> material_ids, const core::material_registry& material_registry);

        vector<mesh_layout> load_meshes(span<core::mesh> meshes);
        void load_textures(span<texture_data_descriptor> texture_sources, bool generate_mip_maps);
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

        inline optional<size_t> get_mesh_id(const guid& id) const
        {
            if (auto it = _mesh_id_map.find(id); it != _mesh_id_map.end())
            {
                return it->second;
            }
            return none();
        }

        inline optional<size_t> get_material_id(const guid& id) const
        {
            if (auto it = _material_id_map.find(id); it != _material_id_map.end())
            {
                return it->second;
            }
            return none();
        }

        inline size_t acquire_new_object() noexcept
        {
            return _object_count++;
        }

        template <typename Fn>
        inline void draw_imgui(Fn&& fn)
        {
            _create_imgui_hierarchy = tempest::forward<Fn>(fn);
        }

        void mark_dirty();

        void draw_profiler();

      private:
        heap_allocator _allocator;
        ecs::registry* _registry;

        std::unique_ptr<render_context> _context;
        render_device* _device;
        std::unique_ptr<render_graph> _graph;
        std::unordered_map<iwindow*, swapchain_resource_handle> _swapchains;

        vector<image_resource_handle> _images;
        vector<buffer_resource_handle> _buffers;
        vector<graphics_pipeline_resource_handle> _graphics_pipelines;
        vector<compute_pipeline_resource_handle> _compute_pipelines;
        vector<sampler_resource_handle> _samplers;

        buffer_resource_handle _vertex_pull_buffer;
        buffer_resource_handle _mesh_layout_buffer;
        buffer_resource_handle _scene_buffer;
        buffer_resource_handle _materials_buffer;
        buffer_resource_handle _instance_buffer;
        buffer_resource_handle _object_buffer;
        buffer_resource_handle _indirect_buffer;
        buffer_resource_handle _hi_z_buffer_constants;

        std::uint32_t _mesh_bytes{0};

        flat_unordered_map<guid, size_t> _image_id_map;
        flat_unordered_map<guid, size_t> _material_id_map;
        flat_unordered_map<guid, size_t> _mesh_id_map;

        vector<mesh_layout> _meshes;
        vector<gpu_material_data> _materials;
        std::uint32_t _object_count{0};

        flat_map<draw_batch_key, draw_batch_payload> _draw_batches;

        sampler_resource_handle _linear_sampler;
        sampler_resource_handle _point_sampler;
        sampler_resource_handle _linear_sampler_no_aniso;
        sampler_resource_handle _point_sampler_no_aniso;

        graphics_pipeline_resource_handle _pbr_opaque_pipeline;
        graphics_pipeline_resource_handle _pbr_transparencies_pipeline;
        graphics_pipeline_resource_handle _z_prepass_pipeline;
        compute_pipeline_resource_handle _hzb_build_pipeline;

        render_system_settings _settings;
        bool _settings_dirty{false};
        bool _static_data_dirty{true};

        graph_pass_handle _pbr_pass;
        graph_pass_handle _pbr_msaa_pass;
        graph_pass_handle _z_prepass_pass;
        graph_pass_handle _z_prepass_msaa_pass;

        gpu_scene_data _scene_data;
        hi_z_data _hi_z_data;
        ecs::entity _camera_entity{ecs::tombstone};

        std::size_t _last_updated_frame{2};

        tempest::function<void()> _create_imgui_hierarchy;

        graphics_pipeline_resource_handle create_pbr_pipeline(bool enable_blend);
        graphics_pipeline_resource_handle create_z_prepass_pipeline();
        compute_pipeline_resource_handle create_hzb_build_pipeline();
        graphics_pipeline_resource_handle create_taa_resolve_pipeline();
        graphics_pipeline_resource_handle create_sharpen_pipeline();
    };
} // namespace tempest::graphics

#endif // tempest_graphcis_render_system_hpp