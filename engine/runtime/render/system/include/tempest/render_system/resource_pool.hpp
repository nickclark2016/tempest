#ifndef tempest_render_system_resource_pool_hpp
#define tempest_render_system_resource_pool_hpp

#include <tempest/api.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/material.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/texture.hpp>
#include <tempest/vector.hpp>
#include <tempest/vertex.hpp>

namespace tempest::render_system
{
    struct TEMPEST_API scene_constants
    {
        // Camera (matches Camera in camera.slang, 272 bytes)
        math::mat4<float> projection{1.0F};
        math::mat4<float> inv_projection{1.0F};
        math::mat4<float> view{1.0F};
        math::mat4<float> inv_view{1.0F};
        math::vec4<float> camera_position{0.0F, 0.0F, 0.0F, 1.0F};

        // Environment & Lighting (matches SceneGlobals in scene.slang)
        math::vec4<float> ambient_light{0.05F, 0.05F, 0.08F, 1.0F};
        math::vec4<float> sun_color_intensity{1.0F, 1.0F, 1.0F, 1.0F};
        math::vec4<float> sun_direction{0.0F, -1.0F, 0.0F, 0.0F};

        // Screen parameters
        math::vec2<float> screen_size{1920.0F, 1080.0F};
        math::vec2<float> inv_screen_size{1.0F / 1920.0F, 1.0F / 1080.0F};

        // Directional Shadows (matches ShadowParameters in shadows.slang)
        shadow_parameters_gpu shadows{};
    };

    struct TEMPEST_API resource_pool_config
    {
        uint32_t initial_vertex_buffer_size{128 * 1024 * 1024};
        uint32_t max_mesh_count{65536};
        uint32_t max_material_count{65536};
        uint32_t max_object_count{262144};
        uint32_t max_instance_count{262144};
        uint32_t max_draw_command_count{65536};
        uint32_t max_lights{256};
        uint32_t staging_buffer_size{128 * 1024 * 1024};
        uint32_t frames_in_flight{2};
    };

    struct TEMPEST_API texture_entry
    {
        rhi::texture_handle texture{};
        rhi::texture_view_handle view{};
        rhi::descriptor_handle descriptor{};
    };

    class TEMPEST_API resource_pool
    {
      public:
        explicit resource_pool(rhi::device& dev, resource_pool_config cfg = {});
        ~resource_pool();

        resource_pool(const resource_pool&) = delete;
        resource_pool& operator=(const resource_pool&) = delete;
        resource_pool(resource_pool&&) noexcept;
        resource_pool& operator=(resource_pool&&) noexcept;

        // Static resource loading & upload
        void load_meshes(span<const guid> mesh_ids, const core::mesh_registry& registry,
                         render_graph::render_graph& graph);
        void load_materials(span<const guid> material_ids, const core::material_registry& registry,
                            render_graph::render_graph& graph);
        void load_textures(span<const guid> texture_ids, const core::texture_registry& registry,
                           render_graph::render_graph& graph);

        // Address resolution for BDA
        [[nodiscard]] auto get_vertex_buffer_address() const noexcept -> uint64_t;
        [[nodiscard]] auto get_mesh_table_address() const noexcept -> uint64_t;
        [[nodiscard]] auto get_material_table_address() const noexcept -> uint64_t;
        [[nodiscard]] auto get_mesh_address(const guid& id) const noexcept -> uint64_t;
        [[nodiscard]] auto get_material_address(const guid& id) const noexcept -> uint64_t;
        [[nodiscard]] auto get_mesh_layout(const guid& id) const noexcept -> optional<mesh_layout>;
        [[nodiscard]] auto get_material_type(const guid& id) const noexcept -> optional<material_type>;
        [[nodiscard]] auto get_material(const guid& id) const noexcept -> optional<material_payload>;
        [[nodiscard]] auto get_texture_descriptor_index(const guid& id) const noexcept -> int16_t;
        [[nodiscard]] auto get_scene_constants_address() const noexcept -> uint64_t;

        // Dynamic per-frame buffer access
        [[nodiscard]] auto get_scene_constants_buffer() const noexcept -> rhi::buffer_handle;
        [[nodiscard]] auto get_object_buffer() const noexcept -> rhi::buffer_handle;
        [[nodiscard]] auto get_instance_buffer() const noexcept -> rhi::buffer_handle;
        [[nodiscard]] auto get_draw_commands_buffer() const noexcept -> rhi::buffer_handle;

        // Frame progression & Per-frame host writes
        void advance_frame() noexcept;
        void write_scene_constants(const scene_constants& constants);
        void write_objects(span<const object_payload> objects);
        void write_instances(span<const uint32_t> instances);
        void write_draw_commands(span<const indexed_indirect_command> commands);

        // Static buffer access
        [[nodiscard]] auto get_vertex_buffer() const noexcept -> rhi::buffer_handle { return _vertex_buffer; }
        [[nodiscard]] auto get_mesh_table_buffer() const noexcept -> rhi::buffer_handle { return _mesh_table_buffer; }
        [[nodiscard]] auto get_material_table_buffer() const noexcept -> rhi::buffer_handle { return _material_table_buffer; }

        // Samplers
        [[nodiscard]] auto get_linear_sampler() const noexcept -> rhi::sampler_handle;
        [[nodiscard]] auto get_point_sampler() const noexcept -> rhi::sampler_handle;
        [[nodiscard]] auto get_linear_sampler_descriptor() const noexcept -> rhi::descriptor_handle;
        [[nodiscard]] auto get_point_sampler_descriptor() const noexcept -> rhi::descriptor_handle;

        void clear_staging_buffers();
        void release_all();

      private:
        rhi::device* _device{nullptr};
        resource_pool_config _cfg;

        // Persistent Device Buffers
        rhi::buffer_handle _vertex_buffer{};
        rhi::buffer_handle _mesh_table_buffer{};
        rhi::buffer_handle _material_table_buffer{};
        rhi::buffer_handle _staging_buffer{};
        vector<rhi::buffer_handle> _staging_buffers_to_free;

        uint64_t _vertex_bytes_allocated{0};
        uint32_t _mesh_count{0};
        uint32_t _material_count{0};
        uint32_t _frame_slot{0};

        flat_unordered_map<guid, uint32_t> _mesh_indices;
        flat_unordered_map<guid, mesh_layout> _mesh_layouts;
        flat_unordered_map<guid, uint32_t> _material_indices;
        flat_unordered_map<guid, material_payload> _materials;

        // Persistent Textures & Bindless
        flat_unordered_map<guid, texture_entry> _textures;
        rhi::sampler_handle _linear_sampler{};
        rhi::sampler_handle _point_sampler{};
        rhi::descriptor_handle _linear_sampler_descriptor{};
        rhi::descriptor_handle _point_sampler_descriptor{};

        // Dynamic Upload Buffers
        rhi::buffer_handle _scene_constants_buffer{};
        rhi::buffer_handle _object_buffer{};
        rhi::buffer_handle _instance_buffer{};
        rhi::buffer_handle _draw_commands_buffer{};

        void _init_buffers();
        void _init_samplers();
    };
} // namespace tempest::render_system

#endif // tempest_render_system_resource_pool_hpp
