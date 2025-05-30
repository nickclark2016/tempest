#ifndef tempest_glfw_window_hpp
#define tempest_glfw_window_hpp

#include <tempest/functional.hpp>
#include <tempest/vector.hpp>
#include <tempest/window.hpp>

#include <GLFW/glfw3.h>

namespace tempest::graphics::glfw
{
    class window final : public iwindow
    {
      public:
        window(const window_factory::create_info& info);
        window(const window&) = delete;
        window(window&& other) noexcept;
        ~window() override;

        window& operator=(const window&) = delete;
        window& operator=(window&& rhs) noexcept;

        inline bool should_close() const noexcept override
        {
            return glfwWindowShouldClose(_win) == GLFW_TRUE;
        }

        inline void close() override
        {
            glfwSetWindowShouldClose(_win, GLFW_TRUE);
        }

        std::uint32_t width() const noexcept override
        {
            return _width;
        }

        std::uint32_t height() const noexcept override
        {
            return _height;
        }

        bool minimized() const noexcept override
        {
            return _width == 0 || _height == 0;
        }

        void show() override
        {
            glfwShowWindow(_win);
        }

        void disable_cursor(bool disable = true) override
        {
            glfwSetInputMode(_win, GLFW_CURSOR, disable ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }

        bool is_cursor_disabled() const override
        {
            return glfwGetInputMode(_win, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
        }

        void register_keyboard_callback(function<void(const core::key_state&)>&& cb) override
        {
            _keyboard_callbacks.push_back(std::move(cb));
        }

        void register_mouse_callback(function<void(const core::mouse_button_state&)>&& cb) override
        {
            _mouse_callbacks.push_back(std::move(cb));
        }

        void register_cursor_callback(function<void(float, float)>&& cb) override
        {
            _cursor_callbacks.push_back(std::move(cb));
        }

        void register_scroll_callback(function<void(float, float)>&& cb) override
        {
            _scroll_callbacks.push_back(std::move(cb));
        }

        GLFWwindow* raw() const noexcept
        {
            return _win;
        }

      private:
        GLFWwindow* _win{};
        std::uint32_t _width{};
        std::uint32_t _height{};

        vector<function<void(const core::key_state&)>> _keyboard_callbacks;
        vector<function<void(const core::mouse_button_state&)>> _mouse_callbacks;
        vector<function<void(float, float)>> _cursor_callbacks;
        vector<function<void(float, float)>> _scroll_callbacks;

        void _release();
    };
} // namespace tempest::graphics::glfw

#endif