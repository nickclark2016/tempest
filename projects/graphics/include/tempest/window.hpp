#ifndef tempest_window_hpp
#define tempest_window_hpp

#include <tempest/keyboard.hpp>
#include <tempest/mouse.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace tempest::graphics
{
    class iwindow
    {
      public:
        using close_callback = std::function<void()>;

        virtual ~iwindow() = default;

        virtual bool should_close() const noexcept = 0;
        virtual void close() = 0;

        virtual std::uint32_t width() const noexcept = 0;
        virtual std::uint32_t height() const noexcept = 0;

        virtual bool minimized() const noexcept = 0;

        virtual void register_keyboard_callback(std::function<void(const core::key_state&)>&& cb) = 0;
        virtual void register_mouse_callback(std::function<void(const core::mouse_button_state&)>&& cb) = 0;
        virtual void register_cursor_callback(std::function<void(float, float)> cb) = 0;

        virtual void show() = 0;
        virtual void disable_cursor(bool disable = true) = 0;
        virtual bool is_cursor_disabled() const = 0;
    };
    
    class window_factory
    {
      public:
        struct create_info
        {
            std::string_view title;
            std::uint32_t width;
            std::uint32_t height;
        };

        static std::unique_ptr<iwindow> create(const create_info& info);
    };
} // namespace tempest::graphics

#endif // tempest_window_hpp
