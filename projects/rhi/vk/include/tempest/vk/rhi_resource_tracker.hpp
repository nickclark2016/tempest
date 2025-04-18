#ifndef tempest_rhi_rhi_resource_tracker_hpp
#define tempest_rhi_rhi_resource_tracker_hpp

#include <tempest/flat_unordered_map.hpp>
#include <tempest/rhi_types.hpp>

#include <VkBootstrap.h>

#include <atomic>

namespace tempest::rhi::vk
{
    class device;

    class global_timeline
    {
      public:
        global_timeline(vkb::DispatchTable& dispatch);
        global_timeline(const global_timeline&) = delete;
        global_timeline(global_timeline&&) noexcept = delete;

        ~global_timeline() = default;

        global_timeline& operator=(const global_timeline&) = delete;
        global_timeline& operator=(global_timeline&&) noexcept = delete;

        VkSemaphore timeline_sem() const noexcept;
        uint64_t get_current_timestamp() const noexcept;
        uint64_t increment_and_get_timestamp() noexcept;

        void destroy() noexcept;

      private:
        vkb::DispatchTable* _dispatch;
        std::atomic<uint64_t> _current_timestamp; // Probably will never wrap

        VkSemaphore _timeline_sem{VK_NULL_HANDLE};
    };

    class resource_tracker
    {
      public:
        resource_tracker(device* dev, vkb::DispatchTable& dispatch, global_timeline& timeline);
        resource_tracker(const resource_tracker&) = delete;
        resource_tracker(resource_tracker&&) noexcept = delete;

        ~resource_tracker() = default;

        resource_tracker& operator=(const resource_tracker&) = delete;
        resource_tracker& operator=(resource_tracker&&) noexcept = delete;

        void track(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer, uint64_t timeline_value);
        void track(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image, uint64_t timeline_value);
        void untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer);
        void untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image);

        bool is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer) const noexcept;
        bool is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image) const noexcept;

        void release(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer);
        void release(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image);

        global_timeline& get_timeline() noexcept
        {
            return *_timeline;
        }

        const global_timeline& get_timeline() const noexcept
        {
            return *_timeline;
        }

        void try_release() noexcept;
        void destroy() noexcept;

      private:
        struct tracking_state
        {
            VkObjectType object;
            uint64_t timeline_value;
            bool deletion_requested;
        };

        device* _device;
        vkb::DispatchTable* _dispatch;
        global_timeline* _timeline;

        // All tracked resources
        flat_unordered_map<rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>, tracking_state> _tracked_buffers;
        flat_unordered_map<rhi::typed_rhi_handle<rhi::rhi_handle_type::image>, tracking_state> _tracked_images;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_rhi_resource_tracker_hpp
