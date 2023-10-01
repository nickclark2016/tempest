#ifndef tempest_graphics_vk_render_device_hpp
#define tempest_graphics_vk_render_device_hpp

#include <tempest/object_pool.hpp>
#include <tempest/render_device.hpp>

#include <functional>
#include <optional>
#include <vector>

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

namespace tempest::graphics::vk
{
    struct image
    {
        VmaAllocation allocation;
        VmaAllocationInfo alloc_info;
        VkImage image;
        VkImageView view;
        VkImageCreateInfo img_info;
        VkImageViewCreateInfo view_info;
        std::string name;
    };

    struct buffer
    {
        VmaAllocation allocation;
        VkBuffer buffer;
        VkBufferCreateInfo info;
        std::string name;
    };

    class resource_deletion_queue
    {
      public:
        explicit resource_deletion_queue(std::size_t frames_in_flight);

        void add_to_queue(std::size_t current_frame, std::function<void()> deleter);
        void flush_frame(std::size_t current_frame);
        void flush_all();

      private:
        struct delete_info
        {
            std::size_t frame;
            std::function<void()> deleter;
        };

        std::vector<delete_info> _queue;
        std::size_t _frames_in_flight;
    };

    class render_device : public graphics::render_device
    {
      public:
        explicit render_device(core::allocator* alloc, vkb::Instance instance, vkb::PhysicalDevice physical,
                               vkb::Device device);
        ~render_device() override;

        void start_frame() noexcept override;
        void end_frame() noexcept override;

        buffer* access_buffer(buffer_resource_handle handle) noexcept;
        const buffer* access_buffer(buffer_resource_handle handle) const noexcept;
        buffer_resource_handle allocate_buffer();
        buffer_resource_handle create_buffer(const buffer_create_info& ci) override;
        buffer_resource_handle create_buffer(const buffer_create_info& ci, buffer_resource_handle handle);
        void release_buffer(buffer_resource_handle handle);

        image* access_image(image_resource_handle handle) noexcept;
        const image* access_image(image_resource_handle handle) const noexcept;
        image_resource_handle allocate_image();
        image_resource_handle create_image(const image_create_info& ci) override;
        image_resource_handle create_image(const image_create_info& ci, image_resource_handle handle);
        void release_image(image_resource_handle handle);

      private:
        core::allocator* _alloc;
        vkb::Instance _instance;
        vkb::PhysicalDevice _physical;
        vkb::Device _device;
        vkb::DispatchTable _dispatch;
        VmaAllocator _vk_alloc{VK_NULL_HANDLE};

        std::optional<core::generational_object_pool> _images;
        std::optional<core::generational_object_pool> _buffers;

        std::optional<resource_deletion_queue> _delete_queue;

        std::size_t _current_frame{0};
        static constexpr std::size_t _frames_in_flight{2};
    };

    class render_context : public graphics::render_context
    {
      public:
        explicit render_context(core::allocator* alloc);
        ~render_context() override;

        bool has_suitable_device() const noexcept override;
        std::uint32_t device_count() const noexcept override;
        graphics::render_device& get_device(std::uint32_t idx) override;

      private:
        std::vector<std::unique_ptr<render_device>> _devices;

        vkb::Instance _instance;
    };
} // namespace tempest::graphics::vk

#endif // tempest_graphics_vk_render_device_hpp