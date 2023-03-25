#ifndef tempest_graphics_command_buffer_hpp__
#define tempest_graphics_command_buffer_hpp__

#include "enums.hpp"
#include "fwd.hpp"
#include "resources.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>

namespace tempest::graphics
{
    class command_buffer_ring;

    class command_buffer
    {
      public:
        command_buffer() = default;
        explicit command_buffer(VkCommandBuffer buf, gfx_device* device, queue_type type, std::uint32_t buffer_size,
                                std::uint32_t submit_size, bool is_baked);

        operator VkCommandBuffer() const noexcept;

        command_buffer& reset();

        command_buffer& set_clear_color(float r, float g, float b, float a);
        command_buffer& set_clear_depth_stencil(float depth, std::uint32_t stencil);
        command_buffer& set_scissor_region(VkRect2D scissor);
        command_buffer& use_default_scissor();
        command_buffer& set_viewport(VkViewport viewport);
        command_buffer& use_default_viewport();
        command_buffer& bind_render_pass(render_pass_handle pass);
        command_buffer& bind_pipeline(pipeline_handle pipeline);
        command_buffer& draw(const std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                             std::uint32_t first_instance);

        void begin();
        void end();

      private:
        VkCommandBuffer _buf{VK_NULL_HANDLE};
        gfx_device* _device{nullptr};
        std::array<VkDescriptorSet, 16> _descriptors{};
        render_pass* _active_pass{nullptr};
        pipeline* _active_pipeline{nullptr};
        std::array<VkClearValue, 2> _clear_values{}; // 0 - color, 1 - depth
        bool _is_recording{false};
        std::uint32_t _handle{0};
        std::uint32_t _current_command{0};
        resource_handle _resource{invalid_resource_handle};
        queue_type _type{queue_type::GRAPHICS};
        std::uint32_t _buffer_size{0};
        bool _is_baked{false};

        friend class command_buffer_ring;
    };

    class command_buffer_ring
    {
      public:
        command_buffer_ring(gfx_device* dev);
        command_buffer_ring(const command_buffer_ring&) = delete;
        command_buffer_ring(command_buffer_ring&&) noexcept = delete;
        ~command_buffer_ring();

        command_buffer_ring& operator=(const command_buffer_ring&) = delete;
        command_buffer_ring& operator=(command_buffer_ring&&) noexcept = delete;

        VkCommandPool get_command_pool(std::size_t index) const noexcept;
        void reset_pools(std::uint32_t frame);
        command_buffer& fetch_buffer(std::uint32_t frame);
        command_buffer& fetch_buffer_instant(std::uint32_t frame);

      private:
        static constexpr std::size_t max_threads = 1;
        static constexpr std::size_t max_pools = 3 * max_threads;
        static constexpr std::size_t buffers_per_pool = 4;
        static constexpr std::size_t max_buffers = buffers_per_pool * max_pools;

        gfx_device* _dev;
        std::array<VkCommandPool, max_pools> _cmd_pools;
        std::array<command_buffer, max_buffers> _command_buffers;
        std::array<std::uint8_t, max_pools> _next_free_per_frame{};
    };
} // namespace tempest::graphics

#endif // tempest_graphics_command_buffer_hpp__