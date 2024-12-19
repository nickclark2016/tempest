#ifndef tempest_tempest_engine_h
#define tempest_tempest_engine_h

#include <tempest/asset_database.hpp>
#include <tempest/functional.hpp>
#include <tempest/input.hpp>
#include <tempest/registry.hpp>
#include <tempest/render_system.hpp>
#include <tempest/vector.hpp>
#include <tempest/window.hpp>

#include <chrono>
#include <functional>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace tempest
{
    class engine
    {
        struct window_payload
        {
            std::unique_ptr<graphics::iwindow> window;
            std::unique_ptr<core::keyboard> keyboard;
            std::unique_ptr<core::mouse> mouse;
        };

        engine();

      public:
        static engine initialize();

        std::tuple<graphics::iwindow*, core::input_group> add_window(std::unique_ptr<graphics::iwindow> window);
        void update(float dt);
        void render();
        void shutdown();

        std::chrono::duration<float> delta_time() const noexcept
        {
            return _delta_time;
        }

        ecs::registry& get_registry()
        {
            return _entity_registry;
        }

        const ecs::registry& get_registry() const
        {
            return _entity_registry;
        }

        void request_close() noexcept
        {
            _should_close = true;
        }

        void on_initialize(function<void(engine&)>&& callback)
        {
            _initialize_callbacks.push_back(std::move(callback));
        }

        void on_close(function<void(engine&)>&& callback)
        {
            _close_callbacks.push_back(std::move(callback));
        }

        void on_update(function<void(engine&, float)>&& callback)
        {
            _update_callbacks.push_back(std::move(callback));
        }

        graphics::render_system& get_render_system()
        {
            return _render_system;
        }

        const graphics::render_system& get_render_system() const
        {
            return _render_system;
        }

        ecs::entity load_entity(ecs::entity src);

        [[noreturn]] void run();

        assets::asset_database& get_asset_database()
        {
            return _asset_database;
        }

        const assets::asset_database& get_asset_database() const
        {
            return _asset_database;
        }

        core::mesh_registry& get_mesh_registry()
        {
            return _mesh_reg;
        }

        const core::mesh_registry& get_mesh_registry() const
        {
            return _mesh_reg;
        }

        core::material_registry& get_material_registry()
        {
            return _material_reg;
        }

        const core::material_registry& get_material_registry() const
        {
            return _material_reg;
        }

        core::texture_registry& get_texture_registry()
        {
            return _texture_reg;
        }

        const core::texture_registry& get_texture_registry() const
        {
            return _texture_reg;
        }

      private:
        ecs::registry _entity_registry;
        core::material_registry _material_reg;
        core::mesh_registry _mesh_reg;
        core::texture_registry _texture_reg;
        assets::asset_database _asset_database;
        vector<window_payload> _windows;

        vector<function<void(engine&)>> _initialize_callbacks;
        vector<function<void(engine&)>> _close_callbacks;
        vector<function<void(engine&, float)>> _update_callbacks;

        std::chrono::steady_clock::time_point _last_frame_time{};
        std::chrono::duration<float> _delta_time{};

        graphics::render_system _render_system;

        bool _should_close{false};
    };
} // namespace tempest

#endif // tempest_tempest_engine_h