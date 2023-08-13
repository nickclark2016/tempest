#include <gtest/gtest.h>

#include <tempest/archetype.hpp>

using namespace tempest::ecs;

TEST(archetype, default_construct)
{
    archetype<16, int, double> arch;
    auto entity = arch.allocate();

    ASSERT_EQ(arch.entity_count(), 1);
    ASSERT_GE(arch.entity_capacity(), 1);

    for_each_mut(arch, [](auto components) {
        auto& [c1, c2] = components;
        c1 = 0;
        c2 = 3.14;
    });

    for_each(std::cref(arch).get(), [](auto components) {
        auto& [c1, c2] = components;
        ASSERT_EQ(c1, 0);
        ASSERT_EQ(c2, 3.14);
    });
}

TEST(archetype, resize_allocation)
{
    archetype<16, int, double> arch;
    const std::size_t count = 64;

    for (std::size_t i = 0; i < count; ++i)
    {
        auto entity = arch.allocate();
        ASSERT_EQ(entity.id, i);
    }

    std::size_t counter = 0;
    for_each(arch, [&counter](auto components) { ++counter; });
    ASSERT_EQ(counter, count);
}

TEST(archetype, for_each_select)
{
    archetype<16, int, double> arch;
    auto entity = arch.allocate();

    ASSERT_EQ(arch.entity_count(), 1);
    ASSERT_GE(arch.entity_capacity(), 1);

    for_each_mut_select<int>(arch, [](std::tuple<int&> components) {
        auto& [c1] = components;
        c1 = 2;
    });

    for_each_select<int>(arch, [](std::tuple<const int&> components) {
        const auto& [c1] = components;
        ASSERT_EQ(c1, 2);
    });
}