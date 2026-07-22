#ifndef tempest_editor_editor_engine_context_hpp
#define tempest_editor_editor_engine_context_hpp

#include <tempest/tempest.hpp>

namespace tempest::editor
{
    enum class simulation_state
    {
        pause,
        play
    };

    class TEMPEST_EDITOR_API editor_engine_context final : public engine_context
    {
      public:
        editor_engine_context();

        engine_context::window_registration_info register_window(rhi::window_surface_desc desc,
                                                                 bool install_swapchain_blit = true) override;
        void register_on_initialize_callback(function<void(engine_context&)> callback) override;
        void register_on_close_callback(function<void(engine_context&)> callback) override;
        void register_on_fixed_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback) override;
        void register_on_variable_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback) override;

        void register_on_editor_paint_callback(function<void(engine_context&)> callback);
        void register_on_editor_update_callback(function<void(engine_context&)> callback);

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

        [[nodiscard]] auto get_simulation_state() const -> simulation_state
        {
            return _sim_state;
        }

        auto set_simulation_state(simulation_state state) -> void
        {
            _sim_state = state;
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

        simulation_state _sim_state = simulation_state::pause;

        struct TEMPEST_API window_context
        {
            unique_ptr<rhi::window_surface> surface;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> render_surface = rhi::null_handle;
            unique_ptr<core::keyboard> keyboard;
            unique_ptr<core::mouse> mouse;
        };

        vector<window_context> _windows;
        
        struct
        {
            vector<function<void(engine_context&)>> on_initialize;
            vector<function<void(engine_context&)>> on_close;
            vector<function<void(engine_context&, std::chrono::duration<float>)>> on_fixed_update;
            vector<function<void(engine_context&, std::chrono::duration<float>)>> on_variable_update;
        } _engine_callbacks;

        struct
        {
            vector<function<void(engine_context&)>> on_paint;
            vector<function<void(engine_context&)>> on_update;
        } _editor_callbacks;

        std::chrono::steady_clock::time_point _last_frame_time;
        std::chrono::duration<float> _delta_frame_time;

        bool _should_close{false};

        void _update_fixed(std::chrono::duration<float> delta_time);
        void _update_variable(std::chrono::duration<float> delta_time);
        void _render_frame();

        vector<ecs::entity> _entities_to_load;

        graphics::renderer _render;
    };
} // namespace tempest::editor

#endif // tempest_editor_editor_engine_context_hpp
