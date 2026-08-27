#ifndef tempest_window_manager_hpp
#define tempest_window_manager_hpp

#include <tempest/api.hpp>
#include <tempest/functional.hpp>
#include <tempest/input.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/rhi.hpp>
#include <tempest/string.hpp>

namespace tempest
{
    /// \brief Strongly-typed handle representing an OS window managed by window_manager.
    struct window_handle
    {
        uint32_t id{0};

        [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
        {
            return id != 0;
        }

        constexpr auto operator<=>(const window_handle&) const noexcept = default;
    };

    inline constexpr window_handle null_window_handle{0};

    /// \brief Cursor modes supported by window_manager.
    enum class cursor_mode : uint8_t
    {
        normal,
        hidden,
        disabled,
    };

    /// \brief Description for creating an OS window.
    struct window_desc
    {
        uint32_t width{1280};
        uint32_t height{720};
        string title{"Tempest Engine"};
        bool fullscreen{false};
        bool resizable{true};
    };

    /// \brief Manages OS windows, GLFW encapsulation, event dispatching, and WSI handle generation.
    class TEMPEST_API window_manager
    {
      public:
        window_manager();
        ~window_manager();

        window_manager(const window_manager&) = delete;
        auto operator=(const window_manager&) -> window_manager& = delete;
        window_manager(window_manager&&) noexcept;
        auto operator=(window_manager&&) noexcept -> window_manager&;

        [[nodiscard]] auto create_window(const window_desc& desc) -> window_handle;
        auto destroy_window(window_handle win) -> void;

        [[nodiscard]] auto get_native_wsi_handle(window_handle win) const -> rhi::native_wsi_handle;
        [[nodiscard]] auto get_width(window_handle win) const -> uint32_t;
        [[nodiscard]] auto get_height(window_handle win) const -> uint32_t;
        [[nodiscard]] auto get_framebuffer_width(window_handle win) const -> uint32_t;
        [[nodiscard]] auto get_framebuffer_height(window_handle win) const -> uint32_t;

        [[nodiscard]] auto should_close(window_handle win) const -> bool;
        auto set_should_close(window_handle win, bool close) -> void;

        auto poll_events() -> void;

        auto set_cursor_mode(window_handle win, cursor_mode mode) -> void;
        [[nodiscard]] auto get_cursor_mode(window_handle win) const -> cursor_mode;
        [[nodiscard]] auto is_cursor_disabled(window_handle win) const -> bool;

        [[nodiscard]] auto get_keyboard(window_handle win) -> core::keyboard&;
        [[nodiscard]] auto get_keyboard(window_handle win) const -> const core::keyboard&;
        [[nodiscard]] auto get_mouse(window_handle win) -> core::mouse&;
        [[nodiscard]] auto get_mouse(window_handle win) const -> const core::mouse&;
        [[nodiscard]] auto get_input_group(window_handle win) -> core::input_group;

        auto register_key_callback(window_handle win, function<void(core::key_state)> cb) -> void;
        auto register_mouse_button_callback(window_handle win, function<void(core::mouse_button_state)> cb) -> void;
        auto register_cursor_pos_callback(window_handle win, function<void(float, float)> cb) -> void;
        auto register_scroll_callback(window_handle win, function<void(float, float)> cb) -> void;
        auto register_resize_callback(window_handle win, function<void(uint32_t, uint32_t)> cb) -> void;
        auto register_close_callback(window_handle win, function<void()> cb) -> void;

      private:
        struct impl;
        unique_ptr<impl> _impl;
    };
} // namespace tempest

#endif // tempest_window_manager_hpp
