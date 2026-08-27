#include <tempest/window_manager.hpp>

#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/vector.hpp>

#include <atomic>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#elif defined(TEMPEST_PLATFORM_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

namespace tempest
{
    namespace
    {
        std::atomic<uint32_t> g_glfw_ref_count{0};

        void init_glfw()
        {
            if (g_glfw_ref_count.fetch_add(1) == 0)
            {
                glfwInit();
            }
        }

        void shutdown_glfw()
        {
            if (g_glfw_ref_count.fetch_sub(1) == 1)
            {
                glfwTerminate();
            }
        }

        auto translate_glfw_key(int key) noexcept -> core::key
        {
            switch (key)
            {
            case GLFW_KEY_SPACE: return core::key::space;
            case GLFW_KEY_APOSTROPHE: return core::key::apostrophe;
            case GLFW_KEY_COMMA: return core::key::comma;
            case GLFW_KEY_MINUS: return core::key::minus;
            case GLFW_KEY_PERIOD: return core::key::period;
            case GLFW_KEY_SLASH: return core::key::slash;
            case GLFW_KEY_0: return core::key::tw_0;
            case GLFW_KEY_1: return core::key::tw_1;
            case GLFW_KEY_2: return core::key::tw_2;
            case GLFW_KEY_3: return core::key::tw_3;
            case GLFW_KEY_4: return core::key::tw_4;
            case GLFW_KEY_5: return core::key::tw_5;
            case GLFW_KEY_6: return core::key::tw_6;
            case GLFW_KEY_7: return core::key::tw_7;
            case GLFW_KEY_8: return core::key::tw_8;
            case GLFW_KEY_9: return core::key::tw_9;
            case GLFW_KEY_SEMICOLON: return core::key::semicolon;
            case GLFW_KEY_EQUAL: return core::key::equal;
            case GLFW_KEY_A: return core::key::a;
            case GLFW_KEY_B: return core::key::b;
            case GLFW_KEY_C: return core::key::c;
            case GLFW_KEY_D: return core::key::d;
            case GLFW_KEY_E: return core::key::e;
            case GLFW_KEY_F: return core::key::f;
            case GLFW_KEY_G: return core::key::g;
            case GLFW_KEY_H: return core::key::h;
            case GLFW_KEY_I: return core::key::i;
            case GLFW_KEY_J: return core::key::j;
            case GLFW_KEY_K: return core::key::k;
            case GLFW_KEY_L: return core::key::l;
            case GLFW_KEY_M: return core::key::m;
            case GLFW_KEY_N: return core::key::n;
            case GLFW_KEY_O: return core::key::o;
            case GLFW_KEY_P: return core::key::p;
            case GLFW_KEY_Q: return core::key::q;
            case GLFW_KEY_R: return core::key::r;
            case GLFW_KEY_S: return core::key::s;
            case GLFW_KEY_T: return core::key::t;
            case GLFW_KEY_U: return core::key::u;
            case GLFW_KEY_V: return core::key::v;
            case GLFW_KEY_W: return core::key::w;
            case GLFW_KEY_X: return core::key::x;
            case GLFW_KEY_Y: return core::key::y;
            case GLFW_KEY_Z: return core::key::z;
            case GLFW_KEY_LEFT_BRACKET: return core::key::left_bracket;
            case GLFW_KEY_BACKSLASH: return core::key::backslash;
            case GLFW_KEY_RIGHT_BRACKET: return core::key::right_bracket;
            case GLFW_KEY_GRAVE_ACCENT: return core::key::grave_accent;
            case GLFW_KEY_WORLD_1: return core::key::world_1;
            case GLFW_KEY_WORLD_2: return core::key::world_2;
            case GLFW_KEY_ESCAPE: return core::key::escape;
            case GLFW_KEY_ENTER: return core::key::enter;
            case GLFW_KEY_TAB: return core::key::tab;
            case GLFW_KEY_BACKSPACE: return core::key::backspace;
            case GLFW_KEY_INSERT: return core::key::insert;
            case GLFW_KEY_DELETE: return core::key::deletion;
            case GLFW_KEY_RIGHT: return core::key::dpad_right;
            case GLFW_KEY_LEFT: return core::key::dpad_left;
            case GLFW_KEY_DOWN: return core::key::dpad_down;
            case GLFW_KEY_UP: return core::key::dpad_up;
            case GLFW_KEY_PAGE_UP: return core::key::page_up;
            case GLFW_KEY_PAGE_DOWN: return core::key::page_down;
            case GLFW_KEY_HOME: return core::key::home;
            case GLFW_KEY_END: return core::key::end;
            case GLFW_KEY_CAPS_LOCK: return core::key::caps_lock;
            case GLFW_KEY_SCROLL_LOCK: return core::key::scroll_lock;
            case GLFW_KEY_NUM_LOCK: return core::key::num_lock;
            case GLFW_KEY_PRINT_SCREEN: return core::key::print_screen;
            case GLFW_KEY_PAUSE: return core::key::pause;
            case GLFW_KEY_F1: return core::key::fn_1;
            case GLFW_KEY_F2: return core::key::fn_2;
            case GLFW_KEY_F3: return core::key::fn_3;
            case GLFW_KEY_F4: return core::key::fn_4;
            case GLFW_KEY_F5: return core::key::fn_5;
            case GLFW_KEY_F6: return core::key::fn_6;
            case GLFW_KEY_F7: return core::key::fn_7;
            case GLFW_KEY_F8: return core::key::fn_8;
            case GLFW_KEY_F9: return core::key::fn_9;
            case GLFW_KEY_F10: return core::key::fn_10;
            case GLFW_KEY_F11: return core::key::fn_11;
            case GLFW_KEY_F12: return core::key::fn_12;
            case GLFW_KEY_F13: return core::key::fn_13;
            case GLFW_KEY_F14: return core::key::fn_14;
            case GLFW_KEY_F15: return core::key::fn_15;
            case GLFW_KEY_F16: return core::key::fn_16;
            case GLFW_KEY_F17: return core::key::fn_17;
            case GLFW_KEY_F18: return core::key::fn_18;
            case GLFW_KEY_F19: return core::key::fn_19;
            case GLFW_KEY_F20: return core::key::fn_20;
            case GLFW_KEY_F21: return core::key::fn_21;
            case GLFW_KEY_F22: return core::key::fn_22;
            case GLFW_KEY_F23: return core::key::fn_23;
            case GLFW_KEY_F24: return core::key::fn_24;
            case GLFW_KEY_F25: return core::key::fn_25;
            case GLFW_KEY_KP_0: return core::key::kp_0;
            case GLFW_KEY_KP_1: return core::key::kp_1;
            case GLFW_KEY_KP_2: return core::key::kp_2;
            case GLFW_KEY_KP_3: return core::key::kp_3;
            case GLFW_KEY_KP_4: return core::key::kp_4;
            case GLFW_KEY_KP_5: return core::key::kp_5;
            case GLFW_KEY_KP_6: return core::key::kp_6;
            case GLFW_KEY_KP_7: return core::key::kp_7;
            case GLFW_KEY_KP_8: return core::key::kp_8;
            case GLFW_KEY_KP_9: return core::key::kp_9;
            case GLFW_KEY_KP_DECIMAL: return core::key::kp_decimal;
            case GLFW_KEY_KP_DIVIDE: return core::key::kp_divide;
            case GLFW_KEY_KP_MULTIPLY: return core::key::kp_multiply;
            case GLFW_KEY_KP_SUBTRACT: return core::key::kp_subtract;
            case GLFW_KEY_KP_ADD: return core::key::kp_add;
            case GLFW_KEY_KP_ENTER: return core::key::kp_enter;
            case GLFW_KEY_KP_EQUAL: return core::key::kp_equal;
            case GLFW_KEY_LEFT_SHIFT: return core::key::left_shift;
            case GLFW_KEY_LEFT_CONTROL: return core::key::left_control;
            case GLFW_KEY_LEFT_ALT: return core::key::left_alt;
            case GLFW_KEY_LEFT_SUPER: return core::key::left_super;
            case GLFW_KEY_RIGHT_SHIFT: return core::key::right_shift;
            case GLFW_KEY_RIGHT_CONTROL: return core::key::right_control;
            case GLFW_KEY_RIGHT_ALT: return core::key::right_alt;
            case GLFW_KEY_RIGHT_SUPER: return core::key::right_super;
            case GLFW_KEY_MENU: return core::key::menu;
            default: return core::key::unknown;
            }
        }

