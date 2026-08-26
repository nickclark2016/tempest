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

        // Clustered Lighting BDA & Bitmask Configuration
        uint64_t lights_address{0};
        uint64_t light_bitmask_address{0};
        uint32_t light_count{0};
        uint32_t words_per_cluster{0}; // ceil(light_count / 32)
        math::vec4<uint32_t> cluster_counts_tile_size{16, 9, 24, 64}; // x, y, z, tile_size_px
        math::vec4<float> cluster_depth_params{0.1F, 1000.0F, 0.0F, 0.0F}; // near, far, log(far/near), pad
    };

    struct TEMPEST_API shadow_cascade_data
    {
        math::mat4<float> view_proj{1.0F};
        math::vec4<float> uv_offset_scale{0.0F, 0.0F, 1.0F, 1.0F}; // xy = offset, zw = scale in atlas
        float split_depth{0.0F};
        float padding[3]{0.0F, 0.0F, 0.0F};
    };

    struct TEMPEST_API directional_shadow_data
    {
        shadow_cascade_data cascades[4];
        uint32_t cascade_count{4};
        float normal_bias{0.02F};
        float depth_bias{0.005F};
        uint32_t debug_mode{0};
    };

    enum class mipmap_generation_mode : uint8_t
    {
        none,       // Do not generate mipmaps; use only mips provided by the asset (1 or more levels).
        if_missing, // (Default) If asset provides fewer than full mips, generate the remaining chain via blit fallback.
        force,      // Force generation of the complete mip chain from level 0 down to 1x1.
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
        mipmap_generation_mode default_mipmap_mode{mipmap_generation_mode::if_missing};
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
                           render_graph::render_graph& graph,
                           mipmap_generation_mode mip_mode = mipmap_generation_mode::if_missing);

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
        [[nodiscard]] auto get_directional_shadow_address() const noexcept -> uint64_t;
        [[nodiscard]] auto get_lights_buffer_address() const noexcept -> uint64_t;
        [[nodiscard]] auto get_object_buffer_address() const noexcept -> uint64_t;
        [[nodiscard]] auto get_instance_buffer_address() const noexcept -> uint64_t;
        [[nodiscard]] auto get_draw_commands_buffer_offset() const noexcept -> uint64_t;
        [[nodiscard]] auto get_frame_slot() const noexcept -> uint32_t { return _frame_slot; }

        // Dynamic per-frame buffer access
        [[nodiscard]] auto get_scene_constants_buffer() const noexcept -> rhi::buffer_handle;
        [[nodiscard]] auto get_directional_shadow_buffer() const noexcept -> rhi::buffer_handle;
        [[nodiscard]] auto get_lights_buffer() const noexcept -> rhi::buffer_handle;
        [[nodiscard]] auto get_object_buffer() const noexcept -> rhi::buffer_handle;
        [[nodiscard]] auto get_instance_buffer() const noexcept -> rhi::buffer_handle;
        [[nodiscard]] auto get_draw_commands_buffer() const noexcept -> rhi::buffer_handle;

        // Frame progression & Per-frame host writes
        void advance_frame() noexcept;
        void write_scene_constants(const scene_constants& constants);
        void write_directional_shadow_data(const directional_shadow_data& data);
        void write_lights(span<const light_payload> lights);
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
        rhi::buffer_handle _directional_shadow_buffer{};
        rhi::buffer_handle _lights_buffer{};
        rhi::buffer_handle _object_buffer{};
        rhi::buffer_handle _instance_buffer{};
        rhi::buffer_handle _draw_commands_buffer{};

        void _init_buffers();
        void _init_samplers();
    };
} // namespace tempest::render_system

#endif // tempest_render_system_resource_pool_hpp
