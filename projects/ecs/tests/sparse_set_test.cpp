#include <gtest/gtest.h>

#include <tempest/entity.hpp>
#include <tempest/sparse_set.hpp>

using namespace tempest::ecs;

TEST(sparse_set, DefaultConstructor)
{
    sparse_set<entity> set;

    ASSERT_EQ(set.size(), 0);
    ASSERT_EQ(set.capacity(), 0);
    ASSERT_TRUE(set.empty());
}

TEST(sparse_set, CopyConstructFromDefault)
{
    sparse_set<entity> src;
    sparse_set<entity> dst = src;

    ASSERT_EQ(dst.size(), 0);
    ASSERT_EQ(dst.capacity(), 0);
    ASSERT_TRUE(dst.empty());
}