        auto translate_glfw_key_action(int action) noexcept -> core::key_action
        {
            switch (action)
            {
            case GLFW_PRESS: return core::key_action::press;
            case GLFW_RELEASE: return core::key_action::release;
            case GLFW_REPEAT: return core::key_action::repeat;
            default: return core::key_action::release;
            }
        }

        auto translate_glfw_modifiers(int mods) noexcept -> enum_mask<core::key_modifier>
        {
            auto res = core::key_modifier::none;
            if ((mods & GLFW_MOD_SHIFT) != 0) res = res | core::key_modifier::shift;
            if ((mods & GLFW_MOD_CONTROL) != 0) res = res | core::key_modifier::control;
            if ((mods & GLFW_MOD_ALT) != 0) res = res | core::key_modifier::alt;
            if ((mods & GLFW_MOD_SUPER) != 0) res = res | core::key_modifier::super;
            if ((mods & GLFW_MOD_CAPS_LOCK) != 0) res = res | core::key_modifier::caps_lock;
            if ((mods & GLFW_MOD_NUM_LOCK) != 0) res = res | core::key_modifier::num_lock;
            return make_enum_mask(res);
        }

        auto translate_glfw_mouse_button(int button) noexcept -> core::mouse_button
        {
            switch (button)
            {
            case GLFW_MOUSE_BUTTON_1: return core::mouse_button::mb_1;
            case GLFW_MOUSE_BUTTON_2: return core::mouse_button::mb_2;
            case GLFW_MOUSE_BUTTON_3: return core::mouse_button::mb_3;
            case GLFW_MOUSE_BUTTON_4: return core::mouse_button::mb_4;
            case GLFW_MOUSE_BUTTON_5: return core::mouse_button::mb_5;
            case GLFW_MOUSE_BUTTON_6: return core::mouse_button::mb_6;
            case GLFW_MOUSE_BUTTON_7: return core::mouse_button::mb_7;
            case GLFW_MOUSE_BUTTON_8: return core::mouse_button::mb_8;
            default: return core::mouse_button::last;
            }
        }

