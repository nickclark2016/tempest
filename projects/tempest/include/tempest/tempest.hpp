#ifndef tempest_tempest_engine_h
#define tempest_tempest_engine_h

#include <tempest/input.hpp>
#include <tempest/registry.hpp>
#include <tempest/render_system.hpp>
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

        void on_initialize(std::function<void(engine&)>&& callback)
        {
            _initialize_callbacks.push_back(std::move(callback));
        }

        void on_close(std::function<void(engine&)>&& callback)
        {
            _close_callbacks.push_back(std::move(callback));
        }

        void on_update(std::function<void(engine&, float)>&& callback)
        {
            _update_callbacks.push_back(std::move(callback));
        }

        ecs::entity load_asset(std::string_view path);

        [[noreturn]] void run();

      private:
        ecs::registry _entity_registry;
        std::vector<window_payload> _windows;

        std::vector<std::function<void(engine&)>> _initialize_callbacks;
        std::vector<std::function<void(engine&)>> _close_callbacks;
        std::vector<std::function<void(engine&, float)>> _update_callbacks;

        std::chrono::steady_clock::time_point _last_frame_time;
        std::chrono::duration<float> _delta_time;

        graphics::render_system _render_system;

        bool _should_close = false;
    };
} // namespace tempest

#endif // tempest_tempest_engine_h