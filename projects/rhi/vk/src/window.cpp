#include <tempest/vk/rhi.hpp>

#include "window.hpp"

#include <tempest/logger.hpp>

#include <GLFW/glfw3.h>

namespace tempest::rhi::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::vk::window_surface"}});

        static consteval array<core::key, GLFW_KEY_LAST + 1> build_key_map()
        {
            array<core::key, GLFW_KEY_LAST + 1> keys;
            keys.fill(core::key::unknown);

            keys[GLFW_KEY_SPACE] = core::key::space;
            keys[GLFW_KEY_APOSTROPHE] = core::key::apostrophe;
            keys[GLFW_KEY_COMMA] = core::key::comma;
            keys[GLFW_KEY_MINUS] = core::key::minus;
            keys[GLFW_KEY_PERIOD] = core::key::period;
            keys[GLFW_KEY_SLASH] = core::key::slash;
            keys[GLFW_KEY_0] = core::key::tw_0;
            keys[GLFW_KEY_1] = core::key::tw_1;
            keys[GLFW_KEY_2] = core::key::tw_2;
            keys[GLFW_KEY_3] = core::key::tw_3;
            keys[GLFW_KEY_4] = core::key::tw_4;
            keys[GLFW_KEY_5] = core::key::tw_5;
            keys[GLFW_KEY_6] = core::key::tw_6;
            keys[GLFW_KEY_7] = core::key::tw_7;
            keys[GLFW_KEY_8] = core::key::tw_8;
            keys[GLFW_KEY_9] = core::key::tw_9;
            keys[GLFW_KEY_SEMICOLON] = core::key::semicolon;
            keys[GLFW_KEY_EQUAL] = core::key::equal;
            keys[GLFW_KEY_A] = core::key::a;
            keys[GLFW_KEY_B] = core::key::b;
            keys[GLFW_KEY_C] = core::key::c;
            keys[GLFW_KEY_D] = core::key::d;
            keys[GLFW_KEY_E] = core::key::e;
            keys[GLFW_KEY_F] = core::key::f;
            keys[GLFW_KEY_G] = core::key::g;
            keys[GLFW_KEY_H] = core::key::h;
            keys[GLFW_KEY_I] = core::key::i;
            keys[GLFW_KEY_J] = core::key::j;
            keys[GLFW_KEY_K] = core::key::k;
            keys[GLFW_KEY_L] = core::key::l;
            keys[GLFW_KEY_M] = core::key::m;
            keys[GLFW_KEY_N] = core::key::n;
            keys[GLFW_KEY_O] = core::key::o;
            keys[GLFW_KEY_P] = core::key::p;
            keys[GLFW_KEY_Q] = core::key::q;
            keys[GLFW_KEY_R] = core::key::r;
            keys[GLFW_KEY_S] = core::key::s;
            keys[GLFW_KEY_T] = core::key::t;
            keys[GLFW_KEY_U] = core::key::u;
            keys[GLFW_KEY_V] = core::key::v;
            keys[GLFW_KEY_W] = core::key::w;
            keys[GLFW_KEY_X] = core::key::x;
            keys[GLFW_KEY_Y] = core::key::y;
            keys[GLFW_KEY_Z] = core::key::z;
            keys[GLFW_KEY_LEFT_BRACKET] = core::key::left_bracket;
            keys[GLFW_KEY_BACKSLASH] = core::key::backslash;
            keys[GLFW_KEY_RIGHT_BRACKET] = core::key::right_bracket;
            keys[GLFW_KEY_GRAVE_ACCENT] = core::key::grave_accent;
            keys[GLFW_KEY_WORLD_1] = core::key::world_1;
            keys[GLFW_KEY_WORLD_2] = core::key::world_2;
            keys[GLFW_KEY_ESCAPE] = core::key::escape;
            keys[GLFW_KEY_ENTER] = core::key::enter;
            keys[GLFW_KEY_TAB] = core::key::tab;
            keys[GLFW_KEY_BACKSPACE] = core::key::backspace;
            keys[GLFW_KEY_INSERT] = core::key::insert;
            keys[GLFW_KEY_DELETE] = core::key::deletion;
            keys[GLFW_KEY_RIGHT] = core::key::dpad_right;
            keys[GLFW_KEY_LEFT] = core::key::dpad_left;
            keys[GLFW_KEY_DOWN] = core::key::dpad_down;
            keys[GLFW_KEY_UP] = core::key::dpad_up;
            keys[GLFW_KEY_PAGE_UP] = core::key::page_up;
            keys[GLFW_KEY_PAGE_DOWN] = core::key::page_down;
            keys[GLFW_KEY_HOME] = core::key::home;
            keys[GLFW_KEY_END] = core::key::end;
            keys[GLFW_KEY_CAPS_LOCK] = core::key::caps_lock;
            keys[GLFW_KEY_NUM_LOCK] = core::key::num_lock;
            keys[GLFW_KEY_PRINT_SCREEN] = core::key::print_screen;
            keys[GLFW_KEY_PAUSE] = core::key::pause;
            keys[GLFW_KEY_F1] = core::key::fn_1;
            keys[GLFW_KEY_F2] = core::key::fn_2;
            keys[GLFW_KEY_F3] = core::key::fn_3;
            keys[GLFW_KEY_F4] = core::key::fn_4;
            keys[GLFW_KEY_F5] = core::key::fn_5;
            keys[GLFW_KEY_F6] = core::key::fn_6;
            keys[GLFW_KEY_F7] = core::key::fn_7;
            keys[GLFW_KEY_F8] = core::key::fn_8;
            keys[GLFW_KEY_F9] = core::key::fn_9;
            keys[GLFW_KEY_F10] = core::key::fn_10;
            keys[GLFW_KEY_F11] = core::key::fn_11;
            keys[GLFW_KEY_F12] = core::key::fn_12;
            keys[GLFW_KEY_F13] = core::key::fn_13;
            keys[GLFW_KEY_F14] = core::key::fn_14;
            keys[GLFW_KEY_F15] = core::key::fn_15;
            keys[GLFW_KEY_F16] = core::key::fn_16;
            keys[GLFW_KEY_F17] = core::key::fn_17;
            keys[GLFW_KEY_F18] = core::key::fn_18;
            keys[GLFW_KEY_F19] = core::key::fn_19;
            keys[GLFW_KEY_F20] = core::key::fn_20;
            keys[GLFW_KEY_F21] = core::key::fn_21;
            keys[GLFW_KEY_F22] = core::key::fn_22;
            keys[GLFW_KEY_F23] = core::key::fn_23;
            keys[GLFW_KEY_F24] = core::key::fn_24;
            keys[GLFW_KEY_F25] = core::key::fn_25;
            keys[GLFW_KEY_KP_0] = core::key::kp_0;
            keys[GLFW_KEY_KP_1] = core::key::kp_1;
            keys[GLFW_KEY_KP_2] = core::key::kp_2;
            keys[GLFW_KEY_KP_3] = core::key::kp_3;
            keys[GLFW_KEY_KP_4] = core::key::kp_4;
            keys[GLFW_KEY_KP_5] = core::key::kp_5;
            keys[GLFW_KEY_KP_6] = core::key::kp_6;
            keys[GLFW_KEY_KP_7] = core::key::kp_7;
            keys[GLFW_KEY_KP_8] = core::key::kp_8;
            keys[GLFW_KEY_KP_9] = core::key::kp_9;
            keys[GLFW_KEY_KP_DECIMAL] = core::key::kp_decimal;
            keys[GLFW_KEY_KP_DIVIDE] = core::key::kp_divide;
            keys[GLFW_KEY_KP_MULTIPLY] = core::key::kp_multiply;
            keys[GLFW_KEY_KP_SUBTRACT] = core::key::kp_subtract;
            keys[GLFW_KEY_KP_ADD] = core::key::kp_add;
            keys[GLFW_KEY_KP_ENTER] = core::key::kp_enter;
            keys[GLFW_KEY_LEFT_SHIFT] = core::key::left_shift;
            keys[GLFW_KEY_LEFT_CONTROL] = core::key::left_control;
            keys[GLFW_KEY_LEFT_ALT] = core::key::left_alt;
            keys[GLFW_KEY_LEFT_SUPER] = core::key::left_super;
            keys[GLFW_KEY_RIGHT_SHIFT] = core::key::right_shift;
            keys[GLFW_KEY_RIGHT_CONTROL] = core::key::right_control;
            keys[GLFW_KEY_RIGHT_ALT] = core::key::right_alt;
            keys[GLFW_KEY_RIGHT_SUPER] = core::key::right_super;
            keys[GLFW_KEY_MENU] = core::key::menu;

            return keys;
        }

        static consteval array<core::key_action, GLFW_REPEAT + 1> build_key_action_map()
        {
            array<core::key_action, GLFW_REPEAT + 1> actions;
            actions.fill(core::key_action::release);
            actions[GLFW_RELEASE] = core::key_action::release;
            actions[GLFW_PRESS] = core::key_action::press;
            actions[GLFW_REPEAT] = core::key_action::repeat;

            return actions;
        }

        static consteval array<core::mouse_button, GLFW_MOUSE_BUTTON_LAST + 1> build_mouse_button_map()
        {
            array<core::mouse_button, GLFW_MOUSE_BUTTON_LAST + 1> buttons;
            buttons[GLFW_MOUSE_BUTTON_1] = core::mouse_button::mb_1;
            buttons[GLFW_MOUSE_BUTTON_2] = core::mouse_button::mb_2;
            buttons[GLFW_MOUSE_BUTTON_3] = core::mouse_button::mb_3;
            buttons[GLFW_MOUSE_BUTTON_4] = core::mouse_button::mb_4;
            buttons[GLFW_MOUSE_BUTTON_5] = core::mouse_button::mb_5;
            buttons[GLFW_MOUSE_BUTTON_6] = core::mouse_button::mb_6;
            buttons[GLFW_MOUSE_BUTTON_7] = core::mouse_button::mb_7;
            buttons[GLFW_MOUSE_BUTTON_8] = core::mouse_button::mb_8;
            return buttons;
        }

        static consteval array<core::mouse_action, GLFW_REPEAT + 1> build_mouse_action_map()
        {
            array<core::mouse_action, GLFW_REPEAT + 1> actions;
            actions.fill(core::mouse_action::release);
            actions[GLFW_RELEASE] = core::mouse_action::release;
            actions[GLFW_PRESS] = core::mouse_action::press;

            return actions;
        }

        static constexpr auto glfw_to_tempest_keys = build_key_map();
        static constexpr auto glfw_to_tempest_key_actions = build_key_action_map();
        static constexpr auto glfw_to_tempest_mouse_buttons = build_mouse_button_map();
        static constexpr auto glfw_to_tempest_mouse_actions = build_mouse_action_map();
    } // namespace

    window_surface::window_surface(GLFWwindow* win, string name, uint32_t width, uint32_t height) noexcept
        : _window(win), _name(tempest::move(name)), _width(width), _height(height), _cursors{}
    {
        std::fill(_cursors.begin(), _cursors.end(), nullptr);

        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(_window, &fb_width, &fb_height);
        _framebuffer_width = static_cast<uint32_t>(fb_width);
        _framebuffer_height = static_cast<uint32_t>(fb_height);
    }

    window_surface::~window_surface()
    {
        if (_window)
        {
            for (auto& cursor : _cursors)
            {
                if (cursor != nullptr)
                {
                    glfwDestroyCursor(cursor);
                    cursor = nullptr;
                }
            }

            glfwDestroyWindow(_window);
        }
    }

    void window_surface::register_keyboard_callback(function<void(const core::key_state&)>&& cb) noexcept
    {
        _keyboard_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_mouse_callback(function<void(const core::mouse_button_state&)>&& cb) noexcept
    {
        _mouse_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_cursor_callback(function<void(float, float)>&& cb) noexcept
    {
        _cursor_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_scroll_callback(function<void(float, float)>&& cb) noexcept
    {
        _scroll_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_character_input_callback(function<void(uint32_t)>&& cb) noexcept
    {
        _character_input_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_close_callback(function<void()>&& cb) noexcept
    {
        _close_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_resize_callback(function<void(uint32_t, uint32_t)>&& cb) noexcept
    {
        _resize_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_focus_callback(function<void(bool)>&& cb) noexcept
    {
        _focus_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_minimize_callback(function<void(bool)>&& cb) noexcept
    {
        _minimize_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_cursor_enter_callback(function<void(bool)>&& cb) noexcept
    {
        _cursor_enter_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::set_cursor_shape(cursor_shape shape) noexcept
    {
        auto& cursor = _cursors[static_cast<uint32_t>(shape)];
        if (cursor == nullptr)
        {
            switch (shape)
            {
            case cursor_shape::arrow:
                cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
                break;
            case cursor_shape::ibeam:
                cursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
                break;
            case cursor_shape::crosshair:
                cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
                break;
            case cursor_shape::hand:
                cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
                break;
            case cursor_shape::resize_horizontal:
                cursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
                break;
            case cursor_shape::resize_vertical:
                cursor = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
                break;
            }
        }

        if (cursor != nullptr)
        {
            glfwSetCursor(_window, cursor);
        }
        else
        {
            logger->error("Failed to create cursor for shape: {}", static_cast<uint32_t>(shape));
        }
    }

    vector<rhi::window_surface::monitor> window_surface::get_monitors() const noexcept
    {
        vector<rhi::window_surface::monitor> monitors;

        int count = 0;
        GLFWmonitor** glfw_monitors = glfwGetMonitors(&count);
        monitors.reserve(static_cast<size_t>(count));

        for (int i = 0; i < count; ++i)
        {
            auto glfw_monitor = glfw_monitors[i];
            auto current_mode = glfwGetVideoMode(glfw_monitor);

            int xpos, ypos, width, height;
            glfwGetMonitorWorkarea(glfw_monitor, &xpos, &ypos, &width, &height);

            int x, y;
            glfwGetMonitorPos(glfw_monitor, &x, &y);

            float x_scale, y_scale;
            glfwGetMonitorContentScale(glfw_monitor, &x_scale, &y_scale);

            rhi::window_surface::monitor monitor{
                .work_x = xpos,
                .work_y = ypos,
                .work_width = static_cast<uint32_t>(width),
                .work_height = static_cast<uint32_t>(height),
                .x = x,
                .y = y,
                .content_scale_x = x_scale,
                .content_scale_y = y_scale,
                .name = glfwGetMonitorName(glfw_monitor),
                .current_video_mode =
                    {
                        .width = static_cast<uint32_t>(current_mode->width),
                        .height = static_cast<uint32_t>(current_mode->height),
                        .refresh_rate = static_cast<uint32_t>(current_mode->refreshRate),
                        .red_bits = static_cast<uint8_t>(current_mode->redBits),
                        .green_bits = static_cast<uint8_t>(current_mode->greenBits),
                        .blue_bits = static_cast<uint8_t>(current_mode->blueBits),
                    },
            };

            monitors.push_back(tempest::move(monitor));
        }

        return monitors;
    }

    void window_surface::execute_keyboard_callbacks(const core::key_state& state) const noexcept
    {
        for (const auto& cb : _keyboard_callbacks)
        {
            cb(state);
        }
    }

    void window_surface::execute_mouse_callbacks(const core::mouse_button_state& state) const noexcept
    {
        for (const auto& cb : _mouse_callbacks)
        {
            cb(state);
        }
    }

    void window_surface::execute_cursor_callbacks(float x, float y) const noexcept
    {
        for (const auto& cb : _cursor_callbacks)
        {
            cb(x, y);
        }
    }

    void window_surface::execute_scroll_callbacks(float x, float y) const noexcept
    {
        for (const auto& cb : _scroll_callbacks)
        {
            cb(x, y);
        }
    }

    void window_surface::execute_character_input_callbacks(uint32_t codepoint) const noexcept
    {
        for (const auto& cb : _character_input_callbacks)
        {
            cb(codepoint);
        }
    }

    void window_surface::execute_close_callbacks() const noexcept
    {
        for (const auto& cb : _close_callbacks)
        {
            cb();
        }
    }

    void window_surface::execute_resize_callbacks(uint32_t width, uint32_t height) const noexcept
    {
        for (const auto& cb : _resize_callbacks)
        {
            cb(width, height);
        }
    }

    void window_surface::execute_focus_callbacks(bool focused) const noexcept
    {
        for (const auto& cb : _focus_callbacks)
        {
            cb(focused);
        }
    }

    void window_surface::execute_minimize_callbacks(bool minimized) const noexcept
    {
        for (const auto& cb : _minimize_callbacks)
        {
            cb(minimized);
        }
    }

    void window_surface::execute_cursor_enter_callbacks(bool entered) const noexcept
    {
        for (const auto& cb : _cursor_enter_callbacks)
        {
            cb(entered);
        }
    }

    unique_ptr<rhi::window_surface> create_window_surface(const rhi::window_surface_desc& desc) noexcept
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        auto window = glfwCreateWindow(desc.width, desc.height, desc.name.c_str(),
                                       desc.fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
        if (window == nullptr)
        {
            logger->error("Failed to create GLFW window: {}", desc.name.c_str());
            return nullptr;
        }

        auto win = make_unique<window_surface>(window, desc.name, desc.width, desc.height);

        glfwSetWindowUserPointer(window, win.get());

        // Keyboard
        glfwSetKeyCallback(window,
                           [](GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, int mods) {
                               auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
                               auto key_state = core::key_state{
                                   .k = glfw_to_tempest_keys[key],
                                   .action = glfw_to_tempest_key_actions[action],
                                   .modifiers = make_enum_mask(static_cast<core::key_modifier>(mods)),
                               };

                               win->execute_keyboard_callbacks(key_state);
                           });

        // Mouse
        glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, [[maybe_unused]] int mods) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            auto mouse_state = core::mouse_button_state{
                .button = glfw_to_tempest_mouse_buttons[button],
                .action = glfw_to_tempest_mouse_actions[action],
            };

            win->execute_mouse_callbacks(mouse_state);
        });

        // Cursor
        glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_cursor_callbacks(static_cast<float>(xpos), static_cast<float>(ypos));
        });

        // Scroll
        glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_scroll_callbacks(static_cast<float>(xoffset), static_cast<float>(yoffset));
        });

        // Character input
        glfwSetCharCallback(window, [](GLFWwindow* window, unsigned int codepoint) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_character_input_callbacks(codepoint);
        });

        // Close
        glfwSetWindowCloseCallback(window, [](GLFWwindow* window) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_close_callbacks();
        });

        // Resize
        glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_resize_callbacks(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
            win->set_size(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        });

        // Framebuffer resize
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->set_framebuffer_size(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        });

        // Focus
        glfwSetWindowFocusCallback(window, [](GLFWwindow* window, int focused) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_focus_callbacks(focused == GLFW_TRUE);
        });

        // Minimize
        glfwSetWindowIconifyCallback(window, [](GLFWwindow* window, int minimized) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_minimize_callbacks(minimized == GLFW_TRUE);
            win->set_minimized(minimized == GLFW_TRUE);
        });

        // Cursor enter
        glfwSetCursorEnterCallback(window, [](GLFWwindow* window, int entered) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_cursor_enter_callbacks(entered != GLFW_FALSE);
        });

        return win;
    }
} // namespace tempest::rhi::vk