        auto translate_glfw_mouse_action(int action) noexcept -> core::mouse_action
        {
            switch (action)
            {
            case GLFW_PRESS: return core::mouse_action::press;
            case GLFW_RELEASE: return core::mouse_action::release;
            default: return core::mouse_action::release;
            }
        }
    } // namespace

    struct window_data
    {
        window_handle handle{null_window_handle};
        GLFWwindow* window{nullptr};
        uint32_t width{0};
        uint32_t height{0};
        core::keyboard keyboard{};
        core::mouse mouse{};
        cursor_mode current_cursor_mode{cursor_mode::normal};

        function<void(core::key_state)> key_callback;
        function<void(core::mouse_button_state)> mouse_button_callback;
        function<void(float, float)> cursor_pos_callback;
        function<void(float, float)> scroll_callback;
        function<void(uint32_t, uint32_t)> resize_callback;
        function<void()> close_callback;
    };

    struct window_manager::impl
    {
        vector<unique_ptr<window_data>> windows;
        uint32_t next_window_id{1};

        auto find_window(window_handle win) -> window_data*
        {
            if (!win.is_valid()) return nullptr;
            for (auto& data : windows)
            {
                if (data && data->handle == win)
                {
                    return data.get();
                }
            }
            return nullptr;
        }

        auto find_window(window_handle win) const -> const window_data*
        {
            if (!win.is_valid()) return nullptr;
            for (const auto& data : windows)
            {
                if (data && data->handle == win)
                {
                    return data.get();
                }
            }
            return nullptr;
        }
    };

