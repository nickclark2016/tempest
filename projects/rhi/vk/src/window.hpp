#ifndef tempest_rhi_vk_window_hpp
#define tempest_rhi_vk_window_hpp

#include <tempest/int.hpp>
#include <tempest/string.hpp>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace tempest::rhi::vk
{
    class window_surface : public rhi::window_surface
    {
      public:
        window_surface(GLFWwindow* win, string name, uint32_t width, uint32_t height) noexcept;
        window_surface(const window_surface&) = delete;
        window_surface(window_surface&&) noexcept = delete;
        ~window_surface() override;

        window_surface& operator=(const window_surface&) = delete;
        window_surface& operator=(window_surface&&) noexcept = delete;

        uint32_t width() const noexcept override
        {
            return _width;
        }

        uint32_t height() const noexcept override
        {
            return _height;
        }

        string name() const noexcept override
        {
            return _name;
        }

        bool should_close() const noexcept override
        {
            return glfwWindowShouldClose(_window);
        }

        expected<VkSurfaceKHR, VkResult> get_surface(VkInstance instance) noexcept
        {
            if (_surface == VK_NULL_HANDLE)
            {
                auto result = glfwCreateWindowSurface(instance, _window, nullptr, &_surface);
                if (result != VK_SUCCESS)
                {
                    return unexpected{result};
                }
            }

            return _surface;
        }

        bool minimized() const noexcept override
        {
            return _is_minimized;
        }

        void close() override
        {
            glfwSetWindowShouldClose(_window, true);
        }

        void release_handle() noexcept
        {
            _surface = VK_NULL_HANDLE;
            _window = nullptr;
        }

        void register_keyboard_callback(function<void(const core::key_state&)>&& cb) noexcept override;
        void register_mouse_callback(function<void(const core::mouse_button_state&)>&& cb) noexcept override;
        void register_cursor_callback(function<void(float, float)>&& cb) noexcept override;
        void register_scroll_callback(function<void(float, float)>&& cb) noexcept override;
        void register_close_callback(function<void()>&& cb) noexcept override;
        void register_resize_callback(function<void(uint32_t, uint32_t)>&& cb) noexcept override;
        void register_focus_callback(function<void(bool)>&& cb) noexcept override;
        void register_minimize_callback(function<void(bool)>&& cb) noexcept override;

        void execute_keyboard_callbacks(const core::key_state& state) const noexcept;
        void execute_mouse_callbacks(const core::mouse_button_state& state) const noexcept;
        void execute_cursor_callbacks(float x, float y) const noexcept;
        void execute_scroll_callbacks(float x, float y) const noexcept;
        void execute_close_callbacks() const noexcept;
        void execute_resize_callbacks(uint32_t width, uint32_t height) const noexcept;
        void execute_focus_callbacks(bool focused) const noexcept;
        void execute_minimize_callbacks(bool minimized) const noexcept;

        void set_minimized(bool minimized) noexcept
        {
            _is_minimized = minimized;
        }

        void set_size(uint32_t width, uint32_t height) noexcept
        {
            _width = width;
            _height = height;
        }

      private:
        GLFWwindow* _window;
        string _name;
        VkSurfaceKHR _surface = VK_NULL_HANDLE;
        bool _is_minimized = false;
        uint32_t _width;
        uint32_t _height;

        vector<function<void(const core::key_state&)>> _keyboard_callbacks;
        vector<function<void(const core::mouse_button_state&)>> _mouse_callbacks;
        vector<function<void(float, float)>> _cursor_callbacks;
        vector<function<void(float, float)>> _scroll_callbacks;
        vector<function<void()>> _close_callbacks;
        vector<function<void(uint32_t, uint32_t)>> _resize_callbacks;
        vector<function<void(bool)>> _focus_callbacks;
        vector<function<void(bool)>> _minimize_callbacks;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_window_hpp
