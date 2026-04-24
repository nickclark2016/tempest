#include <gtest/gtest.h>

#include <tempest/sparse.hpp>

using namespace tempest::ecs;

TEST(sparse_set, default_construct)
{
    sparse_set s;

    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(s.capacity(), 0);
    ASSERT_TRUE(s.empty());
}

TEST(sparse_set, single_insert)
{
    sparse_set s;

    entity e = entity_traits<entity>::construct(0, 0);
    entity e2 = entity_traits<entity>::construct(1, 0);

    auto it = s.insert(e);

    ASSERT_NE(it, s.end());

    ASSERT_EQ(s.size(), 1);
    ASSERT_GE(s.capacity(), 1);

    ASSERT_TRUE(s.contains(e));
    ASSERT_FALSE(s.contains(e2));

    s.erase(it);

    ASSERT_FALSE(s.contains(e));
    ASSERT_EQ(s.find(e), s.end());
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.size(), 0);
}

TEST(sparse_set, multiple_insert)
{
    sparse_set s;

    entity e = entity_traits<entity>::construct(0, 0);
    entity e2 = entity_traits<entity>::construct(1, 0);

    auto it2 = s.insert(e2);
    ASSERT_NE(it2, s.end());

    auto it = s.insert(e);
    ASSERT_NE(it, s.end());

    ASSERT_EQ(s.size(), 2);
    ASSERT_GE(s.capacity(), 2);

    ASSERT_TRUE(s.contains(e));
    ASSERT_TRUE(s.contains(e2));
    
    ASSERT_NE(s.find(e), s.end());
    ASSERT_NE(s.find(e2), s.end());
    ASSERT_EQ(*s.find(e), e);
    ASSERT_EQ(*s.find(e2), e2);

    s.erase(s.find(e2));

    ASSERT_FALSE(s.contains(e2));
    ASSERT_EQ(s.find(e2), s.end());

    s.erase(s.find(e));

    ASSERT_FALSE(s.contains(e));
    ASSERT_EQ(s.find(e), s.end());

    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.size(), 0);
}

TEST(sparse_set, iterator)
{
    sparse_set s;

    for (std::uint32_t i = 0u; i < 4096; ++i)
    {
        auto inv_i = 4095 - i;

        entity e = entity_traits<entity>::construct(0, inv_i);
        s.insert(e);
    }

    ASSERT_EQ(s.size(), 4096);
    ASSERT_GE(s.capacity(), 4096);
    
    auto it = s.begin();

    for (std::uint32_t i = 0u; i < 4096; ++i)
    {
        entity e = entity_traits<entity>::construct(0, i);
        ASSERT_EQ(e, *it);

        ++it;
    }

    std::uint32_t i = 0;
    for (auto e : s)
    {
        entity ent = entity_traits<entity>::construct(0, i++);
        ASSERT_EQ(e, ent);
    }

    ASSERT_EQ(it, s.end());
}

TEST(sparse_map, default_construct)
{
    sparse_map<int> s;

    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(s.capacity(), 0);
}

TEST(sparse_map, insert)
{
    sparse_map<int> s;

    entity e = entity_traits<entity>::construct(0, 0);
    entity e2 = entity_traits<entity>::construct(1, 0);

    auto it = s.insert(e, 42);

    ASSERT_NE(it, s.end());
    ASSERT_EQ(it->second, 42);

    ASSERT_EQ(s.size(), 1);
    ASSERT_GE(s.capacity(), 1);

    ASSERT_TRUE(s.contains(e));
    ASSERT_FALSE(s.contains(e2));

    s.erase(e);

    ASSERT_FALSE(s.contains(e));
    ASSERT_EQ(s.find(e), s.end());
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.size(), 0);
}

TEST(sparse_map, multiple_insert)
{
    sparse_map<int> s;

    for (int i = 0; i < 8192; i++) {
        s.insert(entity_traits<entity>::construct(i, 0), i);
    }
    
    // Check if all elements are present
    for (int i = 0; i < 8192; i++) {
        entity e = entity_traits<entity>::construct(i, 0);
        ASSERT_TRUE(s.contains(e));
        ASSERT_EQ(s.find(e)->second, i);
    }

    // Remove every other value
    for (int i = 0; i < 8192; i += 2) {
        entity e = entity_traits<entity>::construct(i, 0);
        s.erase(e);
    }

    ASSERT_EQ(s.size(), 4096);

    // Check if the removed elements are gone
    for (int i = 0; i < 8192; i += 2) {
        entity e = entity_traits<entity>::construct(i, 0);
        ASSERT_FALSE(s.contains(e));
    }

    s.clear();
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.size(), 0);
}

TEST(sparse_map, iterator)
{
    sparse_map<int> s;

    for (std::uint32_t i = 0u; i < 4096; ++i)
    {
        entity e = entity_traits<entity>::construct(i, 0);
        s.insert(e, 4095u - i);
    }

    ASSERT_EQ(s.size(), 4096);
    ASSERT_GE(s.capacity(), 4096);
    
    auto it = s.begin();

    for (std::uint32_t i = 0u; i < 4096; ++i)
    {
        entity e = entity_traits<entity>::construct(4095u - i, 0);
        ASSERT_EQ(e, it->first);
        ASSERT_EQ(it->second, i);

        ++it;
    }

    std::uint32_t i = 0;
    for (auto [e, v] : s)
    {
        entity ent = entity_traits<entity>::construct(4095u - i, 0);
        ASSERT_EQ(e, ent);
        ASSERT_EQ(v, i);
        
        ++i;
    }

    ASSERT_EQ(it, s.end());
}