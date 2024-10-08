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

TEST(registry, assign_entity)
{
    registry reg;

    const auto entity = reg.acquire_entity();

    ASSERT_TRUE(reg.is_valid(entity));
    ASSERT_EQ(reg.entity_count(), 1);

    reg.assign<int>(entity, 42);

    ASSERT_TRUE(reg.has<int>(entity));
    ASSERT_FALSE(reg.has<float>(entity));
    ASSERT_EQ(reg.get<int>(entity), 42);

    reg.remove<int>(entity);
    ASSERT_FALSE(reg.has<int>(entity));
}

TEST(registry, assign_multiple_entities_multiple_components)
{
    registry reg;

    const auto entity1 = reg.acquire_entity();
    const auto entity2 = reg.acquire_entity();

    ASSERT_TRUE(reg.is_valid(entity1));
    ASSERT_TRUE(reg.is_valid(entity2));
    ASSERT_EQ(reg.entity_count(), 2);

    reg.assign<int>(entity1, 42);
    reg.assign<float>(entity1, 3.14f);

    reg.assign<int>(entity2, 24);
    reg.assign<float>(entity2, 6.28f);

    ASSERT_TRUE(reg.has<int>(entity1));
    ASSERT_TRUE(reg.has<float>(entity1));
    ASSERT_EQ(reg.get<int>(entity1), 42);
    ASSERT_EQ(reg.get<float>(entity1), 3.14f);

    ASSERT_TRUE(reg.has<int>(entity2));
    ASSERT_TRUE(reg.has<float>(entity2));
    ASSERT_EQ(reg.get<int>(entity2), 24);
    ASSERT_EQ(reg.get<float>(entity2), 6.28f);

    reg.remove<int>(entity1);
    reg.remove<float>(entity1);

    reg.remove<int>(entity2);
    reg.remove<float>(entity2);

    ASSERT_FALSE(reg.has<int>(entity1));
    ASSERT_FALSE(reg.has<float>(entity1));

    ASSERT_FALSE(reg.has<int>(entity2));
    ASSERT_FALSE(reg.has<float>(entity2));
}

TEST(registry, assign_multiple_entities_different_components)
{
    registry reg;

    const auto entity1 = reg.acquire_entity();
    const auto entity2 = reg.acquire_entity();

    ASSERT_TRUE(reg.is_valid(entity1));
    ASSERT_TRUE(reg.is_valid(entity2));
    ASSERT_EQ(reg.entity_count(), 2);

    reg.assign<int>(entity1, 42);
    reg.assign<float>(entity2, 3.14f);

    ASSERT_TRUE(reg.has<int>(entity1));
    ASSERT_FALSE(reg.has<float>(entity1));
    ASSERT_EQ(reg.get<int>(entity1), 42);

    ASSERT_FALSE(reg.has<int>(entity2));
    ASSERT_TRUE(reg.has<float>(entity2));
    ASSERT_EQ(reg.get<float>(entity2), 3.14f);

    reg.remove<int>(entity1);
    reg.remove<float>(entity2);

    ASSERT_FALSE(reg.has<int>(entity1));
    ASSERT_FALSE(reg.has<float>(entity2));
}

TEST(registry, assign_large_entity_count)
{
    registry reg;

    std::size_t entity_count = 16384;
    std::vector<entity> entities;

    for (std::size_t i = 0; i < entity_count; ++i)
    {
        const auto entity = reg.acquire_entity();

        if (i % 2 == 0)
        {
            reg.assign<int>(entity, 42);
        }
        else
        {
            reg.assign<float>(entity, 3.14f);
        }

        if (i % 4 < 2)
        {
            reg.assign<double>(entity, 6.28);
        }
        else
        {
            reg.assign<char>(entity, 'a');
        }

        entities.push_back(entity);
    }

    for (std::size_t i = 0; i < entity_count; ++i)
    {
        const auto entity = entities[i];

        if (i % 2 == 0)
        {
            ASSERT_TRUE(reg.has<int>(entity));
            ASSERT_EQ(reg.get<int>(entity), 42);
        }
        else
        {
            ASSERT_TRUE(reg.has<float>(entity));
            ASSERT_EQ(reg.get<float>(entity), 3.14f);
        }

        if (i % 4 < 2)
        {
            ASSERT_TRUE(reg.has<double>(entity));
            ASSERT_EQ(reg.get<double>(entity), 6.28);
        }
        else
        {
            ASSERT_TRUE(reg.has<char>(entity));
            ASSERT_EQ(reg.get<char>(entity), 'a');
        }
    }
}

