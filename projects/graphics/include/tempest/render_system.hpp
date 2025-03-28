#ifndef tempest_graphcis_render_system_hpp
#define tempest_graphcis_render_system_hpp

#include <tempest/imgui_context.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>
#include <tempest/window.hpp>

#include <tempest/passes/pbr.hpp>
#include <tempest/passes/skybox.hpp>

#include <tempest/archetype.hpp>
#include <tempest/flat_map.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/material.hpp>
#include <tempest/memory.hpp>
#include <tempest/registry.hpp>
#include <tempest/shelf_pack.hpp>
#include <tempest/texture.hpp>
#include <tempest/transform_component.hpp>
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
        anti_aliasing_mode aa_mode{anti_aliasing_mode::NONE};
    };

    class render_system
    {
        struct gpu_object_data
        {
            math::mat4<float> model;
            math::mat4<float> inv_tranpose_model;
            math::mat4<float> prev_model;

            uint32_t mesh_id;
            uint32_t material_id;
            uint32_t parent_id;
            uint32_t self_id;
        };

        enum class gpu_material_type : uint32_t
        {
            PBR_OPAQUE = 0,
            PBR_MASK = 1,
            PBR_BLEND = 2,
            PBR_TRANSMISSIVE = 3,
        };

        struct gpu_material_data
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

        enum class gpu_light_type : uint32_t
        {
            DIRECTIONAL = 0,
            POINT = 1,
            SPOT = 2
        };

        struct alignas(16) shadow_map_parameter
        {
            math::mat4<float> inv_view_mat;
            math::vec4<float> bounds;
            float cascade_split;
        };

        struct alignas(16) gpu_light
        {
            math::vec4<float> color_intensity{};
            math::vec4<float> position_falloff{};
            math::vec4<float> direction{};
            array<uint32_t, 6> shadow_map_indices{};
            gpu_light_type light_type{};
            uint32_t shadow_map_count{};
        };

        struct orthogonal_bounds
        {
            float left;
            float right;
            float bottom;
            float top;
            float near;
            float far;
        };

        struct gpu_scene_data
        {
            gpu_camera_data camera;
            math::vec2<float> screen_size;
            math::vec3<float> ambient_light;
            gpu_light sun;
            uint32_t point_light_count{};
        };

        struct alignas(16) gpu_shadow_map_parameter
        {
            math::mat4<float> light_proj_matrix;
            math::vec4<float> shadow_map_region; // x, y, w, h (normalized)
            float cascade_split_far;
        };

        struct cpu_shadow_map_parameter
        {
            math::mat4<float> proj_matrix;
            math::vec4<uint32_t> shadow_map_bounds;
        };

        struct hi_z_data
        {
            math::vec2<uint32_t> size;
            uint32_t mip_count{};
        };

        struct draw_batch_key
        {
            alpha_behavior alpha_type;
            bool double_sided;

            constexpr auto operator<=>(const draw_batch_key&) const noexcept = default;
        };

        struct draw_batch_payload
        {
            graphics_pipeline_resource_handle pipeline;
            vector<indexed_indirect_command> commands;
            size_t index_buffer_offset = 0;
            ecs::basic_sparse_map<ecs::basic_archetype_registry::entity_type, gpu_object_data> objects;
        };

        struct shadow_map_parameters
        {
            static constexpr size_t max_regions = 6; // 6 cascades or 6 faces of a cube map

            // Requirements:
            // * For directional lights, the number of cascade splits is the number of cascades
            // * For point lights, there are no cascades, just projections for each face

            inplace_vector<math::mat4<float>, max_regions> projections;
            inplace_vector<float, max_regions + 1> cascade_splits;
        };

      public:
        render_system(ecs::archetype_registry& entities, const render_system_settings& settings = {});

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

        flat_unordered_map<guid, mesh_layout> load_meshes(span<const guid> mesh_ids,
                                                          core::mesh_registry& mesh_registry);
        void load_textures(span<const guid> texture_ids, const core::texture_registry& texture_registry,
                           bool generate_mip_maps);
        void load_materials(span<const guid> material_ids, const core::material_registry& material_registry);

        [[nodiscard]] inline uint32_t mesh_count() const noexcept
        {
            return static_cast<uint32_t>(_meshes.size());
        }

        [[nodiscard]] inline uint32_t material_count() const noexcept
        {
            return static_cast<uint32_t>(_materials.size());
        }

        [[nodiscard]] inline uint32_t object_count() const noexcept
        {
            return _object_count;
        }

        [[nodiscard]] inline uint32_t texture_count() const noexcept
        {
            return static_cast<uint32_t>(_images.size());
        }

        inline void allocate_entities(uint32_t count)
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

        void set_skybox_texture(const guid& texture, core::texture_registry& reg);

        void mark_dirty();

        void draw_profiler();

      private:
        heap_allocator _allocator;
        ecs::archetype_registry* _registry;

        unique_ptr<render_context> _context;
        render_device* _device;
        unique_ptr<render_graph> _graph;
        flat_unordered_map<iwindow*, swapchain_resource_handle> _swapchains;

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

        uint32_t _mesh_bytes{0};

        flat_unordered_map<guid, size_t> _image_id_map;
        flat_unordered_map<guid, size_t> _material_id_map;
        flat_unordered_map<guid, size_t> _mesh_id_map;

        vector<mesh_layout> _meshes;
        vector<gpu_material_data> _materials;
        uint32_t _object_count{0};

        flat_map<draw_batch_key, draw_batch_payload> _draw_batches;

        sampler_resource_handle _linear_sampler;
        sampler_resource_handle _point_sampler;
        sampler_resource_handle _linear_sampler_no_aniso;
        sampler_resource_handle _point_sampler_no_aniso;

        graphics_pipeline_resource_handle _z_prepass_pipeline;
        graphics_pipeline_resource_handle _directional_shadow_map_pipeline;
        compute_pipeline_resource_handle _hzb_build_pipeline;

        render_system_settings _settings;
        bool _settings_dirty{false};
        bool _static_data_dirty{true};

        graph_pass_handle _pbr_pass;
        graph_pass_handle _z_prepass_pass;
        graph_pass_handle _shadow_map_pass;
        graph_pass_handle _pbr_oit_gather_pass;
        graph_pass_handle _pbr_oit_resolve_pass;
        graph_pass_handle _pbr_oit_blend_pass;
        graph_pass_handle _skybox_pass;

        image_resource_handle _skybox_texture;

        gpu_scene_data _scene_data{};
        hi_z_data _hi_z_data;
        ecs::archetype_entity _camera_entity{ecs::tombstone};

        size_t _last_updated_frame{2};

        vector<cpu_shadow_map_parameter> _cpu_shadow_map_build_params;
        vector<gpu_shadow_map_parameter> _gpu_shadow_map_use_parameters;
        shelf_pack_allocator _shadow_map_subresource_allocator;

        tempest::function<void()> _create_imgui_hierarchy;

        graphics_pipeline_resource_handle create_z_prepass_pipeline();
        compute_pipeline_resource_handle create_hzb_build_pipeline();
        graphics_pipeline_resource_handle create_taa_resolve_pipeline();
        graphics_pipeline_resource_handle create_sharpen_pipeline();
        graphics_pipeline_resource_handle create_directional_shadow_map_pipeline();

        passes::pbr_pass _pbr;
        passes::pbr_oit_gather_pass _pbr_oit_gather;
        passes::pbr_oit_resolve_pass _pbr_oit_resolve;
        passes::pbr_oit_blend_pass _pbr_oit_blend;
        passes::skybox_pass _skybox;

        shadow_map_parameters compute_shadow_map_cascades(const shadow_map_component& shadowing,
                                                          const ecs::transform_component& light_transform,
                                                          const camera_component& camera_data);

        vector<mesh_layout> _load_meshes(span<core::mesh> meshes);
        void _load_textures(span<texture_data_descriptor> texture_sources, bool generate_mip_maps);
    };
} // namespace tempest::graphics

#endif // tempest_graphcis_render_system_hpp