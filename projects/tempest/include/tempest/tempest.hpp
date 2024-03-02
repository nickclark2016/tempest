#ifndef tempest_tempest_engine_h
#define tempest_tempest_engine_h

#include <tempest/registry.hpp>
#include <tempest/render_system.hpp>
#include <tempest/window.hpp>

#include <chrono>
#include <unordered_map>

namespace tempest
{
    class engine
    {
        engine();
      public:
        static engine initialize();

        void add_window(std::unique_ptr<graphics::iwindow> window);
        void update();
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

        [[noreturn]] void run();

      private:
        ecs::registry _entity_registry;
        std::vector<std::unique_ptr<graphics::iwindow>> _windows;

        std::chrono::steady_clock::time_point _last_frame_time;
        std::chrono::duration<float> _delta_time;

        graphics::render_system _render_system;

        bool _should_close = false;
    };
} // namespace tempest

#endif // tempest_tempest_engine_h