TEST(registry, has_multiple)
{
    registry reg;

    const auto entity = reg.acquire_entity();

    ASSERT_TRUE(reg.is_valid(entity));
    ASSERT_EQ(reg.entity_count(), 1);

    reg.assign<int>(entity, 42);
    reg.assign<float>(entity, 3.14f);

    ASSERT_TRUE((reg.has<int, float>(entity)));
    ASSERT_FALSE((reg.has<int, double>(entity)));
    ASSERT_FALSE((reg.has<float, double>(entity)));
    ASSERT_FALSE((reg.has<int, float, double>(entity)));

    auto values = reg.get<int, float>(entity);
    ASSERT_EQ(std::get<0>(values), 42);
    ASSERT_EQ(std::get<1>(values), 3.14f);

    auto try_values = reg.try_get<int, float>(entity);
    ASSERT_NE(std::get<0>(try_values), nullptr);
    ASSERT_NE(std::get<1>(try_values), nullptr);
    ASSERT_EQ(*std::get<0>(try_values), 42);
    ASSERT_EQ(*std::get<1>(try_values), 3.14f);

    auto try_values_with_fail = reg.try_get<int, double>(entity);
    ASSERT_NE(std::get<0>(try_values_with_fail), nullptr);
    ASSERT_EQ(std::get<1>(try_values_with_fail), nullptr);
    ASSERT_EQ(*std::get<0>(try_values_with_fail), 42);

    reg.remove<int>(entity);
    reg.remove<float>(entity);

    ASSERT_FALSE((reg.has<int, float>(entity)));
}

TEST(registry, stress_test)
{
    registry reg;

    const std::size_t entity_count = 1000000; // Adjust this number based on your system's capabilities
    std::vector<entity> entities;
    entities.reserve(entity_count);

    // Create a large number of entities and assign them data
    for(std::size_t i = 0; i < entity_count; ++i)
    {
        const auto entity = reg.acquire_entity();
        reg.assign<int>(entity, static_cast<std::uint32_t>(i));
        reg.assign<float>(entity, static_cast<float>(i));

        entities.push_back(entity);
    }

    ASSERT_EQ(reg.entity_count(), entity_count);

    // Check that the data was correctly assigned
    for(std::size_t i = 0; i < entity_count; ++i)
    {
        const auto entity = entities[i];
        auto values = reg.get<int, float>(entity);
        ASSERT_EQ(std::get<0>(values), i);
        ASSERT_EQ(std::get<1>(values), static_cast<float>(i));
    }

    // Remove the entities and their data
    for(std::size_t i = 0; i < entity_count; ++i)
    {
        const auto entity = entities[i];
        reg.remove<int>(entity);
        reg.remove<float>(entity);
        reg.release_entity(entity);
    }

    ASSERT_EQ(reg.entity_count(), 0);

    // Reuse slots

    entities.clear();

    for(std::size_t i = 0; i < entity_count; ++i)
    {
        const auto entity = reg.acquire_entity();
        reg.assign<int>(entity, static_cast<std::uint32_t>(i));
        reg.assign<float>(entity, static_cast<float>(i));

        entities.push_back(entity);
    }

    ASSERT_EQ(reg.entity_count(), entity_count);

    for(std::size_t i = 0; i < entity_count; ++i)
    {
        const auto entity = entities[i];
        auto values = reg.get<int, float>(entity);
        ASSERT_EQ(std::get<0>(values), i);
        ASSERT_EQ(std::get<1>(values), static_cast<float>(i));
    }
}

TEST(registry, view_test)
{
    registry reg;

    const std::size_t entity_count = 1000;
    std::vector<entity> entities;
    entities.reserve(entity_count);

    for (std::size_t i = 0; i < entity_count; ++i)
    {
        const auto entity = reg.acquire_entity();
        reg.assign<int>(entity, static_cast<std::uint32_t>(i));
        reg.assign<float>(entity, static_cast<float>(i));

        entities.push_back(entity);
    }

    ASSERT_EQ(reg.entity_count(), entity_count);

    auto view = reg.view<int, float>();

    for (auto [e, int_val, float_val] : view)
    {
        ASSERT_EQ(int_val, entity_traits<entity>::as_integral(e));
        ASSERT_EQ(float_val, static_cast<float>(entity_traits<entity>::as_integral(e)));
    }

    for (auto [entity, int_val, float_val] : view)
    {
        reg.remove<int>(entity);
        reg.remove<float>(entity);
    }

    // Check to make sure no entities are found in the view
    ASSERT_EQ(view.begin(), view.end());
}

TEST(registry, const_view_test)
{
    registry reg;

    const std::size_t entity_count = 1000;
    std::vector<entity> entities;
    entities.reserve(entity_count);

    for (std::size_t i = 0; i < entity_count; ++i)
    {
        const auto entity = reg.acquire_entity();
        reg.assign<int>(entity, static_cast<std::uint32_t>(i));
        reg.assign<float>(entity, static_cast<float>(i));

        entities.push_back(entity);
    }

    ASSERT_EQ(reg.entity_count(), entity_count);

    // Get const reference to the registry
    const auto& const_reg = reg;

    const auto view = const_reg.view<int, float>();

    for (const auto [e, int_val, float_val] : view)
    {
        ASSERT_EQ(int_val, entity_traits<entity>::as_integral(e));
        ASSERT_EQ(float_val, static_cast<float>(entity_traits<entity>::as_integral(e)));
    }

    for (const auto [entity, int_val, float_val] : view)
    {
        reg.remove<int>(entity);
        reg.remove<float>(entity);
    }

    // Check to make sure no entities are found in the view
    ASSERT_EQ(view.begin(), view.end());
}