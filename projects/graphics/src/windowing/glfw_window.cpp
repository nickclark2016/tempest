#include "glfw_window.hpp"

#include <tempest/window.hpp>

#include <cassert>
#include <utility>

namespace tempest::graphics::glfw
{
    namespace
    {
        bool initialize_glfw()
        {
            static bool init = glfwInit();
            return init;
        }

        static consteval std::array<core::key, GLFW_KEY_LAST + 1> build_key_map()
        {
            std::array<core::key, GLFW_KEY_LAST + 1> keys;
            std::fill(std::begin(keys), std::end(keys), core::key::UNKNOWN);

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

        static consteval std::array<core::key_action, GLFW_REPEAT + 1> build_key_action_map()
        {
            std::array<core::key_action, GLFW_REPEAT + 1> actions;
            std::fill(std::begin(actions), std::end(actions), core::key_action::RELEASE);
            actions[GLFW_RELEASE] = core::key_action::RELEASE;
            actions[GLFW_PRESS] = core::key_action::PRESS;
            actions[GLFW_REPEAT] = core::key_action::REPEAT;

            return actions;
        }

        static consteval std::array<core::mouse_button, GLFW_MOUSE_BUTTON_LAST + 1> build_mouse_button_map()
        {
            std::array<core::mouse_button, GLFW_MOUSE_BUTTON_LAST + 1> buttons;
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

        static constexpr auto glfw_to_tempest_keys = build_key_map();
        static constexpr auto glfw_to_tempest_key_actions = build_key_action_map();
        static constexpr auto glfw_to_tempest_mouse_buttons = build_mouse_button_map();

    } // namespace

    window::window(const window_factory::create_info& info) : _width{info.width}, _height{info.height}
    {
        auto init = initialize_glfw();
        assert(init && "Failed to initialize GLFW.");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        _win = glfwCreateWindow(static_cast<int>(info.width), static_cast<int>(info.height), info.title.data(), nullptr,
                                nullptr);

        assert(_win && "Failed to create GLFW Window.");
        glfwSetWindowUserPointer(_win, this);

        glfwSetWindowSizeCallback(_win, [](GLFWwindow* win, int width, int height) {
            window* w = reinterpret_cast<window*>(glfwGetWindowUserPointer(win));
            w->_width = width;
            w->_height = height;
        });

        glfwSetKeyCallback(_win, [](GLFWwindow* win, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
            window* w = reinterpret_cast<window*>(glfwGetWindowUserPointer(win));
            auto k = glfw_to_tempest_keys[key];
            auto a = glfw_to_tempest_key_actions[action];
            auto m = static_cast<core::key_modifier>(mods);
            
            core::key_state state = {
                .k = k,
                .action = a,
                .modifiers = m
            };

            for (const auto& cb : w->_keyboard_callbacks)
            {
                cb(state);
            }
        });

        glfwSetMouseButtonCallback(_win, [](GLFWwindow* win, int button, int action, [[maybe_unused]] int mods) {
            window* w = reinterpret_cast<window*>(glfwGetWindowUserPointer(win));
            core::mouse_button_state state = {
                .button = glfw_to_tempest_mouse_buttons[button],
                .action = static_cast<core::mouse_action>(action)
            };

            for (const auto& cb : w->_mouse_callbacks)
            {
                cb(state);
            }
        });

        glfwSetCursorPosCallback(_win, [](GLFWwindow* win, double xpos, double ypos) {
            window* w = reinterpret_cast<window*>(glfwGetWindowUserPointer(win));
            for (const auto& cb : w->_cursor_callbacks)
            {
                cb(static_cast<float>(xpos), static_cast<float>(ypos));
            }
        });

        glfwSetScrollCallback(_win, [](GLFWwindow* win, double xoffset, double yoffset) {
            window* w = reinterpret_cast<window*>(glfwGetWindowUserPointer(win));
            for (const auto& cb : w->_scroll_callbacks)
            {
                cb(static_cast<float>(xoffset), static_cast<float>(yoffset));
            }
        });
    }

    window::window(window&& other) noexcept
        : _win{std::move(other._win)}, _width{std::move(other._width)}, _height{std::move(other._height)}
    {
        other._win = nullptr;
        glfwSetWindowUserPointer(_win, this);
    }

    window::~window()
    {
        _release();
    }

    window& window::operator=(window&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release();

        std::swap(_win, rhs._win);
        std::swap(_width, rhs._width);
        std::swap(_height, rhs._height);
        glfwSetWindowUserPointer(_win, this);

        return *this;
    }

    void window::_release()
    {
        if (_win)
        {
            glfwDestroyWindow(_win);
            _win = nullptr;
        }
    }
} // namespace tempest::graphics::glfw