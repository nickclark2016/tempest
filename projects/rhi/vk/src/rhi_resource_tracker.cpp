#include <tempest/vk/rhi_resource_tracker.hpp>

#include <tempest/logger.hpp>
#include <tempest/utility.hpp>
#include <tempest/vk/rhi.hpp>

namespace tempest::rhi::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::rhi::vk::rhi_resource_tracker"}});
    }

    global_timeline::global_timeline(vkb::DispatchTable& dispatch) : _dispatch{&dispatch}
    {
        VkSemaphoreTypeCreateInfo sem_type_ci = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 0,
        };

        VkSemaphoreCreateInfo sem_ci = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &sem_type_ci,
            .flags = 0,
        };

        VkResult result = _dispatch->createSemaphore(&sem_ci, nullptr, &_timeline_sem);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create global timeline semaphore: {}", to_underlying(result));
            _timeline_sem = VK_NULL_HANDLE;
        }
    }

    VkSemaphore global_timeline::timeline_sem() const noexcept
    {
        return _timeline_sem;
    }

    uint64_t global_timeline::get_current_timestamp() const noexcept
    {
        return _current_timestamp.load();
    }

    uint64_t global_timeline::increment_and_get_timestamp() noexcept
    {
        return _current_timestamp.fetch_add(1) + 1;
    }

    void global_timeline::destroy() noexcept
    {
        if (_timeline_sem != VK_NULL_HANDLE)
        {
            _dispatch->destroySemaphore(_timeline_sem, nullptr);
            _timeline_sem = VK_NULL_HANDLE;
        }
    }

    resource_tracker::resource_tracker(device* dev, vkb::DispatchTable& dispatch, global_timeline& timeline)
        : _device{dev}, _dispatch{&dispatch}, _timeline{&timeline}
    {
    }

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer, uint64_t timeline_value)
    {
        auto it = _tracked_buffers.find(buffer);
        if (it != _tracked_buffers.end())
        {
            it->second.timeline_value = timeline_value;
            it->second.deletion_request_count = 0;
        }
        else
        {
            _tracked_buffers[buffer] = {
                .object = VK_OBJECT_TYPE_BUFFER,
                .timeline_value = timeline_value,
                .deletion_request_count = 0,
            };
        }
    }

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image, uint64_t timeline_value)
    {
        auto it = _tracked_images.find(image);
        if (it != _tracked_images.end())
        {
            it->second.timeline_value = timeline_value;
            it->second.deletion_request_count = 0;
        }
        else
        {
            _tracked_images[image] = {
                .object = VK_OBJECT_TYPE_IMAGE,
                .timeline_value = timeline_value,
                .deletion_request_count = 0,
            };
        }
    }

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> desc_set,
                                 uint64_t timeline_value)
    {
        auto it = _tracked_desc_set_layouts.find(desc_set);
        if (it != _tracked_desc_set_layouts.end())
        {
            it->second.timeline_value = timeline_value;
            it->second.deletion_request_count = 0;
        }
        else
        {
            _tracked_desc_set_layouts[desc_set] = {
                .object = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                .timeline_value = timeline_value,
                .deletion_request_count = 0,
            };
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer)
    {
        auto it = _tracked_buffers.find(buffer);
        if (it != _tracked_buffers.end())
        {
            _tracked_buffers.erase(it);
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image)
    {
        auto it = _tracked_images.find(image);
        if (it != _tracked_images.end())
        {
            _tracked_images.erase(it);
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> desc_set)
    {
        auto it = _tracked_desc_set_layouts.find(desc_set);
        if (it != _tracked_desc_set_layouts.end())
        {
            _tracked_desc_set_layouts.erase(it);
        }
    }

    bool resource_tracker::is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer) const noexcept
    {
        return _tracked_buffers.find(buffer) != _tracked_buffers.end();
    }

    bool resource_tracker::is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image) const noexcept
    {
        return _tracked_images.find(image) != _tracked_images.end();
    }

    bool resource_tracker::is_tracked(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> layout) const noexcept
    {
        return _tracked_desc_set_layouts.find(layout) != _tracked_desc_set_layouts.end();
    }

    void resource_tracker::release(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer)
    {
        auto it = _tracked_buffers.find(buffer);
        if (it != _tracked_buffers.end())
        {
            it->second.deletion_request_count = 1;
        }
    }

    void resource_tracker::release(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image)
    {
        auto it = _tracked_images.find(image);
        if (it != _tracked_images.end())
        {
            it->second.deletion_request_count = 1;
        }
    }

    void resource_tracker::release(rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> layout)
    {
        auto it = _tracked_desc_set_layouts.find(layout);
        if (it != _tracked_desc_set_layouts.end())
        {
            it->second.deletion_request_count++;
        }
    }

    void resource_tracker::try_release() noexcept
    {
        uint64_t completed_value = 0;
        _dispatch->getSemaphoreCounterValue(_timeline->timeline_sem(), &completed_value);

        for (auto it = _tracked_buffers.begin(); it != _tracked_buffers.end();)
        {
            if (it->second.deletion_request_count && it->second.timeline_value <= completed_value)
            {
                _device->release_resource_immediate(it->first);
                it = _tracked_buffers.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = _tracked_images.begin(); it != _tracked_images.end();)
        {
            if (it->second.deletion_request_count && it->second.timeline_value <= completed_value)
            {
                _device->release_resource_immediate(it->first);
                it = _tracked_images.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = _tracked_desc_set_layouts.begin(); it != _tracked_desc_set_layouts.end();)
        {
            if (it->second.deletion_request_count && it->second.timeline_value <= completed_value)
            {
                // For each time a descriptor set had its deletion requested, we need to release the resource
                // as it is cached

                for (size_t i = 0; i < it->second.deletion_request_count; ++i)
                {
                    auto done = _device->release_resource_immediate(it->first);
                    if (done)
                    {
                        it = _tracked_desc_set_layouts.erase(it);
                        break;
                    }
                }
            }
            else
            {
                ++it;
            }
        }
    }

    void resource_tracker::destroy() noexcept
    {
        for (const auto& [key, _] : _tracked_buffers)
        {
            _device->release_resource_immediate(key);
        }

        for (const auto& [key, _] : _tracked_images)
        {
            _device->release_resource_immediate(key);
        }
    }
} // namespace tempest::rhi::vk