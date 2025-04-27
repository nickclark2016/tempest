#ifndef tempest_rhi_rhi_resource_tracker_hpp
#define tempest_rhi_rhi_resource_tracker_hpp

#include <tempest/flat_unordered_map.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/rhi_types.hpp>

#include <VkBootstrap.h>

#include <atomic>

namespace tempest::rhi::vk
{
    class device;
    class work_queue;

    // resource key type
    // 8 bits for rhi handle type
    // 24 bits for generation
    // 32 bits for id

    using resource_key = uint64_t;

    inline constexpr resource_key make_resource_key(rhi::rhi_handle_type type, uint32_t generation,
                                                    uint32_t id) noexcept
    {
        return (static_cast<resource_key>(type) << 56) | (static_cast<resource_key>(generation) << 32) |
               static_cast<resource_key>(id);
    }

    inline constexpr void extract_resource_key(resource_key key, rhi::rhi_handle_type& type, uint32_t& generation,
                                               uint32_t& id) noexcept
    {
        type = static_cast<rhi::rhi_handle_type>((key >> 56) & 0xFF);
        generation = static_cast<uint32_t>((key >> 32) & 0xFFFFFF);
        id = static_cast<uint32_t>(key & 0xFFFFFFFF);
    }

    template <rhi::rhi_handle_type T>
    inline constexpr typed_rhi_handle<T> extract_resource_key(resource_key key)
    {
        rhi::rhi_handle_type type;
        uint32_t generation;
        uint32_t id;
        extract_resource_key(key, type, generation, id);

        assert(type == T);

        return typed_rhi_handle<T>{.id = id, .generation = generation};
    }

    struct resource_usage_record
    {
        work_queue* queue;
        uint64_t timeline_value;
    };

    struct tracked_resource
    {
        void (*destroy_fn)(uint64_t, device*); // key, device
        uint64_t key;
        bool delete_requested;
        inplace_vector<resource_usage_record, 8> usage_records;
    };

    class resource_tracker
    {
      public:
        resource_tracker(device* dev, vkb::DispatchTable& dispatch);
        resource_tracker(const resource_tracker&) = delete;
        resource_tracker(resource_tracker&&) noexcept = delete;

        ~resource_tracker() = default;

        resource_tracker& operator=(const resource_tracker&) = delete;
        resource_tracker& operator=(resource_tracker&&) noexcept = delete;

        void track(rhi::typed_rhi_handle<rhi::rhi_handle_type::BUFFER> buffer, uint64_t timeline_value,
                   vk::work_queue* queue);
        void track(rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> image, uint64_t timeline_value,
                   vk::work_queue* queue);
        void track(rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline, uint64_t timeline_value,
                   vk::work_queue* queue);
        void untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::BUFFER> buffer, vk::work_queue* queue);
        void untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> image, vk::work_queue* queue);
        void untrack(rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline, vk::work_queue* queue);

        bool is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::BUFFER> buffer) const noexcept;
        bool is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> image) const noexcept;
        bool is_tracked(rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline) const noexcept;

        void request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::BUFFER> buffer);
        void request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> image);
        void request_release(rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline);

        void try_release() noexcept;
        void destroy() noexcept;

      private:
        device* _device;
        [[maybe_unused]] vkb::DispatchTable* _dispatch;

        flat_unordered_map<resource_key, tracked_resource> _tracked_resources;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_rhi_resource_tracker_hpp
