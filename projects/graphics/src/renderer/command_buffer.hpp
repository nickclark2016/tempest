#ifndef tempest_graphics_command_buffer_hpp
#define tempest_graphics_command_buffer_hpp

#include "enums.hpp"
#include "fwd.hpp"
#include "resources.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>

namespace tempest::graphics
{
    class command_buffer_ring;

    struct state_transition_descriptor
    {
        texture_handle texture;
        buffer_handle buffer;

        std::uint32_t first_mip;
        std::uint32_t mip_count;
        std::uint32_t base_layer;
        std::uint32_t layer_count;
        std::uint32_t offset;
        std::uint32_t range;

        resource_state src_state;
        resource_state dst_state;
    };

    struct render_attachment_descriptor
    {
        texture_handle tex;

        VkImageLayout layout;
        VkAttachmentLoadOp load{VK_ATTACHMENT_LOAD_OP_CLEAR};
        VkAttachmentStoreOp store{VK_ATTACHMENT_STORE_OP_STORE};
        VkClearValue clear;

        texture_handle resolve_target{.index{invalid_resource_handle}};
        VkImageLayout resolve_layout;
        VkResolveModeFlagBits resolve_mode{VK_RESOLVE_MODE_NONE};
    };

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
        command_buffer& set_viewport(VkViewport viewport, bool flip = true);
        command_buffer& use_default_viewport(bool flip = true);
        command_buffer& bind_render_pass(render_pass_handle pass);
        command_buffer& begin_rendering(VkRect2D viewport, std::span<render_attachment_descriptor> colors,
                                        const std::optional<render_attachment_descriptor>& depth,
                                        const std::optional<render_attachment_descriptor>& stencil);
        command_buffer& bind_pipeline(pipeline_handle pipeline);
        command_buffer& bind_descriptor_set(std::span<descriptor_set_handle> sets, std::span<std::uint32_t> offsets,
                                            std::uint32_t first_set = 0);
        command_buffer& draw(const std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                             std::uint32_t first_instance);
        command_buffer& end_rendering();
        command_buffer& barrier(const execution_barrier& barrier);

        command_buffer& transition_to_depth_image(texture_handle depth_tex);
        command_buffer& transition_to_color_image(texture_handle color_tex);
        command_buffer& blit_image(texture_handle src, texture_handle dst);
        command_buffer& transition_resource(std::span<state_transition_descriptor> descs, pipeline_stage src,
                                            pipeline_stage dst);

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

#endif // tempest_graphics_command_buffer_hpp