#ifndef tempest_rhi_vk_execution_port_hpp
#define tempest_rhi_vk_execution_port_hpp

#include <tempest/array.hpp>
#include <tempest/assert.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/limits.hpp>
#include <tempest/mutex.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vector.hpp>

#include <VkBootstrap.h>
#include <VkBootstrapDispatch.h>

namespace tempest::rhi::vk
{
    class execution_port;
    class device;

    class TEMPEST_API command_list final : public rhi::command_list
    {
      public:
        command_list(VkCommandBuffer command_buffer, const vkb::DispatchTable& dispatch_table,
                     const vk::device& device) noexcept;
        command_list(const command_list&) = delete;
        command_list(command_list&&) noexcept = delete;

        /**
         * @note The slab that owns this command list is responsible for cleaning up the command buffer when it is
         * destroyed. The command list does not own the command buffer and should not attempt to free it.
         */
        ~command_list() override = default;

        command_list& operator=(const command_list&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        command_list& operator=(command_list&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        auto begin() const -> void override;
        auto end() const -> void override;

        // Synchronization
        auto pipeline_barrier(span<const texture_barrier> texture_barriers,
                              span<const buffer_barrier> buffer_barriers) const -> void override;
        auto signal_event(event_handle event, span<const texture_barrier> texture_sources,
                          span<const buffer_barrier> buffer_sources) const -> void override;
        auto wait_event(event_handle event, span<const texture_barrier> texture_destinations,
                        span<const buffer_barrier> buffer_destinations) const -> void override;
        auto reset_event(event_handle event,
                         enum_mask<pipeline_stage> stages = enum_mask{pipeline_stage::bottom_of_pipe}) const
            -> void override;

        // General commands
        auto push_constants(enum_mask<shader_stage> stages, uint32_t offset, span<const byte> data) -> void override;

        // Rendering
        auto begin_render_pass(span<const color_attachment> color_attachments,
                               optional<depth_stencil_attachment> depth_stencil_attachment, uint32_t width,
                               uint32_t height) -> void override;
        auto end_render_pass() -> void override;
        auto bind_pipeline(graphics_pipeline_handle pipeline) -> void override;
        auto set_viewport(float x, float y, float width, float height, // NOLINT(readability-identifier-length)
                          float min_depth, float max_depth) -> void override;
        auto set_scissor(int32_t x, int32_t y, uint32_t width, // NOLINT(readability-identifier-length)
                         uint32_t height) -> void override;
        auto set_depth_bias(float constant_factor, float clamp, float slope_factor) -> void override;
        auto set_stencil_reference(uint32_t reference) -> void override;
        auto set_stencil_compare_mask(uint32_t compare_mask) -> void override;
        auto set_stencil_write_mask(uint32_t write_mask) -> void override;
        auto bind_index_buffer(buffer_handle buffer, index_type type, uint64_t offset) -> void override;
        auto draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
            -> void override;
        auto draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
                          uint32_t first_instance) -> void override;
        auto draw_indirect(buffer_handle buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
            -> void override;
        auto draw_indexed_indirect(buffer_handle buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
            -> void override;
        auto draw_indirect_count(buffer_handle buffer, uint64_t offset, buffer_handle count_buffer,
                                 uint64_t count_buffer_offset, uint32_t max_draw_count, uint32_t stride)
            -> void override;
        auto draw_indexed_indirect_count(buffer_handle buffer, uint64_t offset, buffer_handle count_buffer,
                                         uint64_t count_buffer_offset, uint32_t max_draw_count, uint32_t stride)
            -> void override;

        // Compute
        auto bind_pipeline(compute_pipeline_handle pipeline) -> void override;
        auto dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) -> void override;
        auto dispatch_indirect(buffer_handle buffer, uint64_t offset) -> void override;

        // Transfer
        auto copy_buffer(buffer_handle src, buffer_handle dst, span<const buffer_copy_region> regions) -> void override;
        auto copy_buffer_to_texture(buffer_handle src, texture_handle dst, span<const buffer_texture_copy_region> regions)
            -> void override;
        auto copy_texture_to_buffer(texture_handle src, buffer_handle dst, span<const buffer_texture_copy_region> regions)
            -> void override;

        [[nodiscard]] operator VkCommandBuffer() const noexcept
        {
            return _command_buffer;
        }

      private:
        friend class execution_port;

        VkCommandBuffer _command_buffer;
        const vkb::DispatchTable* _dispatch_table;
        const vk::device* _parent_device;
    };

    /**
     * @brief A command list slab is a collection of command buffers that can be allocated from a single command pool.
     * It is used to manage the lifetime of command buffers and to ensure that command buffers are not reused while they
     * are still in use by the GPU. The slab is associated with a specific queue family and dispatch table, and can be
     * reset when it is no longer in use by the GPU.
     *
     * @tparam N The maximum number of command buffers that can be allocated from the slab.
     * @note The command list slab is not thread safe. It must be externally synchronized if accessed from
     * multiple threads.
     */
    template <size_t N>
    class command_list_slab final
    {
      public:
        static constexpr size_t max_command_lists = N;
        static constexpr size_t thread_id_sentinel = numeric_limits<uint32_t>::max();

        /**
         * @brief Constructs a command list slab with the specified dispatch table and queue family index. Allocates a
         * command pool and command buffers from the dispatch table.
         *
         * @param dispatch_table The dispatch table to use for Vulkan function calls.
         * @param device The Vulkan device to use for command pool and command buffer allocation.
         * @param queue_family_index The index of the queue family to use for the command pool and command buffers.
         */
        command_list_slab(const vkb::DispatchTable& dispatch_table, const vk::device& device,
                          uint32_t queue_family_index)
            : _dispatch_table{&dispatch_table}
        {
            auto command_pool_create_info = VkCommandPoolCreateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queue_family_index,
            };

            [[maybe_unused]] auto result =
                dispatch_table.createCommandPool(&command_pool_create_info, nullptr, &_command_pool);
            TEMPEST_ASSERT(result == VK_SUCCESS);

            auto command_buffer_allocate_info = VkCommandBufferAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = _command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(_command_buffers.size()),
            };

            auto vk_cmd_buffers = inplace_vector<VkCommandBuffer, max_command_lists>{};

            result = dispatch_table.allocateCommandBuffers(&command_buffer_allocate_info, vk_cmd_buffers.data());
            TEMPEST_ASSERT(result == VK_SUCCESS);

            for (size_t i = 0; i < vk_cmd_buffers.size(); ++i)
            {
                _command_buffers.emplace_back(vk_cmd_buffers[i], dispatch_table, device);
            }
        }

