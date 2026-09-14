#ifndef tempest_core_intrusive_stack_hpp
#define tempest_core_intrusive_stack_hpp

#include <tempest/assert.hpp>
#include <tempest/atomic.hpp>
#include <tempest/checked.hpp>

namespace tempest
{
    template <typename T>
    struct treiber_node
    {
        T* next{nullptr};
    };

    /// @brief Generic intrusive lock-free multi-producer, single-consumer (MPSC) stack.
    /// @tparam T The element type stored in the stack.
    /// @tparam Hook Optional member pointer to a treiber_node<T> within T. If nullptr, T must derive from treiber_node<T>.
    template <typename T, treiber_node<T> T::* Hook = nullptr>
    class intrusive_mpsc_stack
    {
      public:
        intrusive_mpsc_stack() noexcept = default;
        ~intrusive_mpsc_stack() = default;

        intrusive_mpsc_stack(const intrusive_mpsc_stack&) = delete;
        intrusive_mpsc_stack(intrusive_mpsc_stack&&) = delete;
        auto operator=(const intrusive_mpsc_stack&) -> intrusive_mpsc_stack& = delete;
        auto operator=(intrusive_mpsc_stack&&) -> intrusive_mpsc_stack& = delete;

        /// @brief Pushes a single node to the top of the stack.
        /// @param node Non-null pointer to the node being pushed.
        auto push(non_null<T> node) noexcept -> void
        {
            _get_node(node.get()).next = nullptr;
            push_range(node, node);
        }

        /// @brief Pushes a pre-linked range of nodes [first, last] to the top of the stack in a single atomic CAS.
        /// @param first Non-null pointer to the first node of the sub-chain.
        /// @param last Non-null pointer to the last node of the sub-chain.
        auto push_range(non_null<T> first, non_null<T> last) noexcept -> void
        {
            auto* const first_node = first.get();
            auto* const last_node = last.get();
            TEMPEST_ASSERT(_get_node(last_node).next == nullptr);
            auto* old_head = _head.load(memory_order::relaxed);
            do
            {
                _get_node(last_node).next = old_head;
            } while (!_head.compare_exchange_weak(old_head, first_node, memory_order::release, memory_order::relaxed));
        }

        /// @brief Atomically drains the entire stack, resetting the head pointer to nullptr.
        /// @return Pointer to the former top node of the stack (head of the LIFO chain), or nullptr if empty.
        [[nodiscard]] auto drain() noexcept -> T*
        {
            return _head.exchange(nullptr, memory_order::acq_rel);
        }

        /// @brief Atomically exchanges the head pointer with a new value.
        /// @param desired Pointer to set as new head.
        /// @param mo Memory order for exchange.
        /// @return Former top node of the stack.
        auto exchange(T* desired, memory_order mo = memory_order::acq_rel) noexcept -> T*
        {
            return _head.exchange(desired, mo);
        }

        /// @brief Checks whether the stack is currently empty.
        /// @param mo Memory order for loading the head pointer.
        /// @return True if empty, false otherwise.
        [[nodiscard]] auto empty(memory_order mo = memory_order::relaxed) const noexcept -> bool
        {
            return _head.load(mo) == nullptr;
        }

        /// @brief Loads the head pointer.
        /// @param mo Memory order for loading the head pointer.
        /// @return Current head pointer.
        [[nodiscard]] auto load(memory_order mo = memory_order::acquire) const noexcept -> T*
        {
            return _head.load(mo);
        }

        /// @brief Stores a new head pointer.
        /// @param desired New head pointer.
        /// @param mo Memory order for store.
        auto store(T* desired, memory_order mo = memory_order::release) noexcept -> void
        {
            _head.store(desired, mo);
        }

        /// @brief Compares and exchanges the head pointer weakly.
        auto compare_exchange_weak(T*& expected, T* desired, memory_order success, memory_order failure) noexcept -> bool
        {
            return _head.compare_exchange_weak(expected, desired, success, failure);
        }

        /// @brief Compares and exchanges the head pointer strongly.
        auto compare_exchange_strong(T*& expected, T* desired, memory_order success, memory_order failure) noexcept -> bool
        {
            return _head.compare_exchange_strong(expected, desired, success, failure);
        }

        /// @brief Reverses a singly linked Treiber chain in-place.
        /// @param head Pointer to the head of the chain to reverse.
        /// @return Pointer to the new head of the reversed chain.
        [[nodiscard]] static auto reverse(T* head) noexcept -> T*
        {
            auto* previous_node = static_cast<T*>(nullptr);
            auto* current_node = head;
            while (current_node != nullptr)
            {
                auto* const next_node = _get_node(current_node).next;
                _get_node(current_node).next = previous_node;
                previous_node = current_node;
                current_node = next_node;
            }
            return previous_node;
        }

        /// @brief Retrieves the next node in the intrusive chain.
        /// @param item Pointer to the node whose successor to retrieve.
        /// @return Pointer to the successor node, or nullptr if item is null or at the tail.
        [[nodiscard]] static auto next(T* item) noexcept -> T*
        {
            if (item == nullptr)
            {
                return nullptr;
            }
            return _get_node(item).next;
        }

      private:
        [[nodiscard]] static constexpr auto _get_node(T* item) noexcept -> treiber_node<T>&
        {
            TEMPEST_ASSERT(item != nullptr);
            if constexpr (Hook != nullptr)
            {
                return item->*Hook;
            }
            else
            {
                return static_cast<treiber_node<T>&>(*item);
            }
        }

        atomic<T*> _head{nullptr};
    };
} // namespace tempest

#endif // tempest_core_intrusive_stack_hpp
