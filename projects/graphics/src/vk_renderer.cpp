#include "vk_renderer.hpp"

#include "glfw_window.hpp"

#include <GLFW/glfw3.h>

#include <cassert>

namespace tempest::graphics::vk
{
    namespace
    {
        instance create_instance()
        {
            auto instance_wrapper = instance(instance_factory::create_info{
                .name{"Tempest Renderer"},
                .version_major{0},
                .version_minor{0},
                .version_patch{1},
            });

            return instance_wrapper;
        }
    }

    renderer::renderer() : _inst{create_instance()}
    {
        auto vkb_instance = _inst.raw();

        auto& device_wrapper = _inst.get_devices()[0];
        auto vkb_device = reinterpret_cast<device*>(device_wrapper.get())->raw();

        _graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
        auto graphics_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

        _transfer_queue = vkb_device.get_queue(vkb::QueueType::transfer).value();
        auto transfer_queue_family_index = vkb_device.get_dedicated_queue_index(vkb::QueueType::transfer).value();

        _compute_queue = vkb_device.get_queue(vkb::QueueType::compute).value();
        auto compute_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::compute).value();

        _ctx.emplace(vuk::ContextCreateParameters{
            .instance{vkb_instance.instance},
            .device{vkb_device.device},
            .physical_device{vkb_device.physical_device},
            .graphics_queue{_graphics_queue},
            .graphics_queue_family_index{graphics_queue_family_index},
            .compute_queue{_compute_queue},
            .compute_queue_family_index{compute_queue_family_index},
            .transfer_queue{_transfer_queue},
            .transfer_queue_family_index{transfer_queue_family_index},
            .pointers{},
        });

        _resources.emplace(*_ctx, FRAMES_IN_FLIGHT);
        _allocator.emplace(*_resources);

        _present_ready = vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>>(*_allocator);
        _render_complete = vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>>(*_allocator);
        _allocator->allocate_semaphores(*_present_ready);
        _allocator->allocate_semaphores(*_render_complete);
    }

    renderer::~renderer()
    {
        _present_ready.reset();
        _render_complete.reset();
        _resources.reset();
        _ctx.reset();

        _release();
    }

    void renderer::draw()
    {
    }

    void renderer::_release()
    {
    }
} // namespace tempest::graphics::vk