        command_list_slab(const command_list_slab&) = delete;
        command_list_slab(command_list_slab&& rhs) noexcept = delete;

        ~command_list_slab()
        {
            if (_command_pool == VK_NULL_HANDLE)
            {
                return;
            }

            auto vk_cmd_buffers = inplace_vector<VkCommandBuffer, max_command_lists>{};
            for (const auto& cmd_list : _command_buffers)
            {
                vk_cmd_buffers.emplace_back(cmd_list);
            }

            _dispatch_table->freeCommandBuffers(_command_pool, static_cast<uint32_t>(vk_cmd_buffers.size()),
                                                vk_cmd_buffers.data());
            _dispatch_table->destroyCommandPool(_command_pool, nullptr);
        }

        // NOLINTNEXTLINE(modernize-use-trailing-return-type)
        command_list_slab& operator=(const command_list_slab&) = delete;

        // NOLINTNEXTLINE(modernize-use-trailing-return-type)
        command_list_slab& operator=(command_list_slab&& rhs) noexcept = delete;

        /**
         * @brief Acquires a command buffer from the slab. If the slab is full, returns VK_NULL_HANDLE.
         *
         * @param current_timeline_value The current timeline value of the queue.
         * @return A command buffer from the slab, or VK_NULL_HANDLE if the slab is full.
         * @see can_reset() to determine if the slab can be reset and reused.
         */
        [[nodiscard]] auto acquire_command_buffer(uint64_t current_timeline_value) -> VkCommandBuffer
        {
            if (_allocated_command_lists >= max_command_lists)
            {
                return VK_NULL_HANDLE;
            }

            return _command_buffers[_allocated_command_lists++];
        }

        /**
         * @brief Marks the slab as submitted to the queue. This is used to determine when the slab can be reused or
         * released.
         *
         * @param timeline_value The timeline value that was submitted to the queue for this slab.
         */
        auto mark_submitted(uint64_t timeline_value) -> void
        {
            _submitted_timeline_value = timeline_value;
        }

        /**
         * @brief Determines if the slab can be reset and reused. A slab can be reset if the submitted timeline value is
         * less than or equal to the current timeline value of the queue.
         *
         * @param current_timeline_value The current timeline value of the queue.
         * @return True if the slab can be reset and reused, false otherwise.
         */
        [[nodiscard]] auto can_reset(uint64_t current_timeline_value) const -> bool
        {
            return _submitted_timeline_value <= current_timeline_value;
        }

        /**
         * @brief Resets the slab, making all command buffers available for acquisition. This should only be called when
         * the slab can be reset, as determined by can_reset().
         *
         * @see can_reset(uint64_t) to determine if the slab can be reset and reused.
         */
        auto reset() -> void
        {
            _allocated_command_lists = 0;
            _dispatch_table->resetCommandPool(_command_pool, 0); // TODO: Heuristic for when to release resources?
        }

      private:
        inplace_vector<command_list, max_command_lists> _command_buffers;
        VkCommandPool _command_pool = VK_NULL_HANDLE;
        uint32_t _allocated_command_lists = 0;
        uint32_t _thread_id = thread_id_sentinel;
        uint64_t _submitted_timeline_value = 0;
        const vkb::DispatchTable* _dispatch_table;
    };