    window_manager::window_manager() : _impl(make_unique<impl>())
    {
        init_glfw();
    }

    window_manager::~window_manager()
    {
        if (_impl)
        {
            for (auto& data : _impl->windows)
            {
                if (data && data->window)
                {
                    glfwSetWindowUserPointer(data->window, nullptr);
                    glfwDestroyWindow(data->window);
                    data->window = nullptr;
                }
            }
            _impl->windows.clear();
        }
        shutdown_glfw();
    }

    window_manager::window_manager(window_manager&&) noexcept = default;
    auto window_manager::operator=(window_manager&&) noexcept -> window_manager& = default;

    auto window_manager::create_window(const window_desc& desc) -> window_handle
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);

        auto* monitor = desc.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
        auto* native_win = glfwCreateWindow(static_cast<int>(desc.width), static_cast<int>(desc.height),
                                            desc.title.c_str(), monitor, nullptr);

        if (native_win == nullptr)
        {
            return null_window_handle;
        }

        auto handle = window_handle{_impl->next_window_id++};
        auto data = make_unique<window_data>();
        data->handle = handle;
        data->window = native_win;
        data->width = desc.width;
        data->height = desc.height;

        glfwSetWindowUserPointer(native_win, data.get());

        // Keyboard callback
        glfwSetKeyCallback(native_win, [](GLFWwindow* win, int key, [[maybe_unused]] int scancode, int action, int mods) {
            auto* win_data = static_cast<window_data*>(glfwGetWindowUserPointer(win));
            if (!win_data) return;

            auto state = core::key_state{
                .k = translate_glfw_key(key),
                .action = translate_glfw_key_action(action),
                .modifiers = translate_glfw_modifiers(mods),
            };

            win_data->keyboard.set(state);
            if (win_data->key_callback)
            {
                win_data->key_callback(state);
            }
        });

        // Mouse button callback
        glfwSetMouseButtonCallback(native_win, [](GLFWwindow* win, int button, int action, [[maybe_unused]] int mods) {
            auto* win_data = static_cast<window_data*>(glfwGetWindowUserPointer(win));
            if (!win_data) return;

            auto state = core::mouse_button_state{
                .button = translate_glfw_mouse_button(button),
                .action = translate_glfw_mouse_action(action),
            };

            win_data->mouse.set(state);
            if (win_data->mouse_button_callback)
            {
                win_data->mouse_button_callback(state);
            }
        });

        // Cursor pos callback
        glfwSetCursorPosCallback(native_win, [](GLFWwindow* win, double xpos, double ypos) {
            auto* win_data = static_cast<window_data*>(glfwGetWindowUserPointer(win));
            if (!win_data) return;

            win_data->mouse.set_position(static_cast<float>(xpos), static_cast<float>(ypos));
            if (win_data->cursor_pos_callback)
            {
                win_data->cursor_pos_callback(static_cast<float>(xpos), static_cast<float>(ypos));
            }
        });

        // Scroll callback
        glfwSetScrollCallback(native_win, [](GLFWwindow* win, double xoffset, double yoffset) {
            auto* win_data = static_cast<window_data*>(glfwGetWindowUserPointer(win));
            if (!win_data) return;

            win_data->mouse.set_scroll(static_cast<float>(xoffset), static_cast<float>(yoffset));
            if (win_data->scroll_callback)
            {
                win_data->scroll_callback(static_cast<float>(xoffset), static_cast<float>(yoffset));
            }
        });

        // Resize / Framebuffer size callback
        glfwSetFramebufferSizeCallback(native_win, [](GLFWwindow* win, int width, int height) {
            auto* win_data = static_cast<window_data*>(glfwGetWindowUserPointer(win));
            if (!win_data) return;

            win_data->width = static_cast<uint32_t>(width);
            win_data->height = static_cast<uint32_t>(height);
            if (win_data->resize_callback)
            {
                win_data->resize_callback(win_data->width, win_data->height);
            }
        });

        // Close callback
        glfwSetWindowCloseCallback(native_win, [](GLFWwindow* win) {
            auto* win_data = static_cast<window_data*>(glfwGetWindowUserPointer(win));
            if (!win_data) return;

            if (win_data->close_callback)
            {
                win_data->close_callback();
            }
        });

        _impl->windows.push_back(tempest::move(data));
        return handle;
    }

    auto window_manager::destroy_window(window_handle win) -> void
    {
        if (!win.is_valid() || !_impl) return;

        for (auto it = _impl->windows.begin(); it != _impl->windows.end(); ++it)
        {
            if ((*it)->handle == win)
            {
                if ((*it)->window)
                {
                    glfwSetWindowUserPointer((*it)->window, nullptr);
                    glfwDestroyWindow((*it)->window);
                    (*it)->window = nullptr;
                }
                _impl->windows.erase(it);
                break;
            }
        }
    }

    auto window_manager::get_native_wsi_handle(window_handle win) const -> rhi::native_wsi_handle
    {
        auto* data = _impl->find_window(win);
        if (!data || !data->window)
        {
            return {};
        }

#if defined(TEMPEST_PLATFORM_WINDOWS)
        return rhi::native_wsi_handle{
            .display = static_cast<void*>(GetModuleHandle(nullptr)),
            .window = static_cast<void*>(glfwGetWin32Window(data->window)),
        };
#elif defined(TEMPEST_PLATFORM_LINUX)
        return rhi::native_wsi_handle{
            .display = static_cast<void*>(glfwGetX11Display()),
            .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(data->window))),
        };
