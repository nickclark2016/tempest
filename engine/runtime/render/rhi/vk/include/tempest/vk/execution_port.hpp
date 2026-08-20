#ifndef tempest_rhi_vk_execution_port_hpp
#define tempest_rhi_vk_execution_port_hpp

#include <tempest/array.hpp>
#include <tempest/assert.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/limits.hpp>
#include <tempest/memory.hpp>
#include <tempest/mutex.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vector.hpp>

#include <VkBootstrap.h>
#include <VkBootstrapDispatch.h>

namespace tempest::rhi::vk
{
    class execution_port;
    class device;
    class render_surface;

    class TEMPEST_API command_list final : public rhi::command_list
    {
      public:
        command_list(VkCommandBuffer command_buffer, const vkb::DispatchTable& dispatch_table,
                     const vk::device& device, vkb::QueueType queue_type = vkb::QueueType::graphics) noexcept;
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

        // Debug markers and regions
        auto begin_debug_region(const debug_label& label) -> void override;
        auto end_debug_region() -> void override;
        auto insert_debug_marker(const debug_label& label) -> void override;

        [[nodiscard]] operator VkCommandBuffer() const noexcept
        {
            return _command_buffer;
        }

        [[nodiscard]] auto get_handle() const noexcept -> VkCommandBuffer
        {
            return _command_buffer;
        }

      private:
        friend class execution_port;

        VkCommandBuffer _command_buffer;
        const vkb::DispatchTable* _dispatch_table;
        const vk::device* _parent_device;
        vkb::QueueType _queue_type = vkb::QueueType::graphics;
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
         * @param queue_type The queue type (graphics, compute, transfer).
         */
        command_list_slab(const vkb::DispatchTable& dispatch_table, const vk::device& device,
                          uint32_t queue_family_index, vkb::QueueType queue_type = vkb::QueueType::graphics)
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

            auto vk_cmd_buffers = array<VkCommandBuffer, max_command_lists>{};

            auto command_buffer_allocate_info = VkCommandBufferAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = _command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(max_command_lists),
            };

            result = dispatch_table.allocateCommandBuffers(&command_buffer_allocate_info, vk_cmd_buffers.data());
            TEMPEST_ASSERT(result == VK_SUCCESS);

#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
            if (dispatch_table.fp_vkSetDebugUtilsObjectNameEXT != nullptr)
            {
                const auto pool_name_info = VkDebugUtilsObjectNameInfoEXT{
                    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                    .pNext = nullptr,
                    .objectType = VK_OBJECT_TYPE_COMMAND_POOL,
                    .objectHandle = reinterpret_cast<uint64_t>(_command_pool),
                    .pObjectName = "Command Pool",
                };
                dispatch_table.setDebugUtilsObjectNameEXT(&pool_name_info);
            }
#endif

