#include "vk_renderer.hpp"

#include "glfw_window.hpp"

#include <GLFW/glfw3.h>

#include <cassert>

namespace tempest::graphics::vk
{
    commands::commands(VkCommandBuffer buffer) : _buffer{buffer}
    {
    }

    renderer::renderer(const iinstance& instance, const idevice& dev, const iwindow& win)
        : _device{static_cast<const vk::device&>(dev).raw()}, _inst{static_cast<const vk::instance&>(instance).raw()},
          _dispatch{_device.make_table()}
    {
        const auto& inst = static_cast<const vk::instance&>(instance);
        const auto& window = static_cast<const glfw::window&>(win);

        auto surface_result = glfwCreateWindowSurface(inst.raw().instance, window.raw(), nullptr, &_surface);
        assert(surface_result == VK_SUCCESS && "Failed to create VkSurfaceKHR.");

        auto swapchain_result =
            vkb::SwapchainBuilder{_device, _surface}
                .set_desired_extent(window.width(), window.height()) // on the derived class to skip a virtual dispatch
                .set_desired_min_image_count(3)
                .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                .build();
        assert(swapchain_result && "Failed to create VkSwapchainKHR.");
        _swapchain = *swapchain_result;

        auto swapchain_view_result = _swapchain.get_image_views();
        assert(swapchain_view_result && "Failed to fetch swapchain images.");
        _swapchain_images = *swapchain_view_result;

        const VkSemaphoreCreateInfo semCi = {
            .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
        };

        const VkFenceCreateInfo fenceCi = {
            .sType{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO},
            .pNext{nullptr},
            .flags{VK_FENCE_CREATE_SIGNALED_BIT},
        };

        for (auto& frame : _frames)
        {
            auto present_result = _dispatch.createSemaphore(&semCi, nullptr, &frame.present);
            auto render_result = _dispatch.createSemaphore(&semCi, nullptr, &frame.render);
            auto render_wait_result = _dispatch.createFence(&fenceCi, nullptr, &frame.render_wait);

            assert(present_result == VK_SUCCESS && render_result == VK_SUCCESS && render_wait_result == VK_SUCCESS &&
                   "Failed to create frame synchronization primitives.");
        }
    }

    renderer::renderer(renderer&& other) noexcept
        : _swapchain{std::move(other._swapchain)}, _surface{std::move(other._surface)}, _device{std::move(
                                                                                            other._device)},
          _inst{std::move(other._inst)}, _dispatch{std::move(other._dispatch)}, _frames{std::move(other._frames)}
    {
        other._swapchain = {};
        other._surface = {};
        other._device = {};
        other._inst = {};
        other._dispatch = {};
        other._frames = {};
    }

    renderer::~renderer()
    {
        _release();
    }

    renderer& renderer::operator=(renderer&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release();

        std::swap(_swapchain, rhs._swapchain);
        std::swap(_surface, rhs._surface);
        std::swap(_device, rhs._device);
        std::swap(_inst, rhs._inst);
        std::swap(_dispatch, rhs._dispatch);
        std::swap(_frames, rhs._frames);

        return *this;
    }

    void renderer::draw(const draw_command& cmd)
    {
        // fetch the next frame

        commands buf{nullptr};
        cmd(buf);

        // submit
        // present
        // increment frame counter
    }

    void renderer::_release()
    {
        for (auto& frame : _frames)
        {
            if (frame.present)
            {
                _dispatch.destroySemaphore(frame.present, nullptr);
            }

            if (frame.render)
            {
                _dispatch.destroySemaphore(frame.render, nullptr);
            }

            if (frame.render_wait)
            {
                _dispatch.destroyFence(frame.render_wait, nullptr);
            }
        }

        _frames = {};

        if (_swapchain)
        {
            _swapchain.destroy_image_views(_swapchain_images);
            vkb::destroy_swapchain(_swapchain);
            _swapchain = {};
        }

        if (_surface)
        {
            vkb::destroy_surface(_inst, _surface);
            _surface = {};
        }

        _dispatch = {};
    }
} // namespace tempest::graphics::vk