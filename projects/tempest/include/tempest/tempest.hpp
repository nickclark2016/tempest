#ifndef tempest_tempest_engine_h
#define tempest_tempest_engine_h

#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/functional.hpp>
#include <tempest/input.hpp>
#include <tempest/registry.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/render_system.hpp>
#include <tempest/rhi.hpp>
#include <tempest/tuple.hpp>
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

        ecs::archetype_registry& get_archetype_registry()
        {
            return _archetype_entity_registry;
        }

        const ecs::archetype_registry& get_archetype_registry() const
        {
            return _archetype_entity_registry;
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

        ecs::archetype_entity load_entity(ecs::archetype_entity src);

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
        ecs::archetype_registry _archetype_entity_registry;
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

        void _start_frame();
    };

    class engine_context
    {
        struct window_context
        {
            unique_ptr<rhi::window_surface> surface;
            unique_ptr<core::keyboard> keyboard;
            unique_ptr<core::mouse> mouse;
        };

      public:
        engine_context();

        tuple<rhi::window_surface*, core::input_group> register_window(rhi::window_surface_desc desc);
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

        template <typename T, typename... Ts>
            requires derived_from<T, graphics::render_pipeline>
        T* register_pipeline(rhi::window_surface* surface, Ts&&... args);

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

        graphics::renderer _render;

        bool _should_close{false};

        void _update_fixed(std::chrono::duration<float> dt);
        void _update_variable(std::chrono::duration<float> dt);
        void _render_frame();

        vector<ecs::archetype_entity> _entities_to_load;
    };

    template <typename T, typename... Ts>
        requires derived_from<T, graphics::render_pipeline>
    inline T* engine_context::register_pipeline(rhi::window_surface* surface, Ts&&... args)
    {
        return _render.register_window<T>(surface, tempest::forward<Ts>(args)...);
    }
} // namespace tempest

#endif // tempest_tempest_engine_h