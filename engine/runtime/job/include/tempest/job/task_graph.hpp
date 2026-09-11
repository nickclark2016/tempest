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



    class task_node;

    struct node_invoker
    {
        virtual ~node_invoker() = default;
        virtual auto execute(task_node& node) -> task<void> = 0;
    };

    class TEMPEST_API task_node
    {
      public:
        friend class task_graph;
        friend class job_system;
        template <typename F>
        friend struct typed_node_invoker;

        task_node(string_view name, size_t id, task_graph* graph) noexcept;
        ~task_node() = default;

        task_node(const task_node&) = delete;
        task_node& operator=(const task_node&) = delete;
        task_node(task_node&&) = delete;
        task_node& operator=(task_node&&) = delete;

        template <typename... Nodes>
        auto precede(task_node& first, Nodes&... rest) -> task_node&
        {
            task_node* nodes[] = {&first, (&rest)...};
            for (auto* n : nodes)
            {
                _add_successor(n);
            }
            return *this;
        }

        auto precede(span<const non_null<task_node>> nodes) -> task_node&;
        auto precede(span<task_node*> nodes) -> task_node&;

        template <typename... Nodes>
        auto succeed(task_node& first, Nodes&... rest) -> task_node&
        {
            task_node* nodes[] = {&first, (&rest)...};
            for (auto* n : nodes)
            {
                n->_add_successor(this);
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

        string_view _name{};
        size_t _id{0};
        task_graph* _graph{nullptr};

        vector<task_node*> _successors{};
        vector<task_node*> _predecessors{};
        size_t _static_in_degree{0};

        atomic<size_t> _runtime_in_degree{0};
        atomic<bool> _failed{false};
        expected<void, job_error> _result{expected<void, job_error>{}};

        unique_ptr<node_invoker> _invoker{};
    };

    template <typename F>
    struct typed_node_invoker : node_invoker
    {
        F callable;

        template <typename Fn>
        explicit typed_node_invoker(Fn&& fn) : callable{tempest::forward<Fn>(fn)}
        {
        }

        auto execute(task_node& node) -> task<void> override
        {
            using Ret = invoke_result_t<F&>;
            if constexpr (detail::is_task_v<Ret>)
            {
                auto t = callable();
                struct raw_task_awaiter
                {
                    Ret& child;
                    auto await_ready() const noexcept -> bool
                    {
                        return child.is_ready();
                    }
                    auto await_suspend(coroutine_handle<> h) noexcept -> coroutine_handle<>
                    {
                        child.handle().promise().continuation = h;
                        return child.handle();
                    }
                    auto await_resume() noexcept -> void
                    {
                    }
                };
                co_await raw_task_awaiter{t};
                if (!t.has_value())
                {
                    node._result = unexpected{static_cast<job_error>(t.error())};
                }
                else
                {
                    node._result = expected<void, job_error>{};
                }
            }
            else if constexpr (requires { callable().has_value(); callable().error(); })
            {
                auto res = callable();
                if (!res.has_value())
                {
                    node._result = unexpected{static_cast<job_error>(res.error())};
                }
                else
                {
                    node._result = expected<void, job_error>{};
                }
            }
            else
            {
                callable();
                node._result = expected<void, job_error>{};
            }
            co_return;
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
        vector<unique_ptr<task_node>> _nodes{};
    };
} // namespace tempest::job

#endif // tempest_job_task_graph_hpp
