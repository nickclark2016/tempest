#ifndef tempest_tempest_engine_h
#define tempest_tempest_engine_h

#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/functional.hpp>
#include <tempest/input.hpp>
#include <tempest/renderer.hpp>
#include <tempest/rhi.hpp>
#include <tempest/tuple.hpp>
#include <tempest/vector.hpp>

#include <chrono>

namespace tempest
{
    class engine_context
    {
        struct window_context
        {
            unique_ptr<rhi::window_surface> surface;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> render_surface = rhi::null_handle;
            unique_ptr<core::keyboard> keyboard;
            unique_ptr<core::mouse> mouse;
        };

      public:
        engine_context();

        tuple<rhi::window_surface*, rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>, core::input_group>
        register_window(rhi::window_surface_desc desc, bool install_swapchain_blit = true);
        void register_on_initialize_callback(function<void(engine_context&)> callback);
        void register_on_close_callback(function<void(engine_context&)> callback);
        void register_on_fixed_update_callback(function<void(engine_context&, std::chrono::duration<float>)> callback);
        void register_on_variable_update_callback(
            function<void(engine_context&, std::chrono::duration<float>)> callback);

        [[noreturn]] void run();

        [[nodiscard]] ecs::archetype_registry& get_registry() noexcept;
        [[nodiscard]] const ecs::archetype_registry& get_registry() const noexcept;

        [[nodiscard]] core::material_registry& get_material_registry() noexcept;
        [[nodiscard]] const core::material_registry& get_material_registry() const noexcept;
        [[nodiscard]] core::mesh_registry& get_mesh_registry() noexcept;
        [[nodiscard]] const core::mesh_registry& get_mesh_registry() const noexcept;
        [[nodiscard]] core::texture_registry& get_texture_registry() noexcept;
        [[nodiscard]] const core::texture_registry& get_texture_registry() const noexcept;
        [[nodiscard]] assets::asset_database& get_asset_database() noexcept;
        [[nodiscard]] const assets::asset_database& get_asset_database() const noexcept;

        [[nodiscard]] graphics::renderer& get_renderer() noexcept;
        [[nodiscard]] const graphics::renderer& get_renderer() const noexcept;

        void request_close(bool close = true) noexcept;
        [[nodiscard]] bool should_close() const noexcept;

        ecs::archetype_entity load_entity(ecs::archetype_entity src);

      private:
        ecs::archetype_registry _entity_registry;
        core::material_registry _material_reg;
        core::mesh_registry _mesh_reg;
        core::texture_registry _texture_reg;
        assets::asset_database _asset_database;

        vector<window_context> _windows;
        vector<function<void(engine_context&)>> _on_initialize_callbacks;
        vector<function<void(engine_context&)>> _on_close_callbacks;
        vector<function<void(engine_context&, std::chrono::duration<float>)>> _on_fixed_update_callbacks;
        vector<function<void(engine_context&, std::chrono::duration<float>)>> _on_variable_update_callbacks;

        std::chrono::steady_clock::time_point _last_frame_time{};
        std::chrono::duration<float> _delta_frame_time{};

        bool _should_close{false};

        void _update_fixed(std::chrono::duration<float> dt);
        void _update_variable(std::chrono::duration<float> dt);
        void _render_frame();

        vector<ecs::archetype_entity> _entities_to_load;

        graphics::renderer _render;
    };
} // namespace tempest

#endif // tempest_tempest_engine_h