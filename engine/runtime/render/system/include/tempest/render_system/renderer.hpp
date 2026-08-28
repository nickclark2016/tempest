#ifndef tempest_render_system_renderer_hpp
#define tempest_render_system_renderer_hpp

#include <tempest/api.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/functional.hpp>
#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/render_system/shelf_allocator.hpp>
#include <tempest/rhi.hpp>
#include <tempest/texture.hpp>
#include <tempest/transform_component.hpp>

namespace tempest::render_system
{
    struct TEMPEST_API renderer_config
    {
        uint32_t render_width{1920};
        uint32_t render_height{1080};
        rhi::data_format hdr_color_format{rhi::data_format::rgba16_float};
        rhi::data_format depth_format{rhi::data_format::depth32_float};
        rhi::data_format tonemapped_color_format{rhi::data_format::rgba8_srgb};
        bool enable_ssao{false};
        shadow_debug_mode shadow_debug{shadow_debug_mode::none};
        float cluster_far_plane{1000.0F};
        resource_pool_config pool_config{};
    };

    struct TEMPEST_API renderer_inputs
    {
        ecs::registry* entity_registry{nullptr};
        camera_system* camera_sys{nullptr};
        const core::mesh_registry* meshes{nullptr};
        const core::texture_registry* textures{nullptr};
        const core::material_registry* materials{nullptr};
    };

    class TEMPEST_API renderer
    {
      public:
        class TEMPEST_API builder
        {
          public:
            builder() = default;

            builder& set_config(const renderer_config& cfg)
            {
                _cfg = cfg;
                return *this;
            }

            builder& set_inputs(const renderer_inputs& inputs)
            {
                _inputs = inputs;
                return *this;
            }

            [[nodiscard]] auto build(rhi::device& dev, logger& log) -> unique_ptr<renderer>;

          private:
            renderer_config _cfg{};
            renderer_inputs _inputs{};
        };

        explicit renderer(rhi::device& dev, logger& log, renderer_config cfg, renderer_inputs inputs,
                          unique_ptr<camera_system> camera_sys = nullptr);
        ~renderer();

        renderer(const renderer&) = delete;
        renderer& operator=(const renderer&) = delete;
        renderer(renderer&&) noexcept;
        renderer& operator=(renderer&&) noexcept;

        /// @brief Builds the complete Render Graph DAG for the frame.
        void prepare_frame(uint32_t width, uint32_t height, optional<rhi::texture_handle> swapchain_tex = nullopt,
                           optional<rhi::texture_view_handle> swapchain_view = nullopt,
                           optional<render_camera> camera_override = nullopt);

        /// @brief Executes the compiled Render Graph DAG on the GPU.
        auto render(const render_graph::frame_sync_options& sync = {}) -> expected<void, render_graph::execution_error>;

        /// @brief Resizes render targets and surface.
        void resize(uint32_t width, uint32_t height);

        [[nodiscard]] auto get_config() const noexcept -> const renderer_config&
        {
            return _cfg;
        }

        [[nodiscard]] auto get_tracked_renderable_count() const noexcept -> size_t
        {
            return _tracked_entities.size();
        }

        [[nodiscard]] auto get_device() noexcept -> rhi::device&
        {
            return *_device;
        }

        [[nodiscard]] auto get_camera_system() noexcept -> camera_system&
        {
            return *_camera_system;
        }

        [[nodiscard]] auto get_resource_pool() noexcept -> resource_pool&
        {
            return _pool;
        }

        [[nodiscard]] auto get_shader_manager() noexcept -> shader_manager&
        {
            return _shaders;
        }

        [[nodiscard]] auto get_render_graph() noexcept -> render_graph::render_graph&
        {
            return _graph;
        }

        [[nodiscard]] auto get_directional_shadow_atlas_texture() const noexcept -> render_graph::rg_texture_id
        {
            return _directional_shadow_atlas_target;
        }

        [[nodiscard]] auto get_punctual_shadow_atlas_texture() const noexcept -> render_graph::rg_texture_id
        {
            return _punctual_shadow_atlas_target;
        }

        [[nodiscard]] auto get_tonemapped_color_texture() const noexcept -> render_graph::rg_texture_id
        {
            return _tonemapped_color_target;
        }

        [[nodiscard]] auto get_moments_texture() const noexcept -> render_graph::rg_texture_id
        {
            return _moments_target;
        }

        [[nodiscard]] auto get_zeroth_moment_texture() const noexcept -> render_graph::rg_texture_id
        {
            return _zeroth_moment_target;
        }

        [[nodiscard]] auto get_transparency_accum_texture() const noexcept -> render_graph::rg_texture_id
        {
            return _transparency_accum_target;
        }

        [[nodiscard]] auto get_cluster_bounds_buffer() const noexcept -> render_graph::rg_buffer_id
        {
            return _cluster_bounds_target;
        }

        [[nodiscard]] auto get_light_bitmask_buffer() const noexcept -> render_graph::rg_buffer_id
        {
            return _light_bitmask_target;
        }