#else
        return {};
#endif
    }

    auto window_manager::get_width(window_handle win) const -> uint32_t
    {
        auto* data = _impl->find_window(win);
        if (!data || !data->window) return 0;
        auto w = 0;
        auto h = 0;
        glfwGetWindowSize(data->window, &w, &h);
        return static_cast<uint32_t>(w);
    }

    auto window_manager::get_height(window_handle win) const -> uint32_t
    {
        auto* data = _impl->find_window(win);
        if (!data || !data->window) return 0;
        auto w = 0;
        auto h = 0;
        glfwGetWindowSize(data->window, &w, &h);
        return static_cast<uint32_t>(h);
    }

    auto window_manager::get_framebuffer_width(window_handle win) const -> uint32_t
    {
        auto* data = _impl->find_window(win);
        if (!data || !data->window) return 0;
        auto w = 0;
        auto h = 0;
        glfwGetFramebufferSize(data->window, &w, &h);
        return static_cast<uint32_t>(w);
    }

    auto window_manager::get_framebuffer_height(window_handle win) const -> uint32_t
    {
        auto* data = _impl->find_window(win);
        if (!data || !data->window) return 0;
        auto w = 0;
        auto h = 0;
        glfwGetFramebufferSize(data->window, &w, &h);
        return static_cast<uint32_t>(h);
    }

    auto window_manager::should_close(window_handle win) const -> bool
    {
        auto* data = _impl->find_window(win);
        if (!data || !data->window) return true;
        return glfwWindowShouldClose(data->window) == GLFW_TRUE;
    }

    auto window_manager::set_should_close(window_handle win, bool close) -> void
    {
        auto* data = _impl->find_window(win);
        if (data && data->window)
        {
            glfwSetWindowShouldClose(data->window, close ? GLFW_TRUE : GLFW_FALSE);
        }
    }

    auto window_manager::poll_events() -> void
    {
        glfwPollEvents();
    }

    auto window_manager::set_cursor_mode(window_handle win, cursor_mode mode) -> void
    {
        auto* data = _impl->find_window(win);
        if (!data || !data->window) return;

        data->current_cursor_mode = mode;
        switch (mode)
        {
        case cursor_mode::normal:
            glfwSetInputMode(data->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            data->mouse.set_disabled(false);
            break;
        case cursor_mode::hidden:
            glfwSetInputMode(data->window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            data->mouse.set_disabled(false);
            break;
        case cursor_mode::disabled:
            glfwSetInputMode(data->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            data->mouse.set_disabled(true);
            break;
        }
    }

    auto window_manager::get_cursor_mode(window_handle win) const -> cursor_mode
    {
        auto* data = _impl->find_window(win);
        return data ? data->current_cursor_mode : cursor_mode::normal;
    }

    auto window_manager::is_cursor_disabled(window_handle win) const -> bool
    {
        auto* data = _impl->find_window(win);
        return data ? (data->current_cursor_mode == cursor_mode::disabled) : false;
    }

    auto window_manager::get_keyboard(window_handle win) -> core::keyboard&
    {
        auto* data = _impl->find_window(win);
        if (!data)
        {
            static auto dummy_keyboard = core::keyboard{};
            return dummy_keyboard;
        }
        return data->keyboard;
    }

    auto window_manager::get_keyboard(window_handle win) const -> const core::keyboard&
    {
        auto* data = _impl->find_window(win);
        if (!data)
        {
            static const auto dummy_keyboard = core::keyboard{};
            return dummy_keyboard;
        }
        return data->keyboard;
    }

    auto window_manager::get_mouse(window_handle win) -> core::mouse&
    {
        auto* data = _impl->find_window(win);
        if (!data)
        {
            static auto dummy_mouse = core::mouse{};
            return dummy_mouse;
        }
        return data->mouse;
    }

    auto window_manager::get_mouse(window_handle win) const -> const core::mouse&
    {
        auto* data = _impl->find_window(win);
        if (!data)
        {
            static const auto dummy_mouse = core::mouse{};
            return dummy_mouse;
        }
        return data->mouse;
    }

    auto window_manager::get_input_group(window_handle win) -> core::input_group
    {
        auto* data = _impl->find_window(win);
        if (!data) return {};
        return core::input_group{
            .kb = &data->keyboard,
            .ms = &data->mouse,
        };
    }

    auto window_manager::register_key_callback(window_handle win, function<void(core::key_state)> cb) -> void
    {
        auto* data = _impl->find_window(win);
        if (data)
        {
            data->key_callback = tempest::move(cb);
        }
    }

    auto window_manager::register_mouse_button_callback(window_handle win, function<void(core::mouse_button_state)> cb)
        -> void
    {
        auto* data = _impl->find_window(win);
        if (data)
        {
            data->mouse_button_callback = tempest::move(cb);
        }
    }

    auto window_manager::register_cursor_pos_callback(window_handle win, function<void(float, float)> cb) -> void
    {
        auto* data = _impl->find_window(win);
        if (data)
        {
            data->cursor_pos_callback = tempest::move(cb);
        }
    }

    auto window_manager::register_scroll_callback(window_handle win, function<void(float, float)> cb) -> void
    {
        auto* data = _impl->find_window(win);
        if (data)
        {
            data->scroll_callback = tempest::move(cb);
        }
    }

    auto window_manager::register_resize_callback(window_handle win, function<void(uint32_t, uint32_t)> cb) -> void
    {
        auto* data = _impl->find_window(win);
        if (data)
        {
            data->resize_callback = tempest::move(cb);
        }
    }

    auto window_manager::register_close_callback(window_handle win, function<void()> cb) -> void
    {
        auto* data = _impl->find_window(win);
        if (data)
        {
            data->close_callback = tempest::move(cb);
        }
    }
} // namespace tempest
