#include <gtest/gtest.h>

#include <tempest/entity.hpp>
#include <tempest/sparse_set.hpp>

using namespace tempest::ecs;

TEST(sparse_set, DefaultConstructor)
{
    sparse_set<entity> set;

    ASSERT_EQ(set.size(), 0);
    ASSERT_EQ(set._capacity(), 0);
    ASSERT_TRUE(set.empty());
}

TEST(sparse_set, CopyConstructFromDefault)
{
    sparse_set<entity> src;
    sparse_set<entity> dst = src;

    ASSERT_EQ(dst.size(), 0);
    ASSERT_EQ(dst._capacity(), 0);
    ASSERT_TRUE(dst.empty());
}

TEST(sparse_set, MoveFromDefault)
{
    sparse_set<entity> src;
    sparse_set<entity> dst = std::move(src);

    ASSERT_EQ(dst.size(), 0);
    ASSERT_EQ(dst._capacity(), 0);
    ASSERT_TRUE(dst.empty());
}

TEST(sparse_set, InsertByConstRef)
{
    sparse_set<entity> set;

    entity e{.id{0}, .generation{0}};
    ASSERT_TRUE(set.insert(e));
    ASSERT_TRUE(set.contains(e));

    ASSERT_EQ(set.size(), 1);
    ASSERT_GE(set._capacity(), 1);

    ASSERT_TRUE(set.remove(e));
    ASSERT_FALSE(set.contains(e));
}

TEST(sparse_set, InsertByConstRefUntilResize)
{
    sparse_set<entity> set;
    entity e{.id{0}, .generation{0}};

    set.insert(e);
    auto cap = set._capacity();

    for (std::uint32_t i = 1; i <= cap; ++i)
    {
        e.id = i;
        set.insert(e);
    }

    ASSERT_EQ(set.size(), 9);
    ASSERT_GE(set._capacity(), 9);

    for (std::uint32_t i = 0; i < 9; ++i)
    {
        e.id = i;
        ASSERT_TRUE(set.contains(e));
    }

    for (std::uint32_t i = 0; i < 9; ++i)
    {
        e.id = i;
        ASSERT_TRUE(set.remove(e));
    }

    ASSERT_EQ(set.size(), 0);
    ASSERT_GE(set._capacity(), 0);
}

TEST(sparse_set, MovedAssignSetWithContents)
{
    sparse_set<entity> set;
    entity e = {
        .id{0},
        .generation{0},
    };

    set.insert(e);

    sparse_set<entity> dst = std::move(set);

    ASSERT_TRUE(dst.contains(e));
    ASSERT_FALSE(dst.insert(e));
}