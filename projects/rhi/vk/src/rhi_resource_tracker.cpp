#include <tempest/vk/rhi_resource_tracker.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/logger.hpp>
#include <tempest/utility.hpp>
#include <tempest/vk/rhi.hpp>

namespace tempest::rhi::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::rhi::vk::rhi_resource_tracker"}});

        void release_buffer(uint64_t key, device* dev)
        {
            auto resource_key = extract_resource_key<rhi_handle_type::BUFFER>(key);
            dev->release_resource_immediate(resource_key);
        }

        void release_image(uint64_t key, device* dev)
        {
            auto resource_key = extract_resource_key<rhi_handle_type::IMAGE>(key);
            dev->release_resource_immediate(resource_key);
        }

        void release_graphics_pipeline(uint64_t key, device* dev)
        {
            auto resource_key = extract_resource_key<rhi_handle_type::GRAPHICS_PIPELINE>(key);
            dev->release_resource_immediate(resource_key);
        }
    } // namespace

    resource_tracker::resource_tracker(device* dev, vkb::DispatchTable& dispatch) : _device{dev}, _dispatch{&dispatch}
    {
    }

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::BUFFER> buffer, uint64_t timeline_value,
                                 vk::work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::BUFFER, buffer.generation, buffer.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            _tracked_resources[key] = tracked_resource{
                .destroy_fn = &release_buffer,
                .key = key,
                .delete_requested = false,
                .usage_records = {},
            };

            auto& resource = _tracked_resources[key];
            resource.usage_records.push_back({.queue = queue, .timeline_value = timeline_value});
        }
        else
        {
            auto resource = it->second;

            if (resource.delete_requested)
            {
                logger->error("Resource {} is already marked for deletion", key);
                return;
            }

            // Check if the resource is already tracking usage on the queue and queue index
            auto usage_it =
                std::find_if(resource.usage_records.begin(), resource.usage_records.end(),
                             [queue](const resource_usage_record& record) { return record.queue == queue; });
            if (usage_it == resource.usage_records.end())
            {
                resource.usage_records.push_back({.queue = queue, .timeline_value = timeline_value});
            }
            else
            {
                usage_it->timeline_value = tempest::max(usage_it->timeline_value, timeline_value);
            }
        }
    }

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> image, uint64_t timeline_value,
                                 work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::IMAGE, image.generation, image.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            _tracked_resources[key] = tracked_resource{
                .destroy_fn = release_image,
                .key = key,
                .delete_requested = false,
                .usage_records = {},
            };
            auto& resource = _tracked_resources[key];
            resource.usage_records.push_back({.queue = queue, .timeline_value = timeline_value});
        }
        else
        {
            auto resource = it->second;
            if (resource.delete_requested)
            {
                logger->error("Resource {} is already marked for deletion", key);
                return;
            }
            // Check if the resource is already tracking usage on the queue and queue index
            auto usage_it =
                std::find_if(resource.usage_records.begin(), resource.usage_records.end(),
                             [queue](const resource_usage_record& record) { return record.queue == queue; });
            if (usage_it == resource.usage_records.end())
            {
                resource.usage_records.push_back({.queue = queue, .timeline_value = timeline_value});
            }
            else
            {
                usage_it->timeline_value = tempest::max(usage_it->timeline_value, timeline_value);
            }
        }
    }

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline,
                                 uint64_t timeline_value, work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::GRAPHICS_PIPELINE, pipeline.generation, pipeline.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            _tracked_resources[key] = tracked_resource{
                .destroy_fn = release_graphics_pipeline,
                .key = key,
                .delete_requested = false,
                .usage_records = {},
            };
            auto& resource = _tracked_resources[key];
            resource.usage_records.push_back({.queue = queue, .timeline_value = timeline_value});
        }
        else
        {
            auto resource = it->second;
            if (resource.delete_requested)
            {
                logger->error("Resource {} is already marked for deletion", key);
                return;
            }
            // Check if the resource is already tracking usage on the queue and queue index
            auto usage_it =
                std::find_if(resource.usage_records.begin(), resource.usage_records.end(),
                             [queue](const resource_usage_record& record) { return record.queue == queue; });
            if (usage_it == resource.usage_records.end())
            {
                resource.usage_records.push_back({.queue = queue, .timeline_value = timeline_value});
            }
            else
            {
                usage_it->timeline_value = tempest::max(usage_it->timeline_value, timeline_value);
            }
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::BUFFER> buffer, work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::BUFFER, buffer.generation, buffer.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        auto usage_it = std::find_if(resource.usage_records.begin(), resource.usage_records.end(),
                                     [queue](const resource_usage_record& record) { return record.queue == queue; });
        if (usage_it != resource.usage_records.end())
        {
            resource.usage_records.erase(usage_it);
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> image, work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::IMAGE, image.generation, image.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        auto usage_it = std::find_if(resource.usage_records.begin(), resource.usage_records.end(),
                                     [queue](const resource_usage_record& record) { return record.queue == queue; });
        if (usage_it != resource.usage_records.end())
        {
            resource.usage_records.erase(usage_it);
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline,
                                   work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::GRAPHICS_PIPELINE, pipeline.generation, pipeline.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        auto usage_it = std::find_if(resource.usage_records.begin(), resource.usage_records.end(),
                                     [queue](const resource_usage_record& record) { return record.queue == queue; });
        if (usage_it != resource.usage_records.end())
        {
            resource.usage_records.erase(usage_it);
        }
    }

    bool resource_tracker::is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::BUFFER> buffer) const noexcept
    {
        auto key = make_resource_key(rhi::rhi_handle_type::BUFFER, buffer.generation, buffer.id);
        return _tracked_resources.find(key) != _tracked_resources.end();
    }

    bool resource_tracker::is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> image) const noexcept
    {
        auto key = make_resource_key(rhi::rhi_handle_type::IMAGE, image.generation, image.id);
        return _tracked_resources.find(key) != _tracked_resources.end();
    }

    bool resource_tracker::is_tracked(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline) const noexcept
    {
        auto key = make_resource_key(rhi::rhi_handle_type::GRAPHICS_PIPELINE, pipeline.generation, pipeline.id);
        return _tracked_resources.find(key) != _tracked_resources.end();
    }

    void resource_tracker::request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::BUFFER> buffer)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::BUFFER, buffer.generation, buffer.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        if (resource.delete_requested)
        {
            logger->error("Resource {} is already marked for deletion", key);
            return;
        }
        resource.delete_requested = true;
    }

    void resource_tracker::request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> image)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::IMAGE, image.generation, image.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        if (resource.delete_requested)
        {
            logger->error("Resource {} is already marked for deletion", key);
            return;
        }
        resource.delete_requested = true;
    }

    void resource_tracker::request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::GRAPHICS_PIPELINE, pipeline.generation, pipeline.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        if (resource.delete_requested)
        {
            logger->error("Resource {} is already marked for deletion", key);
            return;
        }
        resource.delete_requested = true;
    }

    void resource_tracker::try_release() noexcept
    {
        // Compute once and reuse for all resources
        // Limits the number of calls to vulkan
        auto timeline_values = _device->compute_current_work_queue_timeline_values();

        for (auto it = _tracked_resources.begin(); it != _tracked_resources.end();)
        {
            auto& resource = it->second;
            if (resource.delete_requested)
            {
                bool can_release = true;

                for (const auto& record : resource.usage_records)
                {
                    if (timeline_values[record.queue] < record.timeline_value)
                    {
                        can_release = false;
                        break;
                    }
                }

                if (can_release)
                {
                    resource.destroy_fn(resource.key, _device);
                    it = _tracked_resources.erase(it);
                }
                else
                {
                    ++it;
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
        for (auto& [key, resource] : _tracked_resources)
        {
            if (resource.delete_requested)
            {
                resource.destroy_fn(resource.key, _device);
            }
        }
        _tracked_resources.clear();
    }
} // namespace tempest::rhi::vk