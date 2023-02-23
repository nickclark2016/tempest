#ifndef tempest_glfw_window_hpp__
#define tempest_glfw_window_hpp__

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

        void show() override
        {
            glfwShowWindow(_win);
        }

      private:
        GLFWwindow* _win{};
        std::uint32_t _width{};
        std::uint32_t _height{};

        void _release();
    };
} // namespace tempest::graphics::glfw

#endif