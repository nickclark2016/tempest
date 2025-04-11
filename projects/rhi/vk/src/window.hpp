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
        window_surface(GLFWwindow* win, string name) noexcept;
        window_surface(const window_surface&) = delete;
        window_surface(window_surface&&) noexcept = delete;
        ~window_surface() override;

        window_surface& operator=(const window_surface&) = delete;
        window_surface& operator=(window_surface&&) noexcept = delete;

        uint32_t width() const noexcept override
        {
            int width, height;
            glfwGetWindowSize(_window, &width, &height);
            return static_cast<uint32_t>(width);
        }

        uint32_t height() const noexcept override
        {
            int width, height;
            glfwGetWindowSize(_window, &width, &height);
            return static_cast<uint32_t>(height);
        }

        string name() const noexcept override
        {
            return _name;
        }

        bool should_close() const noexcept override
        {
            return glfwWindowShouldClose(_window);
        }

        expected<VkSurfaceKHR, VkResult> get_surface(VkInstance instance) const noexcept
        {
            VkSurfaceKHR surface;
            auto result = glfwCreateWindowSurface(instance, _window, nullptr, &surface);
            if (result != VK_SUCCESS)
            {
                return unexpected{result};
            }
            return surface;
        }

      private:
        GLFWwindow* _window;
        string _name;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_window_hpp
