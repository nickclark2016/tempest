#ifndef tempest_core_threading_dependency_graph
#define tempest_core_threading_dependency_graph

namespace tempest::core::threading
{
    class scheduler;
    class task_node;

    class dependency_graph
    {
      public:
        dependency_graph() = default;
        dependency_graph(const dependency_graph&) = delete;
        dependency_graph(dependency_graph&& other) noexcept;
        ~dependency_graph();

        dependency_graph& operator=(const dependency_graph&) = delete;
        dependency_graph& operator=(dependency_graph&& rhs) noexcept = delete;
    };

    class task_node
    {
    };
}

#endif // tempest_core_threading_dependency_graph