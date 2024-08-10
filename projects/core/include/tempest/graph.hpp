#ifndef tempest_core_graph_hpp
#define tempest_core_graph_hpp

#include <tempest/slot_map.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest
{
    template <typename T>
    concept graph = requires(T t) {
        typename T::vertex;
        typename T::edge;
        typename T::weight;

        typename T::size_type;
        typename T::difference_type;
        typename T::vertex_key;

        typename T::iterator;
        typename T::const_iterator;

        typename T::edge_type;
        typename T::vertex_type;

        { t.vertex_count() } -> same_as<typename T::size_type>;
        { t.edge_count() } -> same_as<typename T::size_type>;

        // Add vertex
        { t.add_vertex(tempest::declval<const typename T::vertex&>()) } -> same_as<typename T::vertex_key>;
        { t.add_vertex(tempest::declval<typename T::vertex&&>()) } -> same_as<typename T::vertex_key>;

        // Remove vertex
        { t.remove_vertex(tempest::declval<typename T::vertex_key>()) } -> same_as<void>;

        // Add edge
        {
            t.add_edge(tempest::declval<typename T::vertex_key>(), tempest::declval<typename T::vertex_key>(),
                       tempest::declval<const typename T::edge&>(), tempest::declval<const typename T::weight&>())
        } -> same_as<void>;

        // Remove edge
        {
            t.remove_edge(tempest::declval<typename T::vertex_key>(), tempest::declval<typename T::vertex_key>())
        } -> same_as<void>;

        // Iteration
        { t.begin() } -> same_as<typename T::iterator>;
        { tempest::add_const_t<T>(t).begin() } -> same_as<typename T::const_iterator>;
        { t.cbegin() } -> same_as<typename T::const_iterator>;

        { t.end() } -> same_as<typename T::iterator>;
        { tempest::add_const_t<T>(t).end() } -> same_as<typename T::const_iterator>;
        { t.cend() } -> same_as<typename T::const_iterator>;

        // Find
        { t.find(tempest::declval<typename T::vertex_key>()) } -> same_as<typename T::iterator>;
        {
            tempest::add_const_t<T>(t).find(tempest::declval<typename T::vertex_key>())
        } -> same_as<typename T::const_iterator>;

        // Edge iteration
        {
            t.outgoing_edges(tempest::declval<typename T::vertex_key>())
        } -> same_as<tempest::span<const typename T::edge_type>>;
        {
            t.incoming_edges(tempest::declval<typename T::vertex_key>())
        } -> same_as<tempest::span<const typename T::edge_type>>;
    };

    template <typename V, typename E, typename W>
    class directed_graph
    {
      public:
        struct vertex_type;

        struct edge_type
        {
            E data;
            W weight;

            slot_map<vertex_type>::key_type source;
            slot_map<vertex_type>::key_type target;
        };

        struct vertex_type
        {
            V data;

            vector<edge_type> outgoing_edges;
            vector<edge_type> incoming_edges;
        };

        using vertex = V;
        using edge = E;
        using weight = W;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        using iterator = typename slot_map<vertex_type>::iterator;
        using const_iterator = typename slot_map<vertex_type>::const_iterator;
        using vertex_key = typename slot_map<vertex_type>::key_type;

        directed_graph() = default;
        directed_graph(const directed_graph&) = default;
        directed_graph(directed_graph&&) noexcept = default;
        ~directed_graph() = default;
        directed_graph& operator=(const directed_graph&) = default;
        directed_graph& operator=(directed_graph&&) noexcept = default;

        [[nodiscard]] size_type vertex_count() const noexcept;
        [[nodiscard]] size_type edge_count() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] vertex_key add_vertex(const vertex& v);
        [[nodiscard]] vertex_key add_vertex(vertex&& v);
        void remove_vertex(vertex_key key);

        void add_edge(vertex_key source, vertex_key target, const edge& e, const weight& w = weight{});
        void remove_edge(vertex_key source, vertex_key target);

        [[nodiscard]] iterator begin() noexcept;
        [[nodiscard]] const_iterator begin() const noexcept;
        [[nodiscard]] const_iterator cbegin() const noexcept;

        [[nodiscard]] iterator end() noexcept;
        [[nodiscard]] const_iterator end() const noexcept;
        [[nodiscard]] const_iterator cend() const noexcept;

        [[nodiscard]] iterator find(vertex_key key) noexcept;
        [[nodiscard]] const_iterator find(vertex_key key) const noexcept;

        [[nodiscard]] span<const edge_type> outgoing_edges(vertex_key key) const noexcept;
        [[nodiscard]] span<const edge_type> incoming_edges(vertex_key key) const noexcept;

        void swap(directed_graph& other) noexcept;

      private:
        slot_map<vertex_type> _vertices;
        size_type _edge_count{0};
    };

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::vertex_count() const noexcept -> size_type
    {
        return _vertices.size();
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::edge_count() const noexcept -> size_type
    {
        return _edge_count;
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::empty() const noexcept -> bool
    {
        return _vertices.empty();
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::add_vertex(const vertex& v) -> vertex_key
    {
        return _vertices.insert({v});
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::add_vertex(vertex&& v) -> vertex_key
    {
        return _vertices.insert({tempest::move(v)});
    }

    template <typename V, typename E, typename W>
    void directed_graph<V, E, W>::remove_vertex(vertex_key key)
    {
        const auto vertex_it = _vertices.find(key);
        if (vertex_it == _vertices.end())
        {
            return;
        }

        // Delete the incoming edge information from the target vertices
        for (const edge_type& edge : vertex_it->outgoing_edges)
        {
            const auto target_key = edge.target;
            auto target_it = _vertices.find(target_key);
            if (target_it != _vertices.end()) [[likely]]
            {
                auto& incoming_edges = target_it->incoming_edges;
                const auto edge_count = incoming_edges.size();
                tempest::erase_if(incoming_edges, [&](const edge_type& e) { return e.source == key; });
                const auto edges_removed = edge_count - incoming_edges.size();
                _edge_count -= edges_removed;
            }
        }

        // Delete the outgoing edge information from the source vertices
        for (const edge_type& edge : vertex_it->incoming_edges)
        {
            auto source_key = edge.source;
            auto source_it = _vertices.find(source_key);
            if (source_it != _vertices.end()) [[likely]]
            {
                auto& outgoing_edges = source_it->outgoing_edges;
                const auto edge_count = outgoing_edges.size();
                tempest::erase_if(outgoing_edges, [&](const edge_type& e) { return e.target == key; });
                const auto edges_removed = edge_count - outgoing_edges.size();
                _edge_count -= edges_removed;
            }
        }

        _vertices.erase(key);
    }

    template <typename V, typename E, typename W>
    void directed_graph<V, E, W>::add_edge(vertex_key source, vertex_key target, const edge& e, const weight& w)
    {
        auto source_it = _vertices.find(source);
        auto target_it = _vertices.find(target);

        if (source_it == _vertices.end() || target_it == _vertices.end())
        {
            return;
        }

        source_it->outgoing_edges.push_back({e, w, source, target});
        target_it->incoming_edges.push_back({e, w, source, target});
        ++_edge_count;
    }

    template <typename V, typename E, typename W>
    void directed_graph<V, E, W>::remove_edge(vertex_key source, vertex_key target)
    {
        auto source_it = _vertices.find(source);
        auto target_it = _vertices.find(target);

        if (source_it == _vertices.end() || target_it == _vertices.end())
        {
            return;
        }

        auto& outgoing_edges = source_it->outgoing_edges;
        auto& incoming_edges = target_it->incoming_edges;

        outgoing_edges.erase_if([&](const edge_type& e) { return e.target == target; });
        incoming_edges.erase_if([&](const edge_type& e) { return e.source == source; });
        --_edge_count;
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::begin() noexcept -> iterator
    {
        return _vertices.begin();
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::begin() const noexcept -> const_iterator
    {
        return _vertices.begin();
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::cbegin() const noexcept -> const_iterator
    {
        return _vertices.cbegin();
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::end() noexcept -> iterator
    {
        return _vertices.end();
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::end() const noexcept -> const_iterator
    {
        return _vertices.end();
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::cend() const noexcept -> const_iterator
    {
        return _vertices.cend();
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::find(vertex_key key) noexcept -> iterator
    {
        return _vertices.find(key);
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::find(vertex_key key) const noexcept -> const_iterator
    {
        return _vertices.find(key);
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::outgoing_edges(vertex_key key) const noexcept -> span<const edge_type>
    {
        auto vertex_it = _vertices.find(key);
        if (vertex_it == _vertices.end())
        {
            return {};
        }

        return vertex_it->outgoing_edges;
    }

    template <typename V, typename E, typename W>
    auto directed_graph<V, E, W>::incoming_edges(vertex_key key) const noexcept -> span<const edge_type>
    {
        auto vertex_it = _vertices.find(key);
        if (vertex_it == _vertices.end())
        {
            return {};
        }

        return vertex_it->incoming_edges;
    }

    template <typename V, typename E, typename W>
    inline void directed_graph<V, E, W>::swap(directed_graph& other) noexcept
    {
        using tempest::swap;

        swap(_vertices, other._vertices);
        swap(_edge_count, other._edge_count);
    }

    template <typename V, typename E, typename W>
    auto size(const directed_graph<V, E, W>& graph) noexcept -> typename directed_graph<V, E, W>::size_type
    {
        return graph.vertex_count();
    }

    template <typename V, typename E, typename W>
    auto empty(const directed_graph<V, E, W>& graph) noexcept -> bool
    {
        return graph.empty();
    }

    template <typename V, typename E, typename W>
    auto begin(directed_graph<V, E, W>& graph) noexcept -> typename directed_graph<V, E, W>::iterator
    {
        return graph.begin();
    }

    template <typename V, typename E, typename W>
    auto begin(const directed_graph<V, E, W>& graph) noexcept -> typename directed_graph<V, E, W>::const_iterator
    {
        return graph.begin();
    }

    template <typename V, typename E, typename W>
    auto cbegin(const directed_graph<V, E, W>& graph) noexcept -> typename directed_graph<V, E, W>::const_iterator
    {
        return graph.cbegin();
    }

    template <typename V, typename E, typename W>
    auto end(directed_graph<V, E, W>& graph) noexcept -> typename directed_graph<V, E, W>::iterator
    {
        return graph.end();
    }

    template <typename V, typename E, typename W>
    auto end(const directed_graph<V, E, W>& graph) noexcept -> typename directed_graph<V, E, W>::const_iterator
    {
        return graph.end();
    }

    template <typename V, typename E, typename W>
    auto cend(const directed_graph<V, E, W>& graph) noexcept -> typename directed_graph<V, E, W>::const_iterator
    {
        return graph.cend();
    }

    template <typename V, typename E, typename W>
    void swap(directed_graph<V, E, W>& lhs, directed_graph<V, E, W>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
} // namespace tempest

#endif // tempest_core_graph_hpp