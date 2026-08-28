#ifndef tempest_tempest_engine_h
#define tempest_tempest_engine_h

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/functional.hpp>
#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/render_system/renderer.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vector.hpp>
#include <tempest/window_manager.hpp>

#include <chrono>

namespace tempest
{
    /// \brief The engine context is the main interface for interacting with the engine.
    /// It provides access to the core systems of the engine and allows for registration of windows and execution
    /// callbacks.
    class TEMPEST_API engine_context
    {
      public:
        /// \brief Information about a registered window, containing its handle and input group.
        struct TEMPEST_API window_registration_info
        {
            window_handle handle{null_window_handle};
            core::input_group inputs{};
        };

        engine_context() = default;
        engine_context(const engine_context&) = delete;
        engine_context(engine_context&&) noexcept = delete;
        virtual ~engine_context() = default;

        engine_context& operator=(const engine_context&) = delete;
        engine_context& operator=(engine_context&&) noexcept = delete;

        /// \brief Registers a window with the engine, creating the necessary render surface and input routing.
        virtual auto register_window(window_desc desc, bool install_swapchain_blit = true)
            -> window_registration_info = 0;

        /// \brief Registers a callback to be executed when the engine is initialized.
        virtual auto register_on_initialize_callback(function<void(engine_context&)> callback) -> void = 0;

        /// \brief Registers a callback to be executed when the engine is closed.
        virtual auto register_on_close_callback(function<void(engine_context&)> callback) -> void = 0;

        /// \brief Registers a callback to be executed on fixed update.
        virtual auto register_on_fixed_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback) -> void = 0;

