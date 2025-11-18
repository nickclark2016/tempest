#include <tempest/rhi_types.hpp>
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
            auto resource_key = extract_resource_key<rhi_handle_type::buffer>(key);
            dev->release_resource_immediate(resource_key);
        }

        void release_image(uint64_t key, device* dev)
        {
            auto resource_key = extract_resource_key<rhi_handle_type::image>(key);
            dev->release_resource_immediate(resource_key);
        }

        void release_graphics_pipeline(uint64_t key, device* dev)
        {
            auto resource_key = extract_resource_key<rhi_handle_type::graphics_pipeline>(key);
            dev->release_resource_immediate(resource_key);
        }

        void release_compute_pipeline(uint64_t key, device* dev)
        {
            auto resource_key = extract_resource_key<rhi_handle_type::compute_pipeline>(key);
            dev->release_resource_immediate(resource_key);
        }

        void release_descriptor_set(uint64_t key, device* dev)
        {
            auto resource_key = extract_resource_key<rhi_handle_type::descriptor_set>(key);
            dev->release_resource_immediate(resource_key);
        }

        void release_sampler(uint64_t key, device* dev)
        {
            auto resource_key = extract_resource_key<rhi_handle_type::sampler>(key);
            dev->release_resource_immediate(resource_key);
        }
    } // namespace

    resource_tracker::resource_tracker(device* dev, vkb::DispatchTable& dispatch) : _device{dev}, _dispatch{&dispatch}
    {
    }

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer, uint64_t timeline_value,
                                 vk::work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::buffer, buffer.generation, buffer.id);
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
            auto& resource = it->second;
            if (resource.delete_requested)
            {
                logger->error("Buffer Resource {}:{} is already marked for deletion", buffer.generation, buffer.id);
            }

            // Check if the resource is already tracking usage on the queue and queue index
            auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
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

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image, uint64_t timeline_value,
                                 work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::image, image.generation, image.id);
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
            auto& resource = it->second;
            if (resource.delete_requested)
            {
                // Get the resource name
                const auto& name = _device->get_image(image)->name;
                logger->warn("Image Resource {}:{} ({}) is already marked for deletion", image.generation, image.id,
                             name.empty() ? "Unknown" : name.c_str());
            }
            // Check if the resource is already tracking usage on the queue and queue index
            auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
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

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> sampler, uint64_t timeline_value,
                                 work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::sampler, sampler.generation, sampler.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            _tracked_resources[key] = tracked_resource{
                .destroy_fn = release_sampler,
                .key = key,
                .delete_requested = false,
                .usage_records = {},
            };
            auto& resource = _tracked_resources[key];
            resource.usage_records.push_back({.queue = queue, .timeline_value = timeline_value});
        }
        else
        {
            auto& resource = it->second;
            if (resource.delete_requested)
            {
                logger->error("Sampler Resource {}:{} is already marked for deletion", sampler.generation, sampler.id);
            }
            // Check if the resource is already tracking usage on the queue and queue index
            auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
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

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline,
                                 uint64_t timeline_value, work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::graphics_pipeline, pipeline.generation, pipeline.id);
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
            auto& resource = it->second;
            if (resource.delete_requested)
            {
                logger->error("Graphics Pipeline Resource {}:{} is already marked for deletion", pipeline.generation,
                              pipeline.id);
            }
            // Check if the resource is already tracking usage on the queue and queue index
            auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
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

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> desc_set,
                                 uint64_t timeline_value, work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::descriptor_set, desc_set.generation, desc_set.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            _tracked_resources[key] = tracked_resource{
                .destroy_fn = release_descriptor_set,
                .key = key,
                .delete_requested = false,
                .usage_records = {},
            };
            auto& resource = _tracked_resources[key];
            resource.usage_records.push_back({.queue = queue, .timeline_value = timeline_value});
        }
        else
        {
            auto& resource = it->second;
            if (resource.delete_requested)
            {
                logger->error("Descriptor Set Resource {}:{} is already marked for deletion", desc_set.generation,
                              desc_set.id);
            }
            // Check if the resource is already tracking usage on the queue and queue index
            auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
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

    void resource_tracker::track(rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline,
                                 uint64_t timeline_value, work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::compute_pipeline, pipeline.generation, pipeline.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            _tracked_resources[key] = tracked_resource{
                .destroy_fn = release_compute_pipeline,
                .key = key,
                .delete_requested = false,
                .usage_records = {},
            };
            auto& resource = _tracked_resources[key];
            resource.usage_records.push_back({.queue = queue, .timeline_value = timeline_value});
        }
        else
        {
            auto& resource = it->second;
            if (resource.delete_requested)
            {
                logger->error("Compute Pipeline Resource {}:{} is already marked for deletion", pipeline.generation,
                              pipeline.id);
            }
            // Check if the resource is already tracking usage on the queue and queue index
            auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
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

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer, work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::buffer, buffer.generation, buffer.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
                                [queue](const resource_usage_record& record) { return record.queue == queue; });
        if (usage_it != resource.usage_records.end())
        {
            resource.usage_records.erase(usage_it);
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image, work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::image, image.generation, image.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
                                [queue](const resource_usage_record& record) { return record.queue == queue; });
        if (usage_it != resource.usage_records.end())
        {
            resource.usage_records.erase(usage_it);
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> sampler, work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::sampler, sampler.generation, sampler.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
                                [queue](const resource_usage_record& record) { return record.queue == queue; });
        if (usage_it != resource.usage_records.end())
        {
            resource.usage_records.erase(usage_it);
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline,
                                   work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::graphics_pipeline, pipeline.generation, pipeline.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
                                [queue](const resource_usage_record& record) { return record.queue == queue; });
        if (usage_it != resource.usage_records.end())
        {
            resource.usage_records.erase(usage_it);
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> desc_set,
                                   work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::descriptor_set, desc_set.generation, desc_set.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
                                [queue](const resource_usage_record& record) { return record.queue == queue; });
        if (usage_it != resource.usage_records.end())
        {
            resource.usage_records.erase(usage_it);
        }
    }

    void resource_tracker::untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline,
                                   work_queue* queue)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::compute_pipeline, pipeline.generation, pipeline.id);
        auto it = _tracked_resources.find(key);
        if (it == _tracked_resources.end())
        {
            logger->error("Resource {} is not tracked", key);
            return;
        }
        auto& resource = it->second;
        auto usage_it = find_if(resource.usage_records.begin(), resource.usage_records.end(),
                                [queue](const resource_usage_record& record) { return record.queue == queue; });
        if (usage_it != resource.usage_records.end())
        {
            resource.usage_records.erase(usage_it);
        }
    }

    bool resource_tracker::is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer) const noexcept
    {
        auto key = make_resource_key(rhi::rhi_handle_type::buffer, buffer.generation, buffer.id);
        return _tracked_resources.find(key) != _tracked_resources.end();
    }

    bool resource_tracker::is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image) const noexcept
    {
        const auto key = make_resource_key(rhi::rhi_handle_type::image, image.generation, image.id);
        const auto tracked = _tracked_resources.find(key) != _tracked_resources.end();
        if (!tracked)
        {
            // Check is any mip views are tracked
            const auto img = _device->get_image(image);
            for (const auto& mip_view : img->mip_chain_views)
            {
                if (mip_view != null_handle)
                {
                    const auto mip_key =
                        make_resource_key(rhi::rhi_handle_type::image, mip_view.generation, mip_view.id);
                    if (_tracked_resources.find(mip_key) != _tracked_resources.end())
                    {
                        return true;
                    }
                }
            }
            return false;
        }
        return tracked;
    }

    bool resource_tracker::is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> sampler) const noexcept
    {
        auto key = make_resource_key(rhi::rhi_handle_type::sampler, sampler.generation, sampler.id);
        return _tracked_resources.find(key) != _tracked_resources.end();
    }

    bool resource_tracker::is_tracked(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline) const noexcept
    {
        auto key = make_resource_key(rhi::rhi_handle_type::graphics_pipeline, pipeline.generation, pipeline.id);
        return _tracked_resources.find(key) != _tracked_resources.end();
    }

    bool resource_tracker::is_tracked(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> desc_set) const noexcept
    {
        auto key = make_resource_key(rhi::rhi_handle_type::descriptor_set, desc_set.generation, desc_set.id);
        return _tracked_resources.find(key) != _tracked_resources.end();
    }

    bool resource_tracker::is_tracked(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline) const noexcept
    {
        auto key = make_resource_key(rhi::rhi_handle_type::compute_pipeline, pipeline.generation, pipeline.id);
        return _tracked_resources.find(key) != _tracked_resources.end();
    }

    void resource_tracker::request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::buffer, buffer.generation, buffer.id);
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

    void resource_tracker::request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::image, image.generation, image.id);
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

        // Mark the mip views for deletion as well
        const auto img = _device->get_image(image);
        for (const auto& mip_view : img->mip_chain_views)
        {
            if (mip_view != null_handle)
            {
                const auto mip_key =
                    make_resource_key(rhi::rhi_handle_type::image, mip_view.generation, mip_view.id);
                auto mip_it = _tracked_resources.find(mip_key);
                if (mip_it != _tracked_resources.end())
                {
                    auto& mip_resource = mip_it->second;
                    mip_resource.delete_requested = true;
                }
            }
        }

        resource.delete_requested = true;
    }

    void resource_tracker::request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> sampler)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::sampler, sampler.generation, sampler.id);
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

    void resource_tracker::request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::graphics_pipeline, pipeline.generation, pipeline.id);
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

    void resource_tracker::request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set> desc_set)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::descriptor_set, desc_set.generation, desc_set.id);
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

    void resource_tracker::request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline)
    {
        auto key = make_resource_key(rhi::rhi_handle_type::compute_pipeline, pipeline.generation, pipeline.id);
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
                    auto record_it =
                        find_if(timeline_values.begin(), timeline_values.end(),
                                [&](const auto& wq_value_pair) { return wq_value_pair.first == record.queue; });
                    if (record_it != timeline_values.end() && record_it->second - 1 <= record.timeline_value)
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