        [[nodiscard]] auto get_active_draw_count() const noexcept -> uint32_t
        {
            return _active_draw_count;
        }

        [[nodiscard]] auto get_opaque_draw_count() const noexcept -> uint32_t
        {
            return _opaque_draw_count;
        }

        [[nodiscard]] auto get_transparent_draw_count() const noexcept -> uint32_t
        {
            return _transparent_draw_count;
        }

        [[nodiscard]] auto get_opaque_draw_offset() const noexcept -> uint32_t
        {
            return _opaque_draw_offset;
        }

        [[nodiscard]] auto get_transparent_draw_offset() const noexcept -> uint32_t
        {
            return _transparent_draw_offset;
        }

        void set_shadow_debug_mode(shadow_debug_mode mode) noexcept
        {
            _shadow_debug_mode = mode;
        }

        [[nodiscard]] auto get_shadow_debug_mode() const noexcept -> shadow_debug_mode
        {
            return _shadow_debug_mode;
        }

        [[nodiscard]] auto get_tracked_point_light_count() const noexcept -> size_t
        {
            return _point_light_entities.size();
        }

        [[nodiscard]] auto get_cached_lights() const noexcept -> span<const light_payload>
        {
            return _cached_lights;
        }

      private:
        rhi::device* _device{nullptr};
        logger* _log{nullptr};
        renderer_config _cfg;
        renderer_inputs _inputs;
        unique_ptr<camera_system> _owned_camera_system;
        camera_system* _camera_system{nullptr};

        resource_pool _pool;
        shader_manager _shaders;
        render_graph::render_graph _graph;

        // Render Targets (Transient in Render Graph)
        render_graph::rg_texture_id _directional_shadow_atlas_target{};
        render_graph::rg_texture_id _punctual_shadow_atlas_target{};
        render_graph::rg_texture_id _hdr_color_target{};
        render_graph::rg_texture_id _depth_target{};
        render_graph::rg_texture_id _ssao_target{};
        render_graph::rg_texture_id _ssao_blurred_target{};
        render_graph::rg_texture_id _moments_target{};
        render_graph::rg_texture_id _zeroth_moment_target{};
        render_graph::rg_texture_id _transparency_accum_target{};
        render_graph::rg_texture_id _tonemapped_color_target{};
        render_graph::rg_buffer_id _cluster_bounds_target{};
        render_graph::rg_buffer_id _light_bitmask_target{};

        shelf_allocator _directional_shadow_allocator{};
        shelf_allocator _punctual_shadow_allocator{};
        vector<ecs::entity> _tracked_entities{};
        uint32_t _active_draw_count{0};
        uint32_t _opaque_draw_count{0};
        uint32_t _opaque_draw_offset{0};
        uint32_t _transparent_draw_count{0};
        uint32_t _transparent_draw_offset{0};
        shadow_debug_mode _shadow_debug_mode{shadow_debug_mode::none};

        // Renderable Tracking & ECS Subscriptions
        flat_unordered_map<ecs::entity, size_t> _renderable_indices{};
        uint32_t _renderables_dirty_count{0};

        event::event_registry* _events{nullptr};
        event::subscription_handle<ecs::component_added_event<ecs::entity, core::mesh_component>> _mesh_added_sub{};
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, core::mesh_component>>
            _mesh_replaced_sub{};
        event::subscription_handle<ecs::component_removed_event<ecs::entity, core::mesh_component>> _mesh_removed_sub{};
        event::subscription_handle<ecs::component_added_event<ecs::entity, core::material_component>>
            _material_added_sub{};
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, core::material_component>>
            _material_replaced_sub{};
        event::subscription_handle<ecs::component_removed_event<ecs::entity, core::material_component>>
            _material_removed_sub{};

        // Light Tracking & ECS Subscriptions
        flat_unordered_map<ecs::entity, size_t> _point_light_indices{};
        vector<ecs::entity> _point_light_entities{};
        vector<light_payload> _cached_lights{};
        uint32_t _lights_dirty_count{0};

        event::subscription_handle<ecs::component_added_event<ecs::entity, point_light_component>>
            _point_light_added_sub{};
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, point_light_component>>
            _point_light_replaced_sub{};
        event::subscription_handle<ecs::component_removed_event<ecs::entity, point_light_component>>
            _point_light_removed_sub{};
        event::subscription_handle<ecs::component_added_event<ecs::entity, directional_light_component>>
            _dir_light_added_sub{};
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, directional_light_component>>
            _dir_light_replaced_sub{};
        event::subscription_handle<ecs::component_removed_event<ecs::entity, directional_light_component>>
            _dir_light_removed_sub{};
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, ecs::transform_component>>
            _transform_replaced_sub{};
        event::subscription_handle<ecs::entity_destroyed_event<ecs::entity>> _entity_destroyed_sub{};

        void _subscribe_events();
        void _unsubscribe_events();
        void _init_renderables_from_registry();
        void _init_lights_from_registry();
        void _ensure_assets_loaded();
        void _update_renderable_commands();
    };
} // namespace tempest::render_system

#endif // tempest_render_system_renderer_hpp