        /// \brief Registers a callback to be executed on variable update.
        virtual auto register_on_variable_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback) -> void = 0;

        /// \brief Runs the engine, executing the main loop and processing events.
        virtual auto run() -> void = 0;

        [[nodiscard]] virtual auto get_entities() -> ecs::archetype_registry& = 0;
        [[nodiscard]] virtual auto get_entities() const -> const ecs::archetype_registry& = 0;

        [[nodiscard]] virtual auto get_events() -> event::event_registry& = 0;
        [[nodiscard]] virtual auto get_events() const -> const event::event_registry& = 0;

        [[nodiscard]] virtual auto get_materials() -> core::material_registry& = 0;
        [[nodiscard]] virtual auto get_materials() const -> const core::material_registry& = 0;
        [[nodiscard]] virtual auto get_meshes() -> core::mesh_registry& = 0;
        [[nodiscard]] virtual auto get_meshes() const -> const core::mesh_registry& = 0;
        [[nodiscard]] virtual auto get_textures() -> core::texture_registry& = 0;
        [[nodiscard]] virtual auto get_textures() const -> const core::texture_registry& = 0;

        [[nodiscard]] virtual auto get_assets() -> assets::asset_database& = 0;
        [[nodiscard]] virtual auto get_assets() const -> const assets::asset_database& = 0;

        [[nodiscard]] virtual auto get_renderer() -> render_system::renderer& = 0;
        [[nodiscard]] virtual auto get_renderer() const -> const render_system::renderer& = 0;

        [[nodiscard]] virtual auto get_device() -> rhi::device& = 0;
        [[nodiscard]] virtual auto get_device() const -> const rhi::device& = 0;

        [[nodiscard]] virtual auto get_window_manager() -> window_manager& = 0;
        [[nodiscard]] virtual auto get_window_manager() const -> const window_manager& = 0;

        [[nodiscard]] virtual auto get_render_surface(window_handle win) -> rhi::render_surface* = 0;
        [[nodiscard]] virtual auto get_render_surface(window_handle win) const -> const rhi::render_surface* = 0;
        [[nodiscard]] virtual auto get_raw_surface(window_handle win) const -> rhi::raw_surface_handle = 0;

        virtual auto request_close(bool close = true) -> void = 0;
        [[nodiscard]] virtual auto should_close() const -> bool = 0;

        [[nodiscard]] virtual auto load_entity(ecs::entity src) -> ecs::entity = 0;

        [[nodiscard]] virtual auto get_logger() -> logger& = 0;
        [[nodiscard]] virtual auto get_logger() const -> const logger& = 0;
    };

    class TEMPEST_API standalone_engine_context : public engine_context
    {
      public:
        struct TEMPEST_API window_context
        {
            window_handle handle{null_window_handle};
            rhi::raw_surface_handle raw_surface{};
            unique_ptr<rhi::render_surface> render_surface;
            rhi::surface_format surface_format{};
            rhi::present_mode present_mode{rhi::present_mode::vsync};
            rhi::semaphore_handle acquire_sem{};
            rhi::semaphore_handle timeline_sem{};
            uint64_t timeline_value{0};
            vector<rhi::semaphore_handle> render_semaphores;
            bool need_recreate{false};
        };

        standalone_engine_context();
        ~standalone_engine_context() override;

        auto register_window(window_desc desc, bool install_swapchain_blit = true) -> window_registration_info override;
        auto register_on_initialize_callback(function<void(engine_context&)> callback) -> void override;
        auto register_on_close_callback(function<void(engine_context&)> callback) -> void override;
        auto register_on_fixed_update_callback(function<void(engine_context&, std::chrono::duration<float>)> callback)
            -> void override;
        auto register_on_variable_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback) -> void override;

        auto run() -> void override;

        [[nodiscard]] auto get_entities() -> ecs::archetype_registry& override;
        [[nodiscard]] auto get_entities() const -> const ecs::archetype_registry& override;

        [[nodiscard]] auto get_events() -> event::event_registry& override;
        [[nodiscard]] auto get_events() const -> const event::event_registry& override;

        [[nodiscard]] auto get_materials() -> core::material_registry& override;
        [[nodiscard]] auto get_materials() const -> const core::material_registry& override;
        [[nodiscard]] auto get_meshes() -> core::mesh_registry& override;
        [[nodiscard]] auto get_meshes() const -> const core::mesh_registry& override;
        [[nodiscard]] auto get_textures() -> core::texture_registry& override;
        [[nodiscard]] auto get_textures() const -> const core::texture_registry& override;
        [[nodiscard]] auto get_assets() -> assets::asset_database& override;
        [[nodiscard]] auto get_assets() const -> const assets::asset_database& override;

        [[nodiscard]] auto get_renderer() -> render_system::renderer& override;
        [[nodiscard]] auto get_renderer() const -> const render_system::renderer& override;

        [[nodiscard]] auto get_device() -> rhi::device& override;
        [[nodiscard]] auto get_device() const -> const rhi::device& override;

        [[nodiscard]] auto get_window_manager() -> window_manager& override;
        [[nodiscard]] auto get_window_manager() const -> const window_manager& override;

        [[nodiscard]] auto get_render_surface(window_handle win) -> rhi::render_surface* override;
        [[nodiscard]] auto get_render_surface(window_handle win) const -> const rhi::render_surface* override;
        [[nodiscard]] auto get_raw_surface(window_handle win) const -> rhi::raw_surface_handle override;

        auto request_close(bool close = true) -> void override;
        [[nodiscard]] auto should_close() const -> bool override;

        auto load_entity(ecs::entity src) -> ecs::entity override;

        [[nodiscard]] auto get_logger() -> logger& override
        {
            return _logger;
        }

        [[nodiscard]] auto get_logger() const -> const logger& override
        {
            return _logger;
        }

      protected:
        vector<unique_ptr<log_sink>> _log_sinks;
        logger _logger;

        event::event_registry _event_registry;
        ecs::archetype_registry _entity_registry;
        core::material_registry _material_reg;
        core::mesh_registry _mesh_reg;
        core::texture_registry _texture_reg;
        assets::asset_type_registry _asset_type_reg;
        assets::asset_database _asset_database;

        window_manager _window_manager;
        unique_ptr<rhi::context> _rhi_context;
        unique_ptr<rhi::device> _device;
        unique_ptr<render_system::renderer> _renderer;

        vector<window_context> _windows;
        vector<function<void(engine_context&)>> _on_initialize_callbacks;
        vector<function<void(engine_context&)>> _on_close_callbacks;
        vector<function<void(engine_context&, std::chrono::duration<float>)>> _on_fixed_update_callbacks;
        vector<function<void(engine_context&, std::chrono::duration<float>)>> _on_variable_update_callbacks;

        std::chrono::steady_clock::time_point _last_frame_time;
        std::chrono::duration<float> _delta_frame_time{0.0F};

        bool _should_close{false};

        virtual auto _update_fixed(std::chrono::duration<float> delta_time) -> void;
        virtual auto _update_variable(std::chrono::duration<float> delta_time) -> void;
        virtual auto _render_frame() -> void;
    };
} // namespace tempest

#endif // tempest_tempest_engine_h