            for (size_t i = 0; i < max_command_lists; ++i)
            {
#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
                if (dispatch_table.fp_vkSetDebugUtilsObjectNameEXT != nullptr)
                {
                    const auto buf_name_info = VkDebugUtilsObjectNameInfoEXT{
                        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                        .pNext = nullptr,
                        .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
                        .objectHandle = reinterpret_cast<uint64_t>(vk_cmd_buffers[i]),
                        .pObjectName = "Command Buffer",
                    };
                    dispatch_table.setDebugUtilsObjectNameEXT(&buf_name_info);
                }
#endif
                _command_buffers.emplace_back(vk_cmd_buffers[i], dispatch_table, device, queue_type);
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

        [[nodiscard]] auto has_available() const noexcept -> bool
        {
            return _allocated_command_lists < max_command_lists;
        }

        /**
         * @brief Acquires a command list from the slab.
         *
         * @return A command list from the slab.
         */
        [[nodiscard]] auto acquire_command_list() -> command_list&
        {
            TEMPEST_ASSERT(_allocated_command_lists < max_command_lists);
            return _command_buffers[_allocated_command_lists++];
        }

        /**
         * @brief Marks the slab as submitted to the queue. This is used to determine when the slab can be reused or
         * released.
         *
         * @param timeline_value The timeline value that was submitted to the queue for this slab.
         */
        auto mark_submitted(uint64_t timeline_value) noexcept -> void
        {
            _submitted_timeline_value = timeline_value;
        }

        [[nodiscard]] auto get_submitted_timeline_value() const noexcept -> uint64_t
        {
            return _submitted_timeline_value;
        }

        /**
         * @brief Determines if the slab can be reset and reused. A slab can be reset if the submitted timeline value is
         * less than or equal to the current timeline value of the queue.
         *
         * @param current_timeline_value The current timeline value of the queue.
         * @return True if the slab can be reset and reused, false otherwise.
         */
        [[nodiscard]] auto can_reset(uint64_t current_timeline_value) const noexcept -> bool
        {
            return _submitted_timeline_value <= current_timeline_value;
        }

        /**
         * @brief Resets the slab, making all command buffers available for acquisition. This should only be called when
         * the slab can be reset, as determined by can_reset().
         */
        auto reset() -> void
        {
            _allocated_command_lists = 0;
            _dispatch_table->resetCommandPool(_command_pool, 0);
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
                                    uint32_t queue_family_index, vkb::QueueType queue_type = vkb::QueueType::graphics)
            : _dispatch_table{&dispatch_table}
        {
            for (size_t i = 0; i < slab_count; ++i)
            {
                slabs.emplace_back(dispatch_table, device, queue_family_index, queue_type);
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
         * available.
         */
        [[nodiscard]] auto acquire_command_list(uint64_t current_timeline_value, VkSemaphore timeline_sem_vk)
            -> command_list&
        {
            // First check if the current slab has capacity
            if (slabs[current_slab_index].has_available())
            {
                return slabs[current_slab_index].acquire_command_list();
            }

            // Otherwise search for a recyclable slab
            for (size_t i = 0; i < slab_count; ++i)
            {
                auto next_index = (current_slab_index + 1 + i) % slab_count;
                if (slabs[next_index].can_reset(current_timeline_value))
                {
                    current_slab_index = static_cast<uint32_t>(next_index);
                    slabs[current_slab_index].reset();
                    return slabs[current_slab_index].acquire_command_list();
                }
            }

            // All slabs are currently in flight! Wait for the oldest slab to finish.
            auto next_index = (current_slab_index + 1) % slab_count;
            auto wait_val = slabs[next_index].get_submitted_timeline_value();
            if (wait_val > current_timeline_value && timeline_sem_vk != VK_NULL_HANDLE)
            {
                auto timeline_info = VkSemaphoreWaitInfo{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .semaphoreCount = 1,
                    .pSemaphores = &timeline_sem_vk,
                    .pValues = &wait_val,
                };
                _dispatch_table->waitSemaphores(&timeline_info, UINT64_MAX);
            }

            current_slab_index = static_cast<uint32_t>(next_index);
            slabs[current_slab_index].reset();
            return slabs[current_slab_index].acquire_command_list();
        }

        auto mark_current_slab_submitted(uint64_t timeline_value) noexcept -> void
        {
            slabs[current_slab_index].mark_submitted(timeline_value);
        }

      private:
        inplace_vector<command_list_slab<command_list_count>, slab_count> slabs;
        uint32_t current_slab_index = 0;
        const vkb::DispatchTable* _dispatch_table = nullptr;
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

        combined_command_list_slab_allocator(const vkb::DispatchTable& dispatch_table, const vk::device& device,
                                             uint32_t queue_family_index, vkb::QueueType queue_type = vkb::QueueType::graphics)
            : transient_allocator(dispatch_table, device, queue_family_index, queue_type),
              persistent_allocator(dispatch_table, device, queue_family_index, queue_type)
        {
        }
    };

    // Thread safe assuming that the thread_id is unique per thread
    class TEMPEST_API execution_port final : public rhi::execution_port
    {
      public:
        execution_port(vk::device& parent_device, uint32_t queue_family_index, vkb::QueueType queue_type,
                       VkQueue queue, vkb::DispatchTable dispatch_table);
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

        // Debug markers and regions
        auto begin_debug_region(const debug_label& label) -> void override;
        auto end_debug_region() -> void override;
        auto insert_debug_marker(const debug_label& label) -> void override;

        [[nodiscard]] auto get_timeline_sync_point() const noexcept -> host_sync_point override
        {
            return host_sync_point{
                .semaphore = _timeline_semaphore,
                .value = _timeline_value,
            };
        }

        [[nodiscard]] auto get_queue_family_index() const noexcept -> uint32_t
        {
            return _queue_family_index;
        }

        [[nodiscard]] auto get_queue() const noexcept -> VkQueue
        {
            return _queue;
        }

        [[nodiscard]] auto get_timeline_semaphore() const noexcept -> semaphore_handle
        {
            return _timeline_semaphore;
        }

        [[nodiscard]] auto get_current_timeline_value() const noexcept -> uint64_t
        {
            return _timeline_value;
        }

        [[nodiscard]] auto get_submit_mutex() noexcept -> mutex&
        {
            return _submit_mutex;
        }

      private:
        friend class render_surface;

        vk::device* _parent_device{nullptr};
        uint32_t _queue_family_index;
        vkb::QueueType _queue_type;
        VkQueue _queue{VK_NULL_HANDLE};
        vkb::DispatchTable _dispatch_table;

        semaphore_handle _timeline_semaphore{};
        uint64_t _timeline_value{0};

        // Slab allocators for command lists, one per thread
        vector<unique_ptr<combined_command_list_slab_allocator>> _slab_allocators;
        mutex _slab_allocator_mutex;

        // Ensures that only one thread can submit command lists at a time, since Vulkan queues are not thread-safe
        mutex _submit_mutex;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_execution_port_hpp
