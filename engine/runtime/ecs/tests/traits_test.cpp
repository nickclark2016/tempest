#include <tempest/traits.hpp>

#include <gtest/gtest.h>

using namespace tempest::ecs;

TEST(entity_traits, default_entity)
{
    using traits = entity_traits<entity>;

    auto e = traits::construct(0, 0);
    ASSERT_EQ(0, traits::as_integral(e));
    ASSERT_FALSE(e == null);
    ASSERT_FALSE(null == e);
    ASSERT_TRUE(e != null);
    ASSERT_TRUE(null != e);
    ASSERT_FALSE(e == tombstone);
    ASSERT_FALSE(tombstone == e);
    ASSERT_TRUE(e != tombstone);
    ASSERT_TRUE(tombstone != e);
}

TEST(entity_traits, non_zero_entity)
{
    using traits = entity_traits<entity>;

    auto e = traits::construct(1, 0);
    ASSERT_EQ(1, traits::as_entity(e));
    ASSERT_EQ(0, traits::as_version(e));
    ASSERT_FALSE(e == null);
    ASSERT_FALSE(null == e);
    ASSERT_TRUE(e != null);
    ASSERT_TRUE(null != e);
    ASSERT_FALSE(e == tombstone);
    ASSERT_FALSE(tombstone == e);
    ASSERT_TRUE(e != tombstone);
    ASSERT_TRUE(tombstone != e);
}

TEST(entity_traits, non_zero_version)
{
    using traits = entity_traits<entity>;

    auto e = traits::construct(0, 1);
    ASSERT_EQ(0, traits::as_entity(e));
    ASSERT_EQ(1, traits::as_version(e));
    ASSERT_FALSE(e == null);
    ASSERT_FALSE(null == e);
    ASSERT_TRUE(e != null);
    ASSERT_TRUE(null != e);
    ASSERT_FALSE(e == tombstone);
    ASSERT_FALSE(tombstone == e);
    ASSERT_TRUE(e != tombstone);
    ASSERT_TRUE(tombstone != e);
}

TEST(entity_traits, non_zero_entity_and_version)
{
    using traits = entity_traits<entity>;

    auto e = traits::construct(1, 1);
    ASSERT_EQ(1, traits::as_entity(e));
    ASSERT_EQ(1, traits::as_version(e));
    ASSERT_FALSE(e == null);
    ASSERT_FALSE(null == e);
    ASSERT_TRUE(e != null);
    ASSERT_TRUE(null != e);
    ASSERT_FALSE(e == tombstone);
    ASSERT_FALSE(tombstone == e);
    ASSERT_TRUE(e != tombstone);
    ASSERT_TRUE(tombstone != e);
}

TEST(entity_traits, null_conversion)
{
    entity e = null;
    ASSERT_TRUE(e == null);
    ASSERT_TRUE(null == e);
    ASSERT_FALSE(e != null);
    ASSERT_FALSE(null != e);
}

TEST(entity_traits, tombstone_conversion)
{
    entity e = tombstone;
    ASSERT_TRUE(e == tombstone);
    ASSERT_TRUE(tombstone == e);
    ASSERT_FALSE(e != tombstone);
    ASSERT_FALSE(tombstone != e);
}