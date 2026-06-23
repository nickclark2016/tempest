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
#include <tempest/renderer.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/tuple.hpp>
#include <tempest/vector.hpp>

#include <chrono>

namespace tempest
{
    /// <summary>
    /// The engine context is the main interface for interacting with the engine. It provides access to the core systems
    /// of the engine and allows for registration of windows and execution callbacks.
    /// </summary>
    class TEMPEST_API engine_context
    {
      public:
        /// <summary>
        /// Information about a registered window, including the window surface, the render surface, and the input
        /// group.
        /// </summary>
        struct TEMPEST_API window_registration_info
        {
            /// <summary>
            /// The window surface associated with the registered window.
            /// </summary>
            rhi::window_surface* surface = nullptr;

            /// <summary>
            /// The render surface associated with the registered window.
            /// </summary>
            rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> render_surface = rhi::null_handle;

            /// <summary>
            /// The input group associated with the registered window.
            /// </summary>
            core::input_group inputs;
        };

        engine_context() = default;
        engine_context(const engine_context&) = delete;
        engine_context(engine_context&&) noexcept = delete;
        virtual ~engine_context() = default;

        engine_context& operator=(const engine_context&) = delete;
        engine_context& operator=(engine_context&&) noexcept = delete;

        /// <summary>
        /// Registers a window with the engine, creating the necessary render surface and input group for the window.
        /// </summary>
        /// <param name="desc">The description of the window to register.</param>
        /// <param name="install_swapchain_blit">Whether to install a swapchain blit callback for the window. This is
        /// used to automatically blit the swapchain to the render surface after rendering.</param> <returns>Information
        /// about the registered window, including the window surface, render surface, and input group.</returns>
        virtual auto register_window(rhi::window_surface_desc desc, bool install_swapchain_blit = true)
            -> window_registration_info = 0;

        /// <summary>
        /// Registers a callback to be executed when the engine is initialized.
        /// </summary>
        /// <param name="callback">
        // The callback function to execute on engine initialization. The callback receives a
        /// reference to the engine context as a parameter.
        // </param>
        virtual auto register_on_initialize_callback(function<void(engine_context&)> callback) -> void = 0;

        /// <summary>
        /// Registers a callback to be executed when the engine is closed.
        /// </summary>
        /// <param name="callback">
        /// The callback function to execute on engine close. The callback receives a reference to the engine context
        /// as a parameter.
        /// </param>
        virtual auto register_on_close_callback(function<void(engine_context&)> callback) -> void = 0;

        /// <summary>
        /// Registers a callback to be executed on fixed update. Fixed updates are executed at a fixed time step, and
        /// are typically used for physics updates and other time-sensitive logic.
        /// </summary>
        /// <param name="callback">
        /// The callback function to execute on fixed update. The callback receives a reference to the engine context
        /// and the fixed delta time as parameters.
        /// </param>
        virtual auto register_on_fixed_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback) -> void = 0;

        /// <summary>
        /// Registers a callback to be executed on variable update. Variable updates are executed once per frame, and
        /// are typically used for rendering and other non-time-sensitive logic.
        /// </summary> <param name="callback">
        /// The callback function to execute on variable update. The callback receives a reference to the engine context
        /// and the variable delta time as parameters.
        /// </param>
        virtual auto register_on_variable_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback) -> void = 0;

        /// <summary>
        /// Runs the engine, executing the main loop and processing events. This function will block until the engine
        /// is closed.
        /// </summary>
        [[noreturn]] virtual auto run() -> void = 0;

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

        [[nodiscard]] virtual auto get_renderer() -> graphics::renderer& = 0;
        [[nodiscard]] virtual auto get_renderer() const -> const graphics::renderer& = 0;

        virtual auto request_close(bool close = true) -> void = 0;
        [[nodiscard]] virtual auto should_close() const -> bool = 0;

        [[nodiscard]] virtual auto load_entity(ecs::entity src) -> ecs::entity = 0;

        [[nodiscard]] virtual auto get_logger() -> logger& = 0;
        [[nodiscard]] virtual auto get_logger() const -> const logger& = 0;
    };

    class TEMPEST_API standalone_engine_context : public engine_context
    {
        struct TEMPEST_API window_context
        {
            unique_ptr<rhi::window_surface> surface;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> render_surface = rhi::null_handle;
            unique_ptr<core::keyboard> keyboard;
            unique_ptr<core::mouse> mouse;
        };

      public:
        standalone_engine_context();

        engine_context::window_registration_info register_window(rhi::window_surface_desc desc,
                                                                 bool install_swapchain_blit = true) override;
        void register_on_initialize_callback(function<void(engine_context&)> callback) override;
        void register_on_close_callback(function<void(engine_context&)> callback) override;
        void register_on_fixed_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback) override;
        void register_on_variable_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback) override;

        [[noreturn]] void run() override;

        [[nodiscard]] ecs::archetype_registry& get_entities() override;
        [[nodiscard]] const ecs::archetype_registry& get_entities() const override;

        [[nodiscard]] event::event_registry& get_events() override;
        [[nodiscard]] const event::event_registry& get_events() const override;

        [[nodiscard]] core::material_registry& get_materials() override;
        [[nodiscard]] const core::material_registry& get_materials() const override;
        [[nodiscard]] core::mesh_registry& get_meshes() override;
        [[nodiscard]] const core::mesh_registry& get_meshes() const override;
        [[nodiscard]] core::texture_registry& get_textures() override;
        [[nodiscard]] const core::texture_registry& get_textures() const override;
        [[nodiscard]] assets::asset_database& get_assets() override;
        [[nodiscard]] const assets::asset_database& get_assets() const override;

        [[nodiscard]] graphics::renderer& get_renderer() override;
        [[nodiscard]] const graphics::renderer& get_renderer() const override;

        void request_close(bool close) override;
        [[nodiscard]] bool should_close() const override;

        ecs::entity load_entity(ecs::entity src) override;

        [[nodiscard]] auto get_logger() -> logger& override
        {
            return _logger;
        }

        [[nodiscard]] auto get_logger() const -> const logger& override
        {
            return _logger;
        }

      private:
        vector<unique_ptr<log_sink>> _log_sinks;
        logger _logger;

        event::event_registry _event_registry;
        ecs::archetype_registry _entity_registry;
        core::material_registry _material_reg;
        core::mesh_registry _mesh_reg;
        core::texture_registry _texture_reg;
        assets::asset_type_registry _asset_type_reg;
        assets::asset_database _asset_database;

        vector<window_context> _windows;
        vector<function<void(engine_context&)>> _on_initialize_callbacks;
        vector<function<void(engine_context&)>> _on_close_callbacks;
        vector<function<void(engine_context&, std::chrono::duration<float>)>> _on_fixed_update_callbacks;
        vector<function<void(engine_context&, std::chrono::duration<float>)>> _on_variable_update_callbacks;

        std::chrono::steady_clock::time_point _last_frame_time;
        std::chrono::duration<float> _delta_frame_time;

        bool _should_close{false};

        void _update_fixed(std::chrono::duration<float> delta_time);
        void _update_variable(std::chrono::duration<float> delta_time);
        void _render_frame();

        vector<ecs::entity> _entities_to_load;

        graphics::renderer _render;
    };
} // namespace tempest

#endif // tempest_tempest_engine_h