    /**
     * @brief A command list slab allocator is a collection of command list slabs that can be allocated from a single
     * execution port. It is used to manage the lifetime of command list slabs and to ensure that command list slabs are
     * not reused while they are still in use by the GPU. The allocator is associated with a specific queue family and
     * dispatch table, and can be reset when it is no longer in use by the GPU.
     *
     * @tparam SlabCount The maximum number of command list slabs that can be allocated from the allocator.
     * @tparam CommandListCount The maximum number of command buffers that can be allocated from each slab.
     * @note The command list slab allocator is not thread safe. It must be externally synchronized if accessed from
     * multiple threads.
     */
    template <size_t SlabCount, size_t CommandListCount>
    class command_list_slab_allocator final
    {
      public:
        static constexpr size_t slab_count = SlabCount;
        static constexpr size_t command_list_count = CommandListCount;

        /**
         * @brief Constructs a command list slab allocator with the specified dispatch table and queue family index.
         * Allocates a collection of command list slabs from the dispatch table.
         *
         * @param dispatch_table The dispatch table to use for Vulkan function calls.
         * @param device The Vulkan device to use for command pool and command buffer allocation.
         * @param queue_family_index The index of the queue family to use for the command list slabs.
         */
        command_list_slab_allocator(const vkb::DispatchTable& dispatch_table, const vk::device& device,
                                    uint32_t queue_family_index)
        {
            for (size_t i = 0; i < slab_count; ++i)
            {
                slabs.emplace_back(dispatch_table, device, queue_family_index);
            }
        }

        command_list_slab_allocator(const command_list_slab_allocator&) = delete;
        command_list_slab_allocator(command_list_slab_allocator&&) noexcept = default;
        ~command_list_slab_allocator() = default;

        // NOLINTNEXTLINE(modernize-use-trailing-return-type)
        command_list_slab_allocator& operator=(const command_list_slab_allocator&) = delete;
        // NOLINTNEXTLINE(modernize-use-trailing-return-type)
        command_list_slab_allocator& operator=(command_list_slab_allocator&&) noexcept = default;

        /**
         * @brief Acquires a command list from the allocator. If all slabs are full, this will wait for a slab to become
         * available. If no slabs are available within the specified timeout, this will return a VK_NULL_HANDLE.
         */
        [[nodiscard]] auto acquire_command_list(uint64_t timeline_value, vkb::DispatchTable& dispatch, uint64_t timeout)
            -> VkCommandBuffer&;

      private:
        inplace_vector<command_list_slab<command_list_count>, slab_count> slabs;
        uint32_t current_slab_index = 0;
        const vkb::DispatchTable* _dispatch_table;
    };

    // Not thread safe, must be externally synchronized
    struct combined_command_list_slab_allocator
    {
        static constexpr size_t transient_slab_count = 16;
        static constexpr size_t transient_command_list_count = 16;

        static constexpr size_t persistent_slab_count = 4;
        static constexpr size_t persistent_command_list_count = 8;

        command_list_slab_allocator<transient_slab_count, transient_command_list_count> transient_allocator;
        command_list_slab_allocator<persistent_slab_count, persistent_command_list_count> persistent_allocator;
    };

    // Thread safe assuming that the thread_id is unique per thread
    class TEMPEST_API execution_port final : public rhi::execution_port
    {
      public:
        execution_port(const execution_port&) = delete;
        execution_port(execution_port&&) noexcept = delete;
        ~execution_port() override;

        execution_port& operator=(const execution_port&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        execution_port& operator=(execution_port&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        auto wait_idle() -> void override;

        [[nodiscard]] auto acquire_command_list(uint32_t thread_id = 0,
                                                command_list_lifetime lifetime = command_list_lifetime::transient)
            -> rhi::command_list& override;
        [[nodiscard]] auto submit(span<const rhi::command_list*> commands,
                                  span<const device_sync_point> wait_semaphores,
                                  span<const device_sync_point> signal_semaphores)
            -> expected<void, submit_error> override;

        [[nodiscard]] auto get_queue_family_index() const -> uint32_t
        {
            return _queue_family_index;
        }

      private:
        execution_port(vkb::Device& parent_device, uint32_t queue_family_index, vkb::QueueType queue_type,
                       vkb::DispatchTable dispatch_table);

        uint32_t _queue_family_index;
        vkb::QueueType _queue_type;
        vkb::Device* _parent_device{nullptr};
        vkb::DispatchTable _dispatch_table;

        // Slab allocators for command lists, one per thread
        vector<combined_command_list_slab_allocator> _slab_allocators;
        mutex _slab_allocator_mutex;

        // Ensures that only one thread can submit command lists at a time, since Vulkan queues are not thread-safe
        mutex _submit_mutex;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_execution_port_hpp
