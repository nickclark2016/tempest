#include <tempest/graph.hpp>

#include <gtest/gtest.h>

TEST(directed_graph, default_constructor)
{
    tempest::directed_graph<int, float, double> graph;

    EXPECT_EQ(graph.vertex_count(), 0);
    EXPECT_EQ(graph.edge_count(), 0);
    EXPECT_TRUE(graph.empty());
}

TEST(directed_graph, add_vertex)
{
    tempest::directed_graph<int, float, double> graph;

    auto key = graph.add_vertex(42);
    (void)key;

    EXPECT_EQ(graph.vertex_count(), 1);
    EXPECT_EQ(graph.edge_count(), 0);
    EXPECT_FALSE(graph.empty());
}

TEST(directed_graph, remove_vertex)
{
    tempest::directed_graph<int, float, double> graph;

    auto key = graph.add_vertex(42);
    graph.remove_vertex(key);

    EXPECT_EQ(graph.vertex_count(), 0);
    EXPECT_EQ(graph.edge_count(), 0);
    EXPECT_TRUE(graph.empty());
}

TEST(directed_graph, add_edge)
{
    tempest::directed_graph<int, float, double> graph;

    auto key1 = graph.add_vertex(42);
    auto key2 = graph.add_vertex(43);
    graph.add_edge(key1, key2, 3.14f);

    EXPECT_EQ(graph.vertex_count(), 2);
    EXPECT_EQ(graph.edge_count(), 1);
    EXPECT_FALSE(graph.empty());

    // Check that the edge was added correctly
    auto k1_it = graph.find(key1);
    ASSERT_NE(k1_it, graph.end());
    EXPECT_EQ(k1_it->outgoing_edges.size(), 1);
    EXPECT_EQ(k1_it->outgoing_edges[0].target, key2);

    auto k2_it = graph.find(key2);
    ASSERT_NE(k2_it, graph.end());
    EXPECT_EQ(k2_it->incoming_edges.size(), 1);
    EXPECT_EQ(k2_it->incoming_edges[0].source, key1);

    // Check that the edge has the correct value
    EXPECT_EQ(k1_it->outgoing_edges[0].data, 3.14f);
    EXPECT_EQ(k2_it->incoming_edges[0].data, 3.14f);

    // Check that the edge has the correct weight
    EXPECT_EQ(k1_it->outgoing_edges[0].weight, 0.0);
    EXPECT_EQ(k2_it->incoming_edges[0].weight, 0.0);
}

TEST(directed_graph, add_edge_with_weight)
{
    tempest::directed_graph<int, float, double> graph;

    auto key1 = graph.add_vertex(42);
    auto key2 = graph.add_vertex(43);
    graph.add_edge(key1, key2, 3.14f, 2.71);

    EXPECT_EQ(graph.vertex_count(), 2);
    EXPECT_EQ(graph.edge_count(), 1);
    EXPECT_FALSE(graph.empty());

    // Check that the edge was added correctly
    auto k1_it = graph.find(key1);
    ASSERT_NE(k1_it, graph.end());
    EXPECT_EQ(k1_it->outgoing_edges.size(), 1);
    EXPECT_EQ(k1_it->outgoing_edges[0].target, key2);

    auto k2_it = graph.find(key2);
    ASSERT_NE(k2_it, graph.end());
    EXPECT_EQ(k2_it->incoming_edges.size(), 1);
    EXPECT_EQ(k2_it->incoming_edges[0].source, key1);

    // Check that the edge has the correct value
    EXPECT_EQ(k1_it->outgoing_edges[0].data, 3.14f);
    EXPECT_EQ(k2_it->incoming_edges[0].data, 3.14f);

    // Check that the edge has the correct weight
    EXPECT_EQ(k1_it->outgoing_edges[0].weight, 2.71);
    EXPECT_EQ(k2_it->incoming_edges[0].weight, 2.71);
}

TEST(directed_graph, remove_vertex_with_connected_edges)
{
    tempest::directed_graph<int, float, double> graph;

    auto key1 = graph.add_vertex(42);
    auto key2 = graph.add_vertex(43);
    graph.add_edge(key1, key2, 3.14f);

    graph.remove_vertex(key1);

    EXPECT_EQ(graph.vertex_count(), 1);
    EXPECT_EQ(graph.edge_count(), 0);
    EXPECT_FALSE(graph.empty());

    // Check that the edge was removed correctly
    auto k2_it = graph.find(key2);
    ASSERT_NE(k2_it, graph.end());
    EXPECT_EQ(k2_it->incoming_edges.size(), 0);
}

TEST(directed_graph, find_removed_vertex)
{
    tempest::directed_graph<int, float, double> graph;

    auto key1 = graph.add_vertex(42);
    auto key2 = graph.add_vertex(43);
    graph.add_edge(key1, key2, 3.14f);

    graph.remove_vertex(key1);

    auto it = graph.find(key1);
    EXPECT_EQ(it, graph.end());
}

TEST(directed_graph, swap_empty_with_non_empty)
{
    tempest::directed_graph<int, float, double> graph1;
    tempest::directed_graph<int, float, double> graph2;

    auto key = graph2.add_vertex(42);

    tempest::swap(graph1, graph2);

    EXPECT_EQ(graph1.vertex_count(), 1);
    EXPECT_EQ(graph1.edge_count(), 0);
    EXPECT_FALSE(graph1.empty());

    EXPECT_EQ(graph2.vertex_count(), 0);
    EXPECT_EQ(graph2.edge_count(), 0);
    EXPECT_TRUE(graph2.empty());

    auto it = graph1.find(key);
    ASSERT_NE(it, graph1.end());
    EXPECT_EQ(it->data, 42);
}

TEST(directed_graph, swap_non_empty_graphs)
{
    tempest::directed_graph<int, float, double> graph1;
    tempest::directed_graph<int, float, double> graph2;

    auto key1 = graph1.add_vertex(42);
    auto key2 = graph2.add_vertex(43);

    tempest::swap(graph1, graph2);

    EXPECT_EQ(graph1.vertex_count(), 1);
    EXPECT_EQ(graph1.edge_count(), 0);
    EXPECT_FALSE(graph1.empty());

    EXPECT_EQ(graph2.vertex_count(), 1);
    EXPECT_EQ(graph2.edge_count(), 0);
    EXPECT_FALSE(graph2.empty());

    auto it1 = graph1.find(key1);
    ASSERT_NE(it1, graph1.end());
    EXPECT_EQ(it1->data, 43);

    auto it2 = graph2.find(key2);
    ASSERT_NE(it2, graph2.end());
    EXPECT_EQ(it2->data, 42);
}

TEST(directed_graph, matches_graph_concept)
{
    using graph_type = tempest::directed_graph<int, float, double>;
    bool matches = tempest::graph<graph_type>;

    EXPECT_TRUE(matches);
}