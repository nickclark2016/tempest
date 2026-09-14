#ifndef tempest_job_types_hpp
#define tempest_job_types_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>

namespace tempest::job
{
    enum class task_priority : uint8_t
    {
        critical = 0,
        high,
        normal,
        low,
        count
    };

    enum class core_class : uint8_t
    {
        any = 0,
        performance,
        efficiency,
        custom_mask
    };

    enum class job_error : uint8_t
    {
        none = 0,
        canceled,
        channel_closed,
        channel_empty,
        channel_full,
        timeout,
        task_failed,
    };

    enum class wait_state : uint8_t
    {
        pending = 0,
        completed,
        cancelled,
        retired,
    };

    using error_code = job_error;

    struct task_affinity
    {
        core_class target_class{core_class::any};
        uint64_t explicit_mask{0};
    };

    struct cancellation_node
    {
        void (*callback)(void* user_data){nullptr};
        void* user_data{nullptr};
        cancellation_node* next{nullptr};
    };

    class cancellation_state
    {
      public:
        cancellation_state() = default;
        cancellation_state(const cancellation_state&) = delete;
        cancellation_state(cancellation_state&&) noexcept = delete;

        ~cancellation_state() = default;

        auto operator=(const cancellation_state&) -> cancellation_state& = delete;
        auto operator=(cancellation_state&&) noexcept -> cancellation_state& = delete;

        [[nodiscard]] auto is_cancellation_requested() const noexcept -> bool
        {
            return _canceled.load(memory_order::acquire);
        }

        auto request_cancellation() noexcept -> void
        {
            if (_canceled.exchange(true, memory_order::acq_rel))
            {
                return;
            }

            auto* curr = _callbacks.exchange(nullptr, memory_order::acq_rel);
            while (curr != nullptr)
            {
                auto* next = curr->next;
                if (curr->callback != nullptr)
                {
                    curr->callback(curr->user_data);
                }
                curr = next;
            }
        }

        auto register_callback(cancellation_node* node) noexcept -> bool
        {
            if (is_cancellation_requested())
            {
                if (node->callback != nullptr)
                {
                    node->callback(node->user_data);
                }
                return false;
            }

            auto* old_head = _callbacks.load(memory_order::acquire);
            do
            {
                node->next = old_head;
                if (is_cancellation_requested())
                {
                    if (node->callback != nullptr)
                    {
                        node->callback(node->user_data);
                    }
                    return false;
                }
            } while (!_callbacks.compare_exchange_weak(old_head, node, memory_order::release));

            return true;
        }

      private:
        atomic<bool> _canceled{false};
        atomic<cancellation_node*> _callbacks{nullptr};
    };

    class cancellation_token
    {
      public:
        constexpr cancellation_token() noexcept = default;
        explicit cancellation_token(cancellation_state* state) noexcept : _state{state}
        {
        }

        [[nodiscard]] auto is_cancellation_requested() const noexcept -> bool
        {
            return _state != nullptr && _state->is_cancellation_requested();
        }

        [[nodiscard]] auto can_be_canceled() const noexcept -> bool
        {
            return _state != nullptr;
        }

        auto register_callback(cancellation_node* node) const noexcept -> bool
        {
            if (_state != nullptr)
            {
                return _state->register_callback(node);
            }
            return false;
        }

        [[nodiscard]] auto state() const noexcept -> cancellation_state*
        {
            return _state;
        }

      private:
        cancellation_state* _state{nullptr};
    };

    class cancellation_source
    {
      public:
        cancellation_source() : _state{make_unique<cancellation_state>()}
        {
        }
        ~cancellation_source() = default;

        cancellation_source(const cancellation_source&) = delete;
        cancellation_source& operator=(const cancellation_source&) = delete;
        cancellation_source(cancellation_source&& other) noexcept = default;
        cancellation_source& operator=(cancellation_source&& other) noexcept = default;

        auto request_cancellation() noexcept -> void
        {
            if (_state)
            {
                _state->request_cancellation();
            }
        }

        [[nodiscard]] auto is_cancellation_requested() const noexcept -> bool
        {
            return _state && _state->is_cancellation_requested();
        }

        [[nodiscard]] auto get_token() noexcept -> cancellation_token
        {
            return cancellation_token{_state.get()};
        }

        [[nodiscard]] auto get_token() const noexcept -> cancellation_token
        {
            return cancellation_token{_state.get()};
        }

      private:
        unique_ptr<cancellation_state> _state;
    };
} // namespace tempest::job

#endif // tempest_job_types_hpp
