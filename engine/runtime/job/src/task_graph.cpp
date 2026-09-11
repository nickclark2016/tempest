#include <tempest/job/task_graph.hpp>

namespace tempest::job
{
    task_node::task_node(string_view name, size_t id, task_graph* graph) noexcept
        : _name{name}, _id{id}, _graph{graph}
    {
    }

    auto task_node::name() const noexcept -> string_view
    {
        return _name;
    }

    auto task_node::id() const noexcept -> size_t
    {
        return _id;
    }

    auto task_node::in_degree() const noexcept -> size_t
    {
        return _static_in_degree;
    }

    auto task_node::out_degree() const noexcept -> size_t
    {
        return _successors.size();
    }

    auto task_node::_add_successor(task_node* succ) -> void
    {
        if (succ == nullptr || succ == this)
        {
            return;
        }
        for (auto* s : _successors)
        {
            if (s == succ)
            {
                return;
            }
        }
        _successors.push_back(succ);
        succ->_predecessors.push_back(this);
        succ->_static_in_degree = succ->_predecessors.size();
    }

    auto task_node::precede(span<const non_null<task_node>> nodes) -> task_node&
    {
        for (const auto& n : nodes)
        {
            _add_successor(n.get());
        }
        return *this;
    }

    auto task_node::precede(span<task_node*> nodes) -> task_node&
    {
        for (auto* n : nodes)
        {
            _add_successor(n);
        }
        return *this;
    }

    auto task_node::succeed(span<const non_null<task_node>> nodes) -> task_node&
    {
        for (const auto& n : nodes)
        {
            n.get()->_add_successor(this);
        }
        return *this;
    }

    auto task_node::succeed(span<task_node*> nodes) -> task_node&
    {
        for (auto* n : nodes)
        {
            n->_add_successor(this);
        }
        return *this;
    }

    auto task_graph::clear() -> void
    {
        _nodes.clear();
    }
} // namespace tempest::job
