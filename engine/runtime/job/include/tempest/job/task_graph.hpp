#ifndef tempest_job_task_graph_hpp
#define tempest_job_task_graph_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/checked.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/job/task.hpp>
#include <tempest/job/types.hpp>
#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::job
{
    class task_graph;
    class job_system;
    struct graph_executor;

    class task_node;

    struct node_invoker
    {
        node_invoker() = default;
        node_invoker(const node_invoker&) = delete;
        node_invoker(node_invoker&&) noexcept = delete;
        virtual ~node_invoker() = default;
        auto operator=(const node_invoker&) -> node_invoker& = delete;
        auto operator=(node_invoker&&) noexcept -> node_invoker& = delete;
        virtual auto execute() -> task<expected<void, job_error>, void> = 0;
    };

    class TEMPEST_API task_node
    {
      public:
        friend class task_graph;
        friend class job_system;
        friend struct graph_executor;

        task_node(string_view name, size_t tid, task_graph* graph) noexcept;
        task_node(const task_node&) = delete;
        task_node(task_node&&) noexcept = delete;
        ~task_node() = default;
        
        auto operator=(const task_node&) -> task_node& = delete;
        auto operator=(task_node&&) -> task_node& = delete;

        template <typename... Nodes>
        auto precede(task_node& first, Nodes&... rest) -> task_node&
        {
            auto const nodes = array{&first, (&rest)...};
            for (auto* node : nodes)
            {
                _add_successor(node);
            }
            return *this;
        }

        auto precede(span<const non_null<task_node>> nodes) -> task_node&;
        auto precede(span<task_node*> nodes) -> task_node&;

        template <typename... Nodes>
        auto succeed(task_node& first, Nodes&... rest) -> task_node&
        {
            auto nodes = array{&first, (&rest)...};
            for (auto* node : nodes)
            {
                node->_add_successor(this);
            }
            return *this;
        }

        auto succeed(span<const non_null<task_node>> nodes) -> task_node&;
        auto succeed(span<task_node*> nodes) -> task_node&;

        [[nodiscard]] auto name() const noexcept -> string_view;
        [[nodiscard]] auto id() const noexcept -> size_t;
        [[nodiscard]] auto in_degree() const noexcept -> size_t;
        [[nodiscard]] auto out_degree() const noexcept -> size_t;
        [[nodiscard]] auto result() const noexcept -> const expected<void, job_error>&
        {
            return _result;
        }

      private:
        auto _add_successor(task_node* succ) -> void;

        string_view _name;
        size_t _id = 0;
        task_graph* _graph = 0;

        vector<task_node*> _successors;
        vector<task_node*> _predecessors;
        size_t _static_in_degree = 0;

        atomic<size_t> _runtime_in_degree = 0;
        atomic<bool> _failed = false;
        expected<void, job_error> _result;

        unique_ptr<node_invoker> _invoker;
        task<void> _task;
    };

    template <typename F>
    struct typed_node_invoker : node_invoker
    {
        F callable;

        template <typename Fn>
        explicit typed_node_invoker(Fn&& func) : callable{tempest::forward<Fn>(func)}
        {
        }

        auto execute() -> task<expected<void, job_error>, void> override
        {
            using Ret = invoke_result_t<F&>;
            if constexpr (detail::is_task_v<Ret>)
            {
                auto coro_task = callable();
                struct raw_task_awaiter
                {
                    non_null<Ret> child;

                    [[nodiscard]] auto await_ready() const noexcept -> bool
                    {
                        return child->is_ready();
                    }

                    [[nodiscard]] auto await_suspend(coroutine_handle<task<expected<void, job_error>, void>::promise_type> hnd) noexcept -> coroutine_handle<>
                    {
                        child->handle().promise().continuation = hnd;
                        if (child->handle().promise().session == nullptr)
                        {
                            child->handle().promise().session = hnd.promise().session;
                        }
                        if (child->handle().promise().session != nullptr &&
                            child->handle().promise().session->is_enabled() &&
                            child->handle().promise().coroutine_id == 0)
                        {
                            child->handle().promise().coroutine_id =
                                child->handle().promise().session->allocate_coroutine_id();
                        }
                        child->handle().promise().awaited_by_thread_id = tempest::this_thread::get_id().to_uint64();
                        return child->handle();
                    }
                    auto await_resume() noexcept -> void
                    {
                    }
                };
                co_await raw_task_awaiter{coro_task};
                if (!coro_task.has_value())
                {
                    co_return unexpected{static_cast<job_error>(coro_task.error())};
                }
                co_return expected<void, job_error>{};
            }
            else if constexpr (requires { callable().has_value(); callable().error(); })
            {
                auto res = callable();
                if (!res.has_value())
                {
                    co_return unexpected{static_cast<job_error>(res.error())};
                }
                co_return expected<void, job_error>{};
            }
            else
            {
                callable();
                co_return expected<void, job_error>{};
            }
        }
    };

    class TEMPEST_API task_graph
    {
      public:
        task_graph() = default;
        ~task_graph() = default;

        task_graph(const task_graph&) = delete;
        task_graph& operator=(const task_graph&) = delete;
        task_graph(task_graph&&) noexcept = default;
        task_graph& operator=(task_graph&&) noexcept = default;

        template <typename F>
        auto emplace(string_view name, F&& callable) -> task_node&
        {
            auto node_id = _nodes.size();
            auto node = make_unique<task_node>(name, node_id, this);
            node->_invoker = make_unique<typed_node_invoker<decay_t<F>>>(tempest::forward<F>(callable));
            _nodes.push_back(tempest::move(node));
            return *_nodes.back();
        }

        template <typename F>
        auto emplace(F&& callable) -> task_node&
        {
            return emplace("", tempest::forward<F>(callable));
        }

        auto clear() -> void;

        [[nodiscard]] auto empty() const noexcept -> bool
        {
            return _nodes.empty();
        }

        [[nodiscard]] auto size() const noexcept -> size_t
        {
            return _nodes.size();
        }

        [[nodiscard]] auto nodes() noexcept -> vector<unique_ptr<task_node>>&
        {
            return _nodes;
        }

        [[nodiscard]] auto nodes() const noexcept -> const vector<unique_ptr<task_node>>&
        {
            return _nodes;
        }

      private:
        vector<unique_ptr<task_node>> _nodes;
    };

    /// @brief Result of executing a task_graph via linear move semantics.
    /// @details Holds the restored task_graph (guaranteed to be returned across both success
    ///          and error paths) alongside the overall execution status. Being an aggregate,
    ///          it directly supports C++ structured bindings: `auto [graph, status] = co_await ...`.
    struct TEMPEST_API task_graph_result
    {
        task_graph graph;
        expected<void, error_code> status;

        [[nodiscard]] auto has_value() const noexcept -> bool
        {
            return status.has_value();
        }

        [[nodiscard]] auto error() const noexcept -> error_code
        {
            return status.error();
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status.has_value();
        }
    };
} // namespace tempest::job

#endif // tempest_job_task_graph_hpp
