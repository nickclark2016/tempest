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