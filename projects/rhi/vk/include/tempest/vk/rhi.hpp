#ifndef tempest_rhi_vk_rhi_hpp
#define tempest_rhi_vk_rhi_hpp

#include <tempest/optional.hpp>
#include <tempest/rhi.hpp>
#include <tempest/slot_map.hpp>

#include <VkBootstrap.h>

namespace tempest::rhi::vk
{
    class device;
    class work_queue;

    class instance : public rhi::instance
    {
      public:
        explicit instance(vkb::Instance instance, vector<vkb::PhysicalDevice> devices) noexcept;
        ~instance() override;

        vector<rhi_device_description> get_devices() const noexcept override;
        rhi::device& acquire_device(uint32_t device_index) noexcept override;

      private:
        vkb::Instance _vkb_instance;
        vector<vkb::PhysicalDevice> _vkb_phys_devices;
        vector<unique_ptr<vk::device>> _devices;

        friend unique_ptr<rhi::instance> create_instance() noexcept;
    };

    class work_queue : public rhi::work_queue
    {
      public:
        work_queue() = default;
        explicit work_queue(vkb::DispatchTable* dispatch, VkQueue queue, uint32_t queue_family_index) noexcept;
        ~work_queue() override;

        typed_rhi_handle<rhi_handle_type::command_list> get_command_list() noexcept override;

        bool submit(span<const submit_info> infos,
                    typed_rhi_handle<rhi_handle_type::fence> fence =
                        typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept override;
        bool present(const present_info& info) noexcept override;

      private:
        vkb::DispatchTable* _dispatch;
        VkQueue _queue;
        uint32_t _queue_family_index;
    };

    struct image
    {
        VkImage image;
        VkImageView image_view;
        bool swapchain_image;
    };

    struct swapchain
    {
        vkb::Swapchain swapchain;
        VkSurfaceKHR surface;
        vector<typed_rhi_handle<rhi_handle_type::image>> images;
    };

    class device : public rhi::device
    {
      public:
        explicit device(vkb::Device dev, vkb::Instance* instance);
        ~device() override;

        device(const device&) = delete;
        device(device&&) noexcept = delete;
        device& operator=(const device&) = delete;
        device& operator=(device&&) noexcept = delete;

        typed_rhi_handle<rhi_handle_type::buffer> create_buffer(const buffer_desc& desc) noexcept override;
        typed_rhi_handle<rhi_handle_type::image> create_image(const image_desc& desc) noexcept override;
        typed_rhi_handle<rhi_handle_type::fence> create_fence(const fence_info& info) noexcept override;
        typed_rhi_handle<rhi_handle_type::semaphore> create_semaphore(const semaphore_info& info) noexcept override;
        typed_rhi_handle<rhi_handle_type::render_surface> create_render_surface(
            const render_surface_desc& desc) noexcept override;

        void destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override;
        void destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept override;
        void destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept override;
        void destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept override;
        void destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept override;

        rhi::work_queue& get_primary_work_queue() noexcept override;
        rhi::work_queue& get_dedicated_transfer_queue() noexcept override;
        rhi::work_queue& get_dedicated_compute_queue() noexcept override;

        render_surface_info query_render_surface_info(const rhi::window_surface& window) noexcept override;
        span<const typed_rhi_handle<rhi_handle_type::image>> get_render_surfaces(
            typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept override;
        expected<swapchain_image_acquire_info_result, swapchain_error_code> acquire_next_image(
            typed_rhi_handle<rhi_handle_type::semaphore> signal_sem,
            typed_rhi_handle<rhi_handle_type::fence> signal_fence) noexcept;

        typed_rhi_handle<rhi_handle_type::image> acquire_image(image img) noexcept;

      private:
        vkb::Instance* _vkb_instance;
        vkb::Device _vkb_device;
        vkb::DispatchTable _dispatch_table;

        optional<work_queue> _primary_work_queue;
        optional<work_queue> _dedicated_transfer_queue;
        optional<work_queue> _dedicated_compute_queue;

        slot_map<image> _images;
        slot_map<swapchain> _swapchains;
    };

    unique_ptr<rhi::instance> create_instance() noexcept;
    unique_ptr<rhi::window_surface> create_window_surface(const rhi::window_surface_desc& desc) noexcept;
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_rhi_hpp
