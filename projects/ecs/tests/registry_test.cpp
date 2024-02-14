#include <tempest/registry.hpp>

#include <gtest/gtest.h>

using namespace tempest::ecs;

TEST(entity_store, default_constructor)
{
    entity_store store;

    ASSERT_EQ(store.size(), 0);
    ASSERT_GE(store.capacity(), 0);
    ASSERT_TRUE(store.empty());
    ASSERT_EQ(store.begin(), store.end());
    ASSERT_EQ(store.cbegin(), store.cend());
}

TEST(entity_store, construct_with_1024)
{
    entity_store store(1024);

    ASSERT_EQ(store.size(), 0);
    ASSERT_GE(store.capacity(), 1024);
    ASSERT_TRUE(store.empty());
    ASSERT_EQ(store.begin(), store.end());
    ASSERT_EQ(store.cbegin(), store.cend());
}

TEST(entity_store, acquire)
{
    entity_store store;

    const auto entity_count = entity_store::entities_per_chunk * 2;

    for (std::size_t i = 0; i < entity_count; ++i)
    {
        std::ignore = store.acquire();
    }

    ASSERT_EQ(store.size(), entity_count);
    ASSERT_GE(store.capacity(), store.size());
    ASSERT_FALSE(store.empty());
    ASSERT_NE(store.begin(), store.end());

    store.clear();

    ASSERT_EQ(store.size(), 0);
    ASSERT_GE(store.capacity(), store.size());
    ASSERT_TRUE(store.empty());
    ASSERT_EQ(store.begin(), store.end());
    ASSERT_EQ(store.cbegin(), store.cend());
}

TEST(entity_store, release)
{
    entity_store store;

    const auto entity_count = entity_store::entities_per_chunk * 2;

    std::vector<entity> entities;

    for (std::size_t i = 0; i < entity_count; ++i)
    {
        entities.push_back(store.acquire());
    }

    ASSERT_EQ(store.size(), entity_count);
    ASSERT_GE(store.capacity(), store.size());
    ASSERT_FALSE(store.empty());
    ASSERT_NE(store.begin(), store.end());

    for (std::size_t i = 0; i < entity_count; ++i)
    {
        ASSERT_TRUE(store.is_valid(entities[i]));
        store.release(entities[i]);
        ASSERT_FALSE(store.is_valid(entities[i]));
    }

    ASSERT_EQ(store.size(), 0);
    ASSERT_GE(store.capacity(), store.size());
    ASSERT_TRUE(store.empty());
    ASSERT_EQ(store.begin(), store.end());
    ASSERT_EQ(store.cbegin(), store.cend());
}

TEST(entity_store, iterator)
{
    entity_store store;

    const auto entity_count = entity_store::entities_per_chunk * 2;

    std::vector<entity> entities;

    for (std::size_t i = 0; i < entity_count; ++i)
    {
        entities.push_back(store.acquire());
    }

    ASSERT_EQ(store.size(), entity_count);

    entity_traits<entity>::entity_type index = 0;
    for (auto ent : store)
    {
        ASSERT_EQ(ent, entities[index]);
        ++index;
    }

    // remove every other entity
    for (std::size_t i = 0; i < entity_count; i += 2)
    {
        store.release(entities[i]);
    }

    index = 1;

    // test to make sure the iterator skips the removed entities
    for (auto ent : store)
    {
        ASSERT_EQ(entities[index], ent);
        index += 2;
    }
}

TEST(entity_store, recycle_identifier)
{
    entity_store store;
    const auto entity_count = entity_store::entities_per_chunk * 2;

    std::vector<entity> entities;

    for (std::size_t i = 0; i < entity_count; ++i)
    {
        entities.push_back(store.acquire());
    }

    ASSERT_EQ(store.size(), entity_count);

    // release every other entity

    for (std::size_t i = 0; i < entity_count; i += 2)
    {
        store.release(entities[i]);
    }

    // check entity validity
    for (std::size_t i = 0; i < entity_count; ++i)
    {
        if (i % 2 == 0)
        {
            ASSERT_FALSE(store.is_valid(entities[i]));
        }
        else
        {
            ASSERT_TRUE(store.is_valid(entities[i]));
        }
    }

    // acquire new entities
    for (std::size_t i = 0; i < entity_store::entities_per_chunk; ++i)
    {
        entities[2 * i] = store.acquire();
    }

    // check entity validity, all should be valid
    for (std::size_t i = 0; i < entity_count; ++i)
    {
        ASSERT_TRUE(store.is_valid(entities[i]));
    }

    // validate store size
    ASSERT_EQ(store.size(), entity_count);

    // check version of each entity
    for (std::size_t i = 0; i < entity_count; ++i)
    {
        ASSERT_EQ(entity_traits<entity>::as_version(entities[i]), 1 - (i % 2));
    }
}

TEST(registry, acquire_entity)
{
    registry reg;

    const auto entity = reg.acquire_entity();

    ASSERT_TRUE(reg.is_valid(entity));
    ASSERT_EQ(reg.entity_count(), 1);
}