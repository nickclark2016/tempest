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
    class command_list;

    /**
     * @brief A command list slab is a collection of command buffers that can be allocated from a single command pool.
     * It is used to manage the lifetime of command buffers and to ensure that command buffers are not reused while they
     * are still in use by the GPU. The slab is associated with a specific queue family and dispatch table, and can be
     * reset when it is no longer in use by the GPU.
     *
     * @tparam N The maximum number of command buffers that can be allocated from the slab.
     */
    template <size_t N>
    struct command_list_slab
    {
        static constexpr size_t max_command_lists = N;
        static constexpr size_t thread_id_sentinel = numeric_limits<uint32_t>::max();

        /**
         * @brief Constructs a command list slab with the specified dispatch table and queue family index. Allocates a
         * command pool and command buffers from the dispatch table.
         *
         * @param dispatch_table The dispatch table to use for Vulkan function calls.
         * @param queue_family_index The index of the queue family to use for the command pool and command buffers.
         */
        command_list_slab(const vkb::DispatchTable& dispatch_table, uint32_t queue_family_index)
            : _dispatch_table{&dispatch_table}
        {
            auto command_pool_create_info = VkCommandPoolCreateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queue_family_index,
            };

            auto result = dispatch_table.createCommandPool(&command_pool_create_info, nullptr, &_command_pool);
            TEMPEST_ASSERT(result == VK_SUCCESS);

            auto command_buffer_allocate_info = VkCommandBufferAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = _command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(_command_buffers.size()),
            };

            result = dispatch_table.allocateCommandBuffers(&command_buffer_allocate_info, _command_buffers.data());
            TEMPEST_ASSERT(result == VK_SUCCESS);
        }

        command_list_slab(const command_list_slab&) = delete;

        /**
         * @brief Move constructor for command_list_slab. Transfers ownership of the command buffers and command pool
         * from the source slab to the new slab. The source slab is left in an empty state, with its command buffers and
         * command pool set to VK_NULL_HANDLE and its allocated command list count set to 0.
         *
         * @param rhs The source command_list_slab to move from.
         */
        command_list_slab(command_list_slab&& rhs) noexcept
            : _command_buffers{tempest::exchange(rhs._command_buffers, {})},
              _command_pool{tempest::exchange(rhs._command_pool, VK_NULL_HANDLE)},
              _allocated_command_lists{tempest::exchange(rhs._allocated_command_lists, 0)},
              _thread_id{tempest::exchange(rhs._thread_id, thread_id_sentinel)},
              _submitted_timeline_value{tempest::exchange(rhs._submitted_timeline_value, 0)},
              _dispatch_table{rhs._dispatch_table}
        {
        }

        ~command_list_slab()
        {
            if (_command_pool == VK_NULL_HANDLE)
            {
                return;
            }

            _dispatch_table->freeCommandBuffers(_command_pool, static_cast<uint32_t>(_command_buffers.size()),
                                                _command_buffers.data());
            _dispatch_table->destroyCommandPool(_command_pool, nullptr);
        }

        command_list_slab& operator=(const command_list_slab&) = delete; // NOLINT(modernize-use-trailing-return-type)

        /**
         * @brief Move assignment operator for command_list_slab. Transfers ownership of the command buffers and command
         * pool from the source slab to the destination slab. The source slab is left in an empty state, with its
         * command buffers and command pool set to VK_NULL_HANDLE and its allocated command list count set to 0.
         *
         * @param rhs The source command_list_slab to move from.
         * @return A reference to the destination command_list_slab.
         */
        command_list_slab& operator=(command_list_slab&& rhs) noexcept // NOLINT(modernize-use-trailing-return-type)
        {
            if (&rhs == this) {
                return *this;
            }

            if (_command_pool != VK_NULL_HANDLE)
            {
                _dispatch_table->freeCommandBuffers(_command_pool, static_cast<uint32_t>(_command_buffers.size()),
                                                    _command_buffers.data());
                _dispatch_table->destroyCommandPool(_command_pool, nullptr);
            }

            _command_buffers = tempest::exchange(rhs._command_buffers, {});
            _command_pool = tempest::exchange(rhs._command_pool, VK_NULL_HANDLE);
            _allocated_command_lists = tempest::exchange(rhs._allocated_command_lists, 0);
            _thread_id = tempest::exchange(rhs._thread_id, thread_id_sentinel);
            _submitted_timeline_value = tempest::exchange(rhs._submitted_timeline_value, 0);
            _dispatch_table = rhs._dispatch_table;

            return *this;
        }

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
        array<VkCommandBuffer, max_command_lists> _command_buffers;
        VkCommandPool _command_pool = VK_NULL_HANDLE;
        uint32_t _allocated_command_lists = 0;
        uint32_t _thread_id = thread_id_sentinel;
        uint64_t _submitted_timeline_value = 0;
        const vkb::DispatchTable* _dispatch_table;
    };

    // Not thread safe, must be externally synchronized
    template <size_t SlabCount, size_t CommandListCount>
    struct command_list_slab_allocator
    {
        static constexpr size_t slab_count = SlabCount;
        static constexpr size_t command_list_count = CommandListCount;

        // Circular buffer of command list slabs
        inplace_vector<command_list_slab<command_list_count>, slab_count> slabs;
        uint32_t current_slab_index = 0;

        [[nodiscard]] auto acquire_command_list(uint64_t timeline_value, vkb::DispatchTable& dispatch, uint64_t timeout)
            -> command_list&;
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
