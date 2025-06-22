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
            keys.fill(core::key::UNKNOWN);

            keys[GLFW_KEY_SPACE] = core::key::SPACE;
            keys[GLFW_KEY_APOSTROPHE] = core::key::APOSTROPHE;
            keys[GLFW_KEY_COMMA] = core::key::COMMA;
            keys[GLFW_KEY_MINUS] = core::key::MINUS;
            keys[GLFW_KEY_PERIOD] = core::key::PERIOD;
            keys[GLFW_KEY_SLASH] = core::key::SLASH;
            keys[GLFW_KEY_0] = core::key::TW_0;
            keys[GLFW_KEY_1] = core::key::TW_1;
            keys[GLFW_KEY_2] = core::key::TW_2;
            keys[GLFW_KEY_3] = core::key::TW_3;
            keys[GLFW_KEY_4] = core::key::TW_4;
            keys[GLFW_KEY_5] = core::key::TW_5;
            keys[GLFW_KEY_6] = core::key::TW_6;
            keys[GLFW_KEY_7] = core::key::TW_7;
            keys[GLFW_KEY_8] = core::key::TW_8;
            keys[GLFW_KEY_9] = core::key::TW_9;
            keys[GLFW_KEY_SEMICOLON] = core::key::SEMICOLON;
            keys[GLFW_KEY_EQUAL] = core::key::EQUAL;
            keys[GLFW_KEY_A] = core::key::A;
            keys[GLFW_KEY_B] = core::key::B;
            keys[GLFW_KEY_C] = core::key::C;
            keys[GLFW_KEY_D] = core::key::D;
            keys[GLFW_KEY_E] = core::key::E;
            keys[GLFW_KEY_F] = core::key::F;
            keys[GLFW_KEY_G] = core::key::G;
            keys[GLFW_KEY_H] = core::key::H;
            keys[GLFW_KEY_I] = core::key::I;
            keys[GLFW_KEY_J] = core::key::J;
            keys[GLFW_KEY_K] = core::key::K;
            keys[GLFW_KEY_L] = core::key::L;
            keys[GLFW_KEY_M] = core::key::M;
            keys[GLFW_KEY_N] = core::key::N;
            keys[GLFW_KEY_O] = core::key::O;
            keys[GLFW_KEY_P] = core::key::P;
            keys[GLFW_KEY_Q] = core::key::Q;
            keys[GLFW_KEY_R] = core::key::R;
            keys[GLFW_KEY_S] = core::key::S;
            keys[GLFW_KEY_T] = core::key::T;
            keys[GLFW_KEY_U] = core::key::U;
            keys[GLFW_KEY_V] = core::key::V;
            keys[GLFW_KEY_W] = core::key::W;
            keys[GLFW_KEY_X] = core::key::X;
            keys[GLFW_KEY_Y] = core::key::Y;
            keys[GLFW_KEY_Z] = core::key::Z;
            keys[GLFW_KEY_LEFT_BRACKET] = core::key::LEFT_BRACKET;
            keys[GLFW_KEY_BACKSLASH] = core::key::BACKSLASH;
            keys[GLFW_KEY_RIGHT_BRACKET] = core::key::RIGHT_BRACKET;
            keys[GLFW_KEY_GRAVE_ACCENT] = core::key::GRAVE_ACCENT;
            keys[GLFW_KEY_WORLD_1] = core::key::WORLD_1;
            keys[GLFW_KEY_WORLD_2] = core::key::WORLD_2;
            keys[GLFW_KEY_ESCAPE] = core::key::ESCAPE;
            keys[GLFW_KEY_ENTER] = core::key::ENTER;
            keys[GLFW_KEY_TAB] = core::key::TAB;
            keys[GLFW_KEY_BACKSPACE] = core::key::BACKSPACE;
            keys[GLFW_KEY_INSERT] = core::key::INSERT;
            keys[GLFW_KEY_DELETE] = core::key::DELETE;
            keys[GLFW_KEY_RIGHT] = core::key::DPAD_RIGHT;
            keys[GLFW_KEY_LEFT] = core::key::DPAD_LEFT;
            keys[GLFW_KEY_DOWN] = core::key::DPAD_DOWN;
            keys[GLFW_KEY_UP] = core::key::DPAD_UP;
            keys[GLFW_KEY_PAGE_UP] = core::key::PAGE_UP;
            keys[GLFW_KEY_PAGE_DOWN] = core::key::PAGE_DOWN;
            keys[GLFW_KEY_HOME] = core::key::HOME;
            keys[GLFW_KEY_END] = core::key::END;
            keys[GLFW_KEY_CAPS_LOCK] = core::key::CAPS_LOCK;
            keys[GLFW_KEY_NUM_LOCK] = core::key::NUM_LOCK;
            keys[GLFW_KEY_PRINT_SCREEN] = core::key::PRINT_SCREEN;
            keys[GLFW_KEY_PAUSE] = core::key::PAUSE;
            keys[GLFW_KEY_F1] = core::key::FUNCTION_1;
            keys[GLFW_KEY_F2] = core::key::FUNCTION_2;
            keys[GLFW_KEY_F3] = core::key::FUNCTION_3;
            keys[GLFW_KEY_F4] = core::key::FUNCTION_4;
            keys[GLFW_KEY_F5] = core::key::FUNCTION_5;
            keys[GLFW_KEY_F6] = core::key::FUNCTION_6;
            keys[GLFW_KEY_F7] = core::key::FUNCTION_7;
            keys[GLFW_KEY_F8] = core::key::FUNCTION_8;
            keys[GLFW_KEY_F9] = core::key::FUNCTION_9;
            keys[GLFW_KEY_F10] = core::key::FUNCTION_10;
            keys[GLFW_KEY_F11] = core::key::FUNCTION_11;
            keys[GLFW_KEY_F12] = core::key::FUNCTION_12;
            keys[GLFW_KEY_F13] = core::key::FUNCTION_13;
            keys[GLFW_KEY_F14] = core::key::FUNCTION_14;
            keys[GLFW_KEY_F15] = core::key::FUNCTION_15;
            keys[GLFW_KEY_F16] = core::key::FUNCTION_16;
            keys[GLFW_KEY_F17] = core::key::FUNCTION_17;
            keys[GLFW_KEY_F18] = core::key::FUNCTION_18;
            keys[GLFW_KEY_F19] = core::key::FUNCTION_19;
            keys[GLFW_KEY_F20] = core::key::FUNCTION_20;
            keys[GLFW_KEY_F21] = core::key::FUNCTION_21;
            keys[GLFW_KEY_F22] = core::key::FUNCTION_22;
            keys[GLFW_KEY_F23] = core::key::FUNCTION_23;
            keys[GLFW_KEY_F24] = core::key::FUNCTION_24;
            keys[GLFW_KEY_F25] = core::key::FUNCTION_25;
            keys[GLFW_KEY_KP_0] = core::key::KP_0;
            keys[GLFW_KEY_KP_1] = core::key::KP_1;
            keys[GLFW_KEY_KP_2] = core::key::KP_2;
            keys[GLFW_KEY_KP_3] = core::key::KP_3;
            keys[GLFW_KEY_KP_4] = core::key::KP_4;
            keys[GLFW_KEY_KP_5] = core::key::KP_5;
            keys[GLFW_KEY_KP_6] = core::key::KP_6;
            keys[GLFW_KEY_KP_7] = core::key::KP_7;
            keys[GLFW_KEY_KP_8] = core::key::KP_8;
            keys[GLFW_KEY_KP_9] = core::key::KP_9;
            keys[GLFW_KEY_KP_DECIMAL] = core::key::KP_DECIMAL;
            keys[GLFW_KEY_KP_DIVIDE] = core::key::KP_DIVIDE;
            keys[GLFW_KEY_KP_MULTIPLY] = core::key::KP_MULTIPLY;
            keys[GLFW_KEY_KP_SUBTRACT] = core::key::KP_SUBTRACT;
            keys[GLFW_KEY_KP_ADD] = core::key::KP_ADD;
            keys[GLFW_KEY_KP_ENTER] = core::key::KP_ENTER;
            keys[GLFW_KEY_LEFT_SHIFT] = core::key::LEFT_SHIFT;
            keys[GLFW_KEY_LEFT_CONTROL] = core::key::LEFT_CONTROL;
            keys[GLFW_KEY_LEFT_ALT] = core::key::LEFT_ALT;
            keys[GLFW_KEY_LEFT_SUPER] = core::key::LEFT_SUPER;
            keys[GLFW_KEY_RIGHT_SHIFT] = core::key::RIGHT_SHIFT;
            keys[GLFW_KEY_RIGHT_CONTROL] = core::key::RIGHT_CONTROL;
            keys[GLFW_KEY_RIGHT_ALT] = core::key::RIGHT_ALT;
            keys[GLFW_KEY_RIGHT_SUPER] = core::key::RIGHT_SUPER;
            keys[GLFW_KEY_MENU] = core::key::MENU;

            return keys;
        }

        static consteval array<core::key_action, GLFW_REPEAT + 1> build_key_action_map()
        {
            array<core::key_action, GLFW_REPEAT + 1> actions;
            actions.fill(core::key_action::RELEASE);
            actions[GLFW_RELEASE] = core::key_action::RELEASE;
            actions[GLFW_PRESS] = core::key_action::PRESS;
            actions[GLFW_REPEAT] = core::key_action::REPEAT;

            return actions;
        }

        static consteval array<core::mouse_button, GLFW_MOUSE_BUTTON_LAST + 1> build_mouse_button_map()
        {
            array<core::mouse_button, GLFW_MOUSE_BUTTON_LAST + 1> buttons;
            buttons[GLFW_MOUSE_BUTTON_1] = core::mouse_button::MB_1;
            buttons[GLFW_MOUSE_BUTTON_2] = core::mouse_button::MB_2;
            buttons[GLFW_MOUSE_BUTTON_3] = core::mouse_button::MB_3;
            buttons[GLFW_MOUSE_BUTTON_4] = core::mouse_button::MB_4;
            buttons[GLFW_MOUSE_BUTTON_5] = core::mouse_button::MB_5;
            buttons[GLFW_MOUSE_BUTTON_6] = core::mouse_button::MB_6;
            buttons[GLFW_MOUSE_BUTTON_7] = core::mouse_button::MB_7;
            buttons[GLFW_MOUSE_BUTTON_8] = core::mouse_button::MB_8;
            return buttons;
        }

        static consteval array<core::mouse_action, GLFW_REPEAT + 1> build_mouse_action_map()
        {
            array<core::mouse_action, GLFW_REPEAT + 1> actions;
            actions.fill(core::mouse_action::RELEASE);
            actions[GLFW_RELEASE] = core::mouse_action::RELEASE;
            actions[GLFW_PRESS] = core::mouse_action::PRESS;

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
                                   .modifiers = static_cast<core::key_modifier>(mods),
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