#include <tempest/archetype.hpp>

#include <gtest/gtest.h>
#include <tempest/ecs_events.hpp>
#include <tempest/traits.hpp>
#include <tempest/utility.hpp>

TEST(basic_archetype_type_info, get_trivial_type_info)
{
    auto ti = tempest::ecs::create_archetype_type_info<int>();
    ASSERT_EQ(sizeof(int), ti.size);
    ASSERT_EQ(alignof(int), ti.alignment);
}

TEST(basic_archetype_type_info, get_trivial_struct_type_info)
{
    struct foo
    {
        int bar;
        float baz;
        char quux;
    };

    auto ti = tempest::ecs::create_archetype_type_info<foo>();
    ASSERT_EQ(sizeof(foo), ti.size);
    ASSERT_EQ(alignof(foo), ti.alignment);
}

TEST(basic_archetype_storage, construct_for_trivial_struct)
{
    struct foo
    {
        int bar;
        float baz;
        char quux;
    };

    auto ti = tempest::ecs::create_archetype_type_info<foo>();
    auto storage = tempest::ecs::basic_archetype_storage(ti);

    ASSERT_EQ(storage.capacity(), 0);

    storage.reserve(32);

    ASSERT_GE(storage.capacity(), 32 * sizeof(foo));

    foo f1 = {
        .bar = 1,
        .baz = 3.14F,
        .quux = 'q',
    };

    foo f2 = {
        .bar = 2,
        .baz = 6.28F,
        .quux = 'r',
    };

    auto* dst1 = storage.element_at(0);
    tempest::memcpy(dst1, &f1, sizeof(f1));

    auto* dst2 = storage.element_at(1);
    tempest::memcpy(dst2, &f2, sizeof(f2));

    ASSERT_TRUE(::memcmp(&f1, dst1, sizeof(f1)) == 0);
    ASSERT_TRUE(::memcmp(&f2, dst2, sizeof(f2)) == 0);

    storage.copy(0, 1);
    ASSERT_TRUE(::memcmp(dst1, dst2, sizeof(foo)) == 0);
}

TEST(basic_archetype, single_type)
{
    using type = float;

    tempest::ecs::basic_archetype_type_info ti[] = {
        tempest::ecs::create_archetype_type_info<type>(),
    };

    auto archetype = tempest::ecs::basic_archetype(ti);

    auto e1 = archetype.allocate();
    auto e2 = archetype.allocate();

    ASSERT_NE(e1, e2);
    ASSERT_EQ(archetype.size(), 2);
    ASSERT_GE(archetype.capacity(), 2);

    auto* f1 = reinterpret_cast<float*>(archetype.element_at(e1, 0));
    *f1 = 3.14F;
    auto* f2 = reinterpret_cast<float*>(archetype.element_at(e2, 0));
    *f2 = 6.28F;

    ASSERT_EQ(3.14F, *reinterpret_cast<float*>(archetype.element_at(e1, 0)));
    ASSERT_EQ(6.28F, *reinterpret_cast<float*>(archetype.element_at(e2, 0)));

    ASSERT_TRUE(archetype.erase(e1));

    // Ensure the value got moved
    ASSERT_EQ(6.28F, *reinterpret_cast<float*>(archetype.element_at(e2, 0)));

    auto e3 = archetype.allocate();
    ASSERT_EQ(0, e3.index);
    ASSERT_EQ(1, e3.generation);
}

TEST(basic_archetype, single_type_with_resize)
{
    using type = float;

    tempest::ecs::basic_archetype_type_info ti[] = {
        tempest::ecs::create_archetype_type_info<type>(),
    };

    auto archetype = tempest::ecs::basic_archetype(ti);
    tempest::vector<tempest::ecs::basic_archetype_key> keys;

    for (size_t i = 0; i < 32; ++i)
    {
        auto key = archetype.allocate();
        keys.push_back(key);
    }

    ASSERT_EQ(32, archetype.size());
}

TEST(basic_archetype_registry, create)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto entity = reg.create<int, float>();

    reg.replace<int>(entity, 3);
    reg.replace<float>(entity, 3.14F);

    ASSERT_EQ(reg.size(), 1);
    ASSERT_EQ(reg.get<int>(entity), 3);
    ASSERT_EQ(reg.get<float>(entity), 3.14F);
}

TEST(basic_archetype_registry, create_initialized)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto entity = reg.create_initialized<int, float>(3, 3.14F);

    ASSERT_EQ(reg.size(), 1);
    ASSERT_EQ(reg.get<int>(entity), 3);
    ASSERT_EQ(reg.get<float>(entity), 3.14F);
}

TEST(basic_archetype_registry, create_swapped)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto entity = reg.create<float, int>();
    reg.replace<int>(entity, 3);
    reg.replace<float>(entity, 3.14F);

    ASSERT_EQ(reg.size(), 1);
    ASSERT_EQ(reg.get<int>(entity), 3);
    ASSERT_EQ(reg.get<float>(entity), 3.14F);
}

TEST(basic_archetype_registry, create_and_assign)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto entity = reg.create<int, float>();

    reg.assign_or_replace<int>(entity, 3);
    reg.assign_or_replace<float>(entity, 3.14F);

    reg.assign<char>(entity, 'c');

    ASSERT_EQ(reg.size(), 1);
    ASSERT_EQ(reg.get<int>(entity), 3);
    ASSERT_EQ(reg.get<float>(entity), 3.14F);
    ASSERT_EQ(reg.get<char>(entity), 'c');
}

TEST(basic_archetype_registry, has_component)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto entity = reg.create<int, float>();

    ASSERT_TRUE(reg.has<int>(entity));
    ASSERT_TRUE(reg.has<float>(entity));
    ASSERT_FALSE(reg.has<char>(entity));
}

TEST(basic_archetype_registry, remove_component)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto entity = reg.create<int, float>();
    reg.remove<int>(entity);

    ASSERT_FALSE(reg.has<int>(entity));
    ASSERT_TRUE(reg.has<float>(entity));
}

TEST(basic_archetype_registry, try_get_component_with_component)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto entity = reg.create<int, float>();
    reg.replace<int>(entity, 3);
    reg.replace<float>(entity, 3.14F);

    ASSERT_EQ(reg.try_get<int>(entity), &reg.get<int>(entity));
    ASSERT_EQ(reg.try_get<float>(entity), &reg.get<float>(entity));
}

TEST(basic_archetype_registry, try_get_component_with_failure)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto entity = reg.create<int, float>();
    reg.replace<int>(entity, 3);
    reg.replace<float>(entity, 3.14F);

    ASSERT_EQ(reg.try_get<char>(entity), nullptr);
}

TEST(basic_archetype_registry, remove_lots_of_components)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto entity = reg.create<int, float, char, double, short, long, long long>();
    reg.remove<int>(entity);

    // Ensure only int is removed
    ASSERT_FALSE(reg.has<int>(entity));
    ASSERT_TRUE(reg.has<float>(entity));
    ASSERT_TRUE(reg.has<char>(entity));
    ASSERT_TRUE(reg.has<double>(entity));
    ASSERT_TRUE(reg.has<short>(entity));
    ASSERT_TRUE(reg.has<long>(entity));
    ASSERT_TRUE(reg.has<long long>(entity));

    reg.remove<float>(entity);

    // Ensure only float and int are removed
    ASSERT_FALSE(reg.has<int>(entity));
    ASSERT_FALSE(reg.has<float>(entity));
    ASSERT_TRUE(reg.has<char>(entity));
    ASSERT_TRUE(reg.has<double>(entity));
    ASSERT_TRUE(reg.has<short>(entity));
    ASSERT_TRUE(reg.has<long>(entity));
    ASSERT_TRUE(reg.has<long long>(entity));

    // Remove from the middle
    reg.remove<long>(entity);

    // Ensure only long, float and int are removed
    ASSERT_FALSE(reg.has<int>(entity));
    ASSERT_FALSE(reg.has<float>(entity));
    ASSERT_TRUE(reg.has<char>(entity));
    ASSERT_TRUE(reg.has<double>(entity));
    ASSERT_TRUE(reg.has<short>(entity));
    ASSERT_FALSE(reg.has<long>(entity));
    ASSERT_TRUE(reg.has<long long>(entity));
}

TEST(basic_archetype_registry, create_multiple_different_archetypes_with_removes_and_assigns)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto e1 = reg.create<int, float>();
    auto e2 = reg.create<int, float, char>();
    auto e3 = reg.create<int, float, char, double>();
    auto e4 = reg.create<int, float, char, double, short>();
    auto e5 = reg.create<int, float, char, double, short, long>();
    auto e6 = reg.create<int, float, char, double, short, long, long long>();
    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<float>(e1, 3.14F);
    reg.assign_or_replace<int>(e2, 2);
    reg.assign_or_replace<float>(e2, 6.28F);
    reg.assign_or_replace<char>(e2, 'c');
    reg.assign_or_replace<int>(e3, 3);
    reg.assign_or_replace<float>(e3, 9.42F);
    reg.assign_or_replace<char>(e3, 'd');
    reg.assign_or_replace<double>(e3, 1.0);
    reg.assign_or_replace<int>(e4, 4);
    reg.assign_or_replace<float>(e4, 12.56F);
    reg.assign_or_replace<char>(e4, 'e');
    reg.assign_or_replace<double>(e4, 2.0);
    reg.assign_or_replace<short>(e4, 1);
    reg.assign_or_replace<int>(e5, 5);
    reg.assign_or_replace<float>(e5, 15.70F);
    reg.assign_or_replace<char>(e5, 'f');
    reg.assign_or_replace<double>(e5, 3.0);
    reg.assign_or_replace<short>(e5, 2);
    reg.assign_or_replace<long>(e5, 1);
    reg.assign_or_replace<int>(e6, 6);
    reg.assign_or_replace<float>(e6, 18.84F);
    reg.assign_or_replace<char>(e6, 'g');
    reg.assign_or_replace<double>(e6, 4.0);
    reg.assign_or_replace<short>(e6, 3);
    reg.assign_or_replace<long>(e6, 2);
    reg.assign_or_replace<long long>(e6, 1);

    ASSERT_EQ(reg.get<int>(e1), 1);
    ASSERT_EQ(reg.get<float>(e1), 3.14F);
    ASSERT_EQ(reg.get<int>(e2), 2);
    ASSERT_EQ(reg.get<float>(e2), 6.28F);
    ASSERT_EQ(reg.get<char>(e2), 'c');
    ASSERT_EQ(reg.get<int>(e3), 3);
    ASSERT_EQ(reg.get<float>(e3), 9.42F);
    ASSERT_EQ(reg.get<char>(e3), 'd');
    ASSERT_EQ(reg.get<double>(e3), 1.0);
    ASSERT_EQ(reg.get<int>(e4), 4);
    ASSERT_EQ(reg.get<float>(e4), 12.56F);
    ASSERT_EQ(reg.get<char>(e4), 'e');
    ASSERT_EQ(reg.get<double>(e4), 2.0);
    ASSERT_EQ(reg.get<short>(e4), 1);
    ASSERT_EQ(reg.get<int>(e5), 5);
    ASSERT_EQ(reg.get<float>(e5), 15.70F);
    ASSERT_EQ(reg.get<char>(e5), 'f');
    ASSERT_EQ(reg.get<double>(e5), 3.0);
    ASSERT_EQ(reg.get<short>(e5), 2);
    ASSERT_EQ(reg.get<long>(e5), 1);
    ASSERT_EQ(reg.get<int>(e6), 6);
    ASSERT_EQ(reg.get<float>(e6), 18.84F);
    ASSERT_EQ(reg.get<char>(e6), 'g');
    ASSERT_EQ(reg.get<double>(e6), 4.0);
    ASSERT_EQ(reg.get<short>(e6), 3);
    ASSERT_EQ(reg.get<long>(e6), 2);
    ASSERT_EQ(reg.get<long long>(e6), 1);

    reg.remove<int>(e1);
    reg.remove<float>(e2);
    reg.remove<char>(e3);
    reg.remove<double>(e4);
    reg.remove<short>(e5);
    reg.remove<long>(e6);

    ASSERT_FALSE(reg.has<int>(e1));
    ASSERT_FALSE(reg.has<float>(e2));
    ASSERT_FALSE(reg.has<char>(e3));
    ASSERT_FALSE(reg.has<double>(e4));
    ASSERT_FALSE(reg.has<short>(e5));
    ASSERT_FALSE(reg.has<long>(e6));

    // Create more entities
    auto e7 = reg.create<int, float>();
    auto e8 = reg.create<int, float, char>();
    auto e9 = reg.create<int, float, char, double>();
    auto e10 = reg.create<int, float, char, double, short>();

    // Check the components in each exist
    ASSERT_TRUE(reg.has<int>(e7));
    ASSERT_TRUE(reg.has<float>(e7));
    ASSERT_TRUE(reg.has<int>(e8));
    ASSERT_TRUE(reg.has<float>(e8));
    ASSERT_TRUE(reg.has<char>(e8));
    ASSERT_TRUE(reg.has<int>(e9));
    ASSERT_TRUE(reg.has<float>(e9));
    ASSERT_TRUE(reg.has<char>(e9));
    ASSERT_TRUE(reg.has<double>(e9));
    ASSERT_TRUE(reg.has<int>(e10));
    ASSERT_TRUE(reg.has<float>(e10));
    ASSERT_TRUE(reg.has<char>(e10));
    ASSERT_TRUE(reg.has<double>(e10));
    ASSERT_TRUE(reg.has<short>(e10));
}

TEST(basic_archetype_registry, each_single_component)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto e1 = reg.create<int>();
    auto e2 = reg.create<int>();
    auto e3 = reg.create<int>();
    auto e4 = reg.create<int>();
    auto e5 = reg.create<int>();
    auto e6 = reg.create<int>();
    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<int>(e2, 2);
    reg.assign_or_replace<int>(e3, 3);
    reg.assign_or_replace<int>(e4, 4);
    reg.assign_or_replace<int>(e5, 5);
    reg.assign_or_replace<int>(e6, 6);

    // Check to make sure each entity has the correct value
    ASSERT_EQ(1, reg.get<int>(e1));
    ASSERT_EQ(2, reg.get<int>(e2));
    ASSERT_EQ(3, reg.get<int>(e3));
    ASSERT_EQ(4, reg.get<int>(e4));
    ASSERT_EQ(5, reg.get<int>(e5));
    ASSERT_EQ(6, reg.get<int>(e6));

    int sum = 0;

    auto func = [&sum](int i) -> void { sum += i; };
    reg.each(func);

    ASSERT_EQ(21, sum);
}

TEST(basic_archetype_registry, each_single_component_no_match)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto e1 = reg.create<int>();
    auto e2 = reg.create<int>();
    auto e3 = reg.create<int>();
    auto e4 = reg.create<int>();
    auto e5 = reg.create<int>();
    auto e6 = reg.create<int>();
    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<int>(e2, 2);
    reg.assign_or_replace<int>(e3, 3);
    reg.assign_or_replace<int>(e4, 4);
    reg.assign_or_replace<int>(e5, 5);
    reg.assign_or_replace<int>(e6, 6);

    // Check to make sure each entity has the correct value
    ASSERT_EQ(1, reg.get<int>(e1));
    ASSERT_EQ(2, reg.get<int>(e2));
    ASSERT_EQ(3, reg.get<int>(e3));
    ASSERT_EQ(4, reg.get<int>(e4));
    ASSERT_EQ(5, reg.get<int>(e5));
    ASSERT_EQ(6, reg.get<int>(e6));

    // Each on a lambda that doesn't match the entity
    float sum = 0;
    auto func = [&sum](float f) -> void { sum += f; };
    reg.each(func);
    ASSERT_EQ(0.0F, sum);
}

TEST(basic_archetype_registry, each_multiple_components_single_component_match)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto e1 = reg.create<int, float>();
    auto e2 = reg.create<int, float>();
    auto e3 = reg.create<int, float>();
    auto e4 = reg.create<int, float>();
    auto e5 = reg.create<int, float>();
    auto e6 = reg.create<int, float>();
    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<float>(e1, 3.14F);
    reg.assign_or_replace<int>(e2, 2);
    reg.assign_or_replace<float>(e2, 6.28F);
    reg.assign_or_replace<int>(e3, 3);
    reg.assign_or_replace<float>(e3, 9.42F);
    reg.assign_or_replace<int>(e4, 4);
    reg.assign_or_replace<float>(e4, 12.56F);
    reg.assign_or_replace<int>(e5, 5);
    reg.assign_or_replace<float>(e5, 15.70F);
    reg.assign_or_replace<int>(e6, 6);
    reg.assign_or_replace<float>(e6, 18.84F);
    // Check to make sure each entity has the correct value
    ASSERT_EQ(1, reg.get<int>(e1));
    ASSERT_EQ(3.14F, reg.get<float>(e1));
    ASSERT_EQ(2, reg.get<int>(e2));
    ASSERT_EQ(6.28F, reg.get<float>(e2));
    ASSERT_EQ(3, reg.get<int>(e3));
    ASSERT_EQ(9.42F, reg.get<float>(e3));
    ASSERT_EQ(4, reg.get<int>(e4));
    ASSERT_EQ(12.56F, reg.get<float>(e4));
    ASSERT_EQ(5, reg.get<int>(e5));
    ASSERT_EQ(15.70F, reg.get<float>(e5));
    ASSERT_EQ(6, reg.get<int>(e6));
    ASSERT_EQ(18.84F, reg.get<float>(e6));

    // Each on a lambda that matches the entity
    int sum = 0;

    auto func = [&sum](int i) -> void { sum += i; };
    reg.each(func);

    ASSERT_EQ(21, sum);

    float fsum = 0.0F;
    auto func2 = [&fsum](float f) -> void { fsum += f; };
    reg.each(func2);

    ASSERT_FLOAT_EQ(65.94F, fsum);
}

TEST(basic_archetype_registry, each_multiple_components_with_multiple_match_and_extra_components)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    auto e1 = reg.create<int, float, char>();
    auto e2 = reg.create<int, float, char>();
    auto e3 = reg.create<int, float, char>();
    auto e4 = reg.create<int, float, char>();
    auto e5 = reg.create<int, float, char>();
    auto e6 = reg.create<int, float, char>();
    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<float>(e1, 3.14F);
    reg.assign_or_replace<char>(e1, 'a');
    reg.assign_or_replace<int>(e2, 2);
    reg.assign_or_replace<float>(e2, 6.28F);
    reg.assign_or_replace<char>(e2, 'b');
    reg.assign_or_replace<int>(e3, 3);
    reg.assign_or_replace<float>(e3, 9.42F);
    reg.assign_or_replace<char>(e3, 'c');
    reg.assign_or_replace<int>(e4, 4);
    reg.assign_or_replace<float>(e4, 12.56F);
    reg.assign_or_replace<char>(e4, 'd');
    reg.assign_or_replace<int>(e5, 5);
    reg.assign_or_replace<float>(e5, 15.70F);
    reg.assign_or_replace<char>(e5, 'e');
    reg.assign_or_replace<int>(e6, 6);
    reg.assign_or_replace<float>(e6, 18.84F);
    reg.assign_or_replace<char>(e6, 'f');
    // Check to make sure each entity has the correct value
    ASSERT_EQ(1, reg.get<int>(e1));
    ASSERT_EQ(3.14F, reg.get<float>(e1));
    ASSERT_EQ('a', reg.get<char>(e1));
    ASSERT_EQ(2, reg.get<int>(e2));
    ASSERT_EQ(6.28F, reg.get<float>(e2));
    ASSERT_EQ('b', reg.get<char>(e2));
    ASSERT_EQ(3, reg.get<int>(e3));
    ASSERT_EQ(9.42F, reg.get<float>(e3));
    ASSERT_EQ('c', reg.get<char>(e3));
    ASSERT_EQ(4, reg.get<int>(e4));
    ASSERT_EQ(12.56F, reg.get<float>(e4));
    ASSERT_EQ('d', reg.get<char>(e4));
    ASSERT_EQ(5, reg.get<int>(e5));
    ASSERT_EQ(15.70F, reg.get<float>(e5));
    ASSERT_EQ(6, reg.get<int>(e6));
    ASSERT_EQ(18.84F, reg.get<float>(e6));

    // Each on a lambda that matches the entity
    int isum = 0;
    float fsum = 0.0F;

    auto func = [&isum, &fsum](int i, float f) -> void {
        isum += i;
        fsum += f;
    };

    reg.each(func);
    ASSERT_EQ(21, isum);
    ASSERT_FLOAT_EQ(65.94F, fsum);
}

TEST(basic_archetype_registry, each_has_single_component_test_against_multiple)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    auto e1 = reg.create<int>();
    auto e2 = reg.create<int>();

    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<int>(e2, 2);

    float fsum = 0.0F;

    auto func = [&fsum](float f) -> void { fsum += f; };

    reg.each(func);

    ASSERT_EQ(0.0F, fsum);
}

TEST(basic_archetype_registry, create_entity_emit_event)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    auto event_entity = tempest::ecs::entity{tempest::ecs::tombstone};

    [[maybe_unused]] const auto subscription_handle =
        events.dispatcher<tempest::ecs::entity_created_event<tempest::ecs::entity>>().subscribe(
            [&event_entity](auto evt) -> void { event_entity = evt.entity; });

    const auto created_entity = reg.create();
    ASSERT_EQ(created_entity, event_entity);
}

TEST(basic_archetype_registry, destroy_entity_emit_event)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    auto event_entity = tempest::ecs::entity{tempest::ecs::tombstone};

    [[maybe_unused]] const auto subscription_handle =
        events.dispatcher<tempest::ecs::entity_destroyed_event<tempest::ecs::entity>>().subscribe(
            [&event_entity](auto evt) -> void { event_entity = evt.entity; });

    const auto created_entity = reg.create();
    reg.destroy(created_entity);
    ASSERT_EQ(created_entity, event_entity);
}

TEST(basic_archetype_registry, component_added_emit_event)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    auto event_entity = tempest::ecs::entity{tempest::ecs::tombstone};
    auto event_component_value = 0;

    constexpr auto component_value = 42;

    [[maybe_unused]] const auto subscription_handle =
        events.dispatcher<tempest::ecs::component_added_event<tempest::ecs::entity, int>>().subscribe(
            [&event_entity, &event_component_value](auto evt) -> void {
                event_entity = evt.entity;
                event_component_value = evt.component;
            });

    const auto created_entity = reg.create();
    reg.assign_or_replace(created_entity, component_value);

    ASSERT_EQ(created_entity, event_entity);
    ASSERT_EQ(component_value, event_component_value);
}

/// @brief Verifies that assigning and removing components publishes entity_component_added_event
/// and entity_component_removed_event containing the correct entity and type indices.
TEST(basic_archetype_registry, publishes_generic_component_lifecycle_events)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    const auto entity = reg.create();

    auto added_entity = tempest::ecs::entity{tempest::ecs::tombstone};
    auto added_type_index = 0u;
    auto added_count = 0;

    auto removed_entity = tempest::ecs::entity{tempest::ecs::tombstone};
    auto removed_type_index = 0u;
    auto removed_count = 0;

    [[maybe_unused]] const auto add_sub =
        events.dispatcher<tempest::ecs::entity_component_added_event<tempest::ecs::entity>>().subscribe(
            [&](const auto& evt) {
                added_entity = evt.entity;
                added_type_index = evt.component_type_index;
                ++added_count;
            });

    [[maybe_unused]] const auto rem_sub =
        events.dispatcher<tempest::ecs::entity_component_removed_event<tempest::ecs::entity>>().subscribe(
            [&](const auto& evt) {
                removed_entity = evt.entity;
                removed_type_index = evt.component_type_index;
                ++removed_count;
            });

    reg.assign(entity, int{123});

    const auto expected_int_idx = types.ensure<int>();
    EXPECT_EQ(added_count, 1);
    EXPECT_EQ(added_entity, entity);
    EXPECT_EQ(added_type_index, expected_int_idx);

    reg.remove<int>(entity);

    EXPECT_EQ(removed_count, 1);
    EXPECT_EQ(removed_entity, entity);
    EXPECT_EQ(removed_type_index, expected_int_idx);
}

TEST(basic_archetype_registry, component_replaced_emit_event)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    auto event_entity = tempest::ecs::entity{tempest::ecs::tombstone};
    auto event_old_component_value = 0;
    auto event_new_component_value = 0;

    constexpr auto old_component_value = 42;
    constexpr auto new_component_value = 84;

    [[maybe_unused]] const auto subscription_handle =
        events.dispatcher<tempest::ecs::component_replaced_event<tempest::ecs::entity, int>>().subscribe(
            [&event_entity, &event_old_component_value, &event_new_component_value](auto evt) -> void {
                event_entity = evt.entity;
                event_old_component_value = evt.old_component;
                event_new_component_value = evt.new_component;
            });

    const auto created_entity = reg.create();
    reg.assign_or_replace(created_entity, old_component_value);
    reg.replace(created_entity, new_component_value);

    ASSERT_EQ(created_entity, event_entity);
    ASSERT_EQ(old_component_value, event_old_component_value);
    ASSERT_EQ(new_component_value, event_new_component_value);
}

TEST(basic_archetype_registry, component_removed_emit_event)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    auto event_entity = tempest::ecs::entity{tempest::ecs::tombstone};
    auto event_component_value = 0;

    constexpr auto component_value = 42;

    [[maybe_unused]] const auto subscription_handle =
        events.dispatcher<tempest::ecs::component_removed_event<tempest::ecs::entity, int>>().subscribe(
            [&event_entity, &event_component_value](auto evt) -> void {
                event_entity = evt.entity;
                event_component_value = evt.component;
            });

    const auto created_entity = reg.create();
    reg.assign_or_replace(created_entity, component_value);
    reg.remove<int>(created_entity);

    ASSERT_EQ(created_entity, event_entity);
    ASSERT_EQ(component_value, event_component_value);
}

TEST(basic_archetype_registry, component_removed_emit_event_with_multiple_components)
{
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    auto event_entity = tempest::ecs::entity{tempest::ecs::tombstone};
    auto event_component_value = 0;

    constexpr auto component_value = 42;

    [[maybe_unused]] const auto subscription_handle =
        events.dispatcher<tempest::ecs::component_removed_event<tempest::ecs::entity, int>>().subscribe(
            [&event_entity, &event_component_value](auto evt) -> void {
                event_entity = evt.entity;
                event_component_value = evt.component;
            });

    const auto created_entity = reg.create<int, float>();
    reg.assign_or_replace(created_entity, component_value);
    reg.assign_or_replace(created_entity, 3.14F);
    reg.remove<int>(created_entity);

    ASSERT_EQ(created_entity, event_entity);
    ASSERT_EQ(component_value, event_component_value);
}

/// @brief Verifies that calling try_get and has on an empty entity (one without any components)
/// safely returns nullptr and false respectively without crashing or indexing out of bounds.
TEST(basic_archetype_registry, try_get_and_has_on_empty_entity)
{
    // Setup registry and an empty entity with no components assigned
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    const auto entity = reg.create();

    // Act & Assert: Querying components on empty entity returns null / false safely
    EXPECT_EQ(reg.try_get<int>(entity), nullptr);
    EXPECT_FALSE(reg.has<int>(entity));
}

/// @brief Verifies that calling try_get and has on an entity whose components have all been removed
/// safely returns nullptr and false respectively without undefined behavior.
TEST(basic_archetype_registry, try_get_and_has_on_cleared_entity)
{
    // Setup entity with a single component then remove it completely
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);
    const auto entity = reg.create<int>();
    reg.assign_or_replace(entity, 123);

    // Act: Remove the only component, emptying the entity
    reg.remove<int>(entity);

    // Assert: try_get and has safely report absence
    EXPECT_EQ(reg.try_get<int>(entity), nullptr);
    EXPECT_FALSE(reg.has<int>(entity));
}

namespace
{
    struct alignas(16) aligned_16_component
    {
        float x = 1.0F;
        float y = 2.0F;
        float z = 3.0F;
        float w = 4.0F;
    };

    static_assert(tempest::ecs::component<aligned_16_component>);
} // namespace

/// @brief Verifies that removing an overaligned component (alignas(16)) captures the component
/// bytes with correct alignment and safely dispatches component_removed_event without segfaulting.
TEST(basic_archetype_registry, remove_overaligned_component_dispatches_event_safely)
{
    // Setup event registry and archetype registry
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    auto captured_entity = tempest::ecs::entity{tempest::ecs::tombstone};
    auto captured_value = aligned_16_component{};

    [[maybe_unused]] const auto subscription_handle =
        events.dispatcher<tempest::ecs::component_removed_event<tempest::ecs::entity, aligned_16_component>>().subscribe(
            [&captured_entity, &captured_value](const auto& evt) -> void {
                captured_entity = evt.entity;
                captured_value = evt.component;
            });

    const auto entity = reg.create<aligned_16_component, int>();
    const auto initial_component = aligned_16_component{
        .x = 10.0F,
        .y = 20.0F,
        .z = 30.0F,
        .w = 40.0F,
    };
    reg.assign_or_replace(entity, initial_component);
    reg.assign_or_replace(entity, 42);

    // Act: Remove the 16-byte aligned component
    reg.remove<aligned_16_component>(entity);

    // Assert: Event was published with correct entity and component contents
    EXPECT_EQ(captured_entity, entity);
    EXPECT_FLOAT_EQ(captured_value.x, 10.0F);
    EXPECT_FLOAT_EQ(captured_value.y, 20.0F);
    EXPECT_FLOAT_EQ(captured_value.z, 30.0F);
    EXPECT_FLOAT_EQ(captured_value.w, 40.0F);
    EXPECT_FALSE(reg.has<aligned_16_component>(entity));
    EXPECT_TRUE(reg.has<int>(entity));
}

/// @brief Verifies that replacing an existing overaligned component (alignas(16)) captures the old
/// component bytes with correct alignment and safely dispatches component_replaced_event without segfaulting.
TEST(basic_archetype_registry, replace_overaligned_component_dispatches_event_safely)
{
    // Setup event registry and archetype registry
    auto events = tempest::event::event_registry();
    auto types = tempest::ecs::component_type_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events, types);

    auto captured_entity = tempest::ecs::entity{tempest::ecs::tombstone};
    auto captured_old_value = aligned_16_component{};
    auto captured_new_value = aligned_16_component{};

    [[maybe_unused]] const auto subscription_handle =
        events.dispatcher<tempest::ecs::component_replaced_event<tempest::ecs::entity, aligned_16_component>>().subscribe(
            [&captured_entity, &captured_old_value, &captured_new_value](const auto& evt) -> void {
                captured_entity = evt.entity;
                captured_old_value = evt.old_component;
                captured_new_value = evt.new_component;
            });

    const auto entity = reg.create<aligned_16_component>();
    const auto old_component = aligned_16_component{
        .x = 1.0F,
        .y = 2.0F,
        .z = 3.0F,
        .w = 4.0F,
    };
    reg.assign_or_replace(entity, old_component);

    const auto new_component = aligned_16_component{
        .x = 100.0F,
        .y = 200.0F,
        .z = 300.0F,
        .w = 400.0F,
    };

    // Act: Replace the 16-byte aligned component
    reg.assign_or_replace(entity, new_component);

    // Assert: Replaced event was published with correct old and new contents
    EXPECT_EQ(captured_entity, entity);
    EXPECT_FLOAT_EQ(captured_old_value.x, 1.0F);
    EXPECT_FLOAT_EQ(captured_old_value.y, 2.0F);
    EXPECT_FLOAT_EQ(captured_old_value.z, 3.0F);
    EXPECT_FLOAT_EQ(captured_old_value.w, 4.0F);
    EXPECT_FLOAT_EQ(captured_new_value.x, 100.0F);
    EXPECT_FLOAT_EQ(captured_new_value.y, 200.0F);
    EXPECT_FLOAT_EQ(captured_new_value.z, 300.0F);
    EXPECT_FLOAT_EQ(captured_new_value.w, 400.0F);
}

//==============================================================================
// basic_entity_store & basic_entity_store_iterator Tests (P1-9 Bit-Skipping)
//==============================================================================

/// @brief Verifies that iterating an empty entity store does not execute any loop iterations,
/// and that stepping beyond boundaries behaves safely without crashing.
TEST(basic_entity_store_test, empty_store_iteration)
{
    // Setup empty store
    auto store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();

    // Assert begin equals end
    EXPECT_EQ(store.begin(), store.end());
    EXPECT_EQ(store.cbegin(), store.cend());

    auto iteration_count = 0;
    for ([[maybe_unused]] const auto ent : store)
    {
        ++iteration_count;
    }
    EXPECT_EQ(iteration_count, 0);

    // Boundary stepping on empty store
    auto it = store.begin();
    ++it;
    EXPECT_EQ(it, store.end());
    --it;
    EXPECT_EQ(it, store.begin());
}

/// @brief Verifies that forward and reverse iteration over densely populated blocks correctly
/// yields all entities in ascending and descending index order.
TEST(basic_entity_store_test, dense_forward_and_reverse_iteration)
{
    // Setup store with 130 contiguous entities spanning 3 blocks
    auto store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
    constexpr size_t total_entities = 130;
    auto created_entities = tempest::vector<tempest::ecs::entity>{};
    created_entities.reserve(total_entities);

    for (size_t i = 0; i < total_entities; ++i)
    {
        created_entities.push_back(store.acquire());
    }

    // Act: Forward iteration
    auto forward_visited = tempest::vector<tempest::ecs::entity>{};
    for (const auto ent : store)
    {
        forward_visited.push_back(ent);
    }

    // Assert forward match
    ASSERT_EQ(forward_visited.size(), total_entities);
    for (size_t i = 0; i < total_entities; ++i)
    {
        EXPECT_EQ(forward_visited[i], created_entities[i]);
    }

    // Act: Reverse iteration
    auto reverse_visited = tempest::vector<tempest::ecs::entity>{};
    auto rev_it = store.end();
    while (rev_it != store.begin())
    {
        --rev_it;
        reverse_visited.push_back(*rev_it);
    }

    // Assert reverse match
    ASSERT_EQ(reverse_visited.size(), total_entities);
    for (size_t i = 0; i < total_entities; ++i)
    {
        EXPECT_EQ(reverse_visited[i], created_entities[total_entities - 1 - i]);
    }
}

/// @brief Verifies that O(1) word-skipping skips across empty blocks and empty chunks correctly
/// during both forward and reverse iteration.
TEST(basic_entity_store_test, sparse_iteration_skips_words_correctly)
{
    // Setup store with 2 chunks capacity (8192 entities)
    auto store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
    constexpr size_t allocation_count = 8192;
    auto allocated = tempest::vector<tempest::ecs::entity>{};
    allocated.reserve(allocation_count);

    for (size_t i = 0; i < allocation_count; ++i)
    {
        allocated.push_back(store.acquire());
    }

    // Retain only specific sparse indices:
    // 0 (chunk 0, block 0, offset 0)
    // 63 (chunk 0, block 0, offset 63 - intra-block jump)
    // 64 (chunk 0, block 1, offset 0 - block boundary)
    // 127 (chunk 0, block 1, offset 63)
    // 4095 (chunk 0, block 63, offset 63 - chunk boundary)
    // 4096 (chunk 1, block 0, offset 0 - next chunk)
    // 8191 (chunk 1, block 63, offset 63 - last slot)
    auto is_retained = [](size_t i) -> bool {
        return i == 0 || i == 63 || i == 64 || i == 127 || i == 4095 || i == 4096 || i == 8191;
    };
    constexpr auto retained_count = size_t{7};

    for (size_t i = 0; i < allocation_count; ++i)
    {
        if (!is_retained(i))
        {
            store.release(allocated[i]);
        }
    }

    // Act: Forward iteration
    auto forward_visited = tempest::vector<tempest::ecs::entity>{};
    for (const auto ent : store)
    {
        forward_visited.push_back(ent);
    }

    // Assert forward match
    ASSERT_EQ(forward_visited.size(), retained_count);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(forward_visited[0]), 0);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(forward_visited[1]), 63);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(forward_visited[2]), 64);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(forward_visited[3]), 127);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(forward_visited[4]), 4095);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(forward_visited[5]), 4096);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(forward_visited[6]), 8191);

    // Act: Reverse iteration
    auto reverse_visited = tempest::vector<tempest::ecs::entity>{};
    auto rev_it = store.end();
    while (rev_it != store.begin())
    {
        --rev_it;
        reverse_visited.push_back(*rev_it);
    }

    // Assert reverse match
    ASSERT_EQ(reverse_visited.size(), retained_count);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(reverse_visited[0]), 8191);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(reverse_visited[1]), 4096);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(reverse_visited[2]), 4095);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(reverse_visited[3]), 127);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(reverse_visited[4]), 64);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(reverse_visited[5]), 63);
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(reverse_visited[6]), 0);
}

/// @brief Verifies bidirectional stepping consistency (++it followed by --it returns to original position).
TEST(basic_entity_store_test, bidirectional_step_consistency)
{
    auto store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
    auto allocated = tempest::vector<tempest::ecs::entity>{};
    for (size_t i = 0; i < 200; ++i)
    {
        allocated.push_back(store.acquire());
    }

    // Release every 3rd entity to produce arbitrary sparse holes
    for (size_t i = 0; i < 200; i += 3)
    {
        store.release(allocated[i]);
    }

    // Assert step consistency
    for (auto it = store.begin(); it != store.end(); ++it)
    {
        auto stepped = it;
        ++stepped;
        if (stepped != store.end())
        {
            --stepped;
            EXPECT_EQ(stepped, it);
        }
    }
}

/// @brief Verifies that const_iterator inter-operates seamlessly with non-const iterator.
TEST(basic_entity_store_test, const_iterator_support)
{
    auto store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
    const auto e1 = store.acquire();
    const auto e2 = store.acquire();

    const auto& const_store = store;
    auto cit = const_store.begin();
    EXPECT_EQ(*cit, e1);
    ++cit;
    EXPECT_EQ(*cit, e2);
    ++cit;
    EXPECT_EQ(cit, const_store.end());
    EXPECT_EQ(cit, const_store.cend());

    // Comparison across constness
    EXPECT_EQ(store.begin(), const_store.begin());
    EXPECT_EQ(store.end(), const_store.cend());
}

/// @brief Verifies iteration when an isolated entity is positioned at chunk and block boundaries.
TEST(basic_entity_store_test, single_entity_at_block_and_chunk_boundaries)
{
    constexpr auto test_indices = tempest::array<size_t, 5>{0, 63, 64, 4095, 4096};

    for (const auto target_idx : test_indices)
    {
        auto store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
        auto allocated = tempest::vector<tempest::ecs::entity>{};
        allocated.reserve(target_idx + 1);

        for (size_t i = 0; i <= target_idx; ++i)
        {
            allocated.push_back(store.acquire());
        }

        for (size_t i = 0; i < target_idx; ++i)
        {
            store.release(allocated[i]);
        }

        // Assert single entity found
        auto it = store.begin();
        ASSERT_NE(it, store.end());
        EXPECT_EQ(*it, allocated[target_idx]);

        ++it;
        EXPECT_EQ(it, store.end());

        --it;
        EXPECT_EQ(*it, allocated[target_idx]);
        EXPECT_EQ(it, store.begin());

        // Range-based for loop visits exactly this single entity
        auto visit_count = 0;
        for (const auto ent : store)
        {
            EXPECT_EQ(ent, allocated[target_idx]);
            ++visit_count;
        }
        EXPECT_EQ(visit_count, 1);
    }
}

//==============================================================================
// Empirical Challenger Stress Harnesses (Milestone 4 P1-9 Bit-Skipping)
//==============================================================================

/// @brief Adversarially stress-tests basic_entity_store_iterator under pathological
/// sparse allocation patterns at block and chunk boundaries (0, 63, 64, 127, 128, 4095).
/// Verifies strictly monotonic forward and reverse traversal and oscillating bidirectional stepping.
TEST(basic_entity_store_test, pathological_sparse_bit_skipping_stress)
{
    // Setup: Populate store across 4096 capacity chunk, then retain only boundary indices
    auto store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
    constexpr size_t total_alloc = 4096;
    auto allocated = tempest::vector<tempest::ecs::entity>{};
    allocated.reserve(total_alloc);

    for (size_t alloc_idx = 0; alloc_idx < total_alloc; ++alloc_idx)
    {
        allocated.push_back(store.acquire());
    }

    // Retain only pathological boundary targets:
    // Index 0: chunk 0, block 0, bit 0
    // Index 63: chunk 0, block 0, bit 63 (last bit of block 0)
    // Index 64: chunk 0, block 1, bit 0 (first bit of block 1)
    // Index 127: chunk 0, block 1, bit 63 (last bit of block 1)
    // Index 128: chunk 0, block 2, bit 0 (first bit of block 2)
    // Index 4095: chunk 0, block 63, bit 63 (last bit of chunk 0)
    auto is_target = [](size_t idx) -> bool {
        return idx == 0 || idx == 63 || idx == 64 || idx == 127 || idx == 128 || idx == 4095;
    };
    constexpr auto expected_count = size_t{6};
    const auto expected_targets = tempest::array<size_t, 6>{0, 63, 64, 127, 128, 4095};

    for (size_t alloc_idx = 0; alloc_idx < total_alloc; ++alloc_idx)
    {
        if (!is_target(alloc_idx))
        {
            store.release(allocated[alloc_idx]);
        }
    }

    ASSERT_EQ(store.size(), expected_count);

    // Act & Assert: Forward traversal (++it) visits strictly ascending target IDs
    auto forward_visited = tempest::vector<tempest::ecs::entity>{};
    for (auto it = store.begin(); it != store.end(); ++it)
    {
        forward_visited.push_back(*it);
    }

    ASSERT_EQ(forward_visited.size(), expected_count);
    for (size_t target_idx = 0; target_idx < expected_count; ++target_idx)
    {
        EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(forward_visited[target_idx]),
                  expected_targets[target_idx]);
    }

    // Act & Assert: Reverse traversal (--it) visits strictly descending target IDs
    auto reverse_visited = tempest::vector<tempest::ecs::entity>{};
    auto rev_it = store.end();
    while (rev_it != store.begin())
    {
        --rev_it;
        reverse_visited.push_back(*rev_it);
    }

    ASSERT_EQ(reverse_visited.size(), expected_count);
    for (size_t target_idx = 0; target_idx < expected_count; ++target_idx)
    {
        EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(reverse_visited[target_idx]),
                  expected_targets[expected_count - 1 - target_idx]);
    }

    // Act & Assert: Oscillating step test (++it; --it; ++it;) across block boundaries
    auto test_it = store.begin();
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 0);

    ++test_it; // jumps to 63
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 63);

    --test_it; // jumps back to 0
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 0);

    ++test_it; // back to 63
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 63);

    ++test_it; // crosses block boundary to 64
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 64);

    --test_it; // steps back across block boundary to 63
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 63);

    ++test_it; // forward to 64
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 64);

    ++test_it; // forward to 127
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 127);

    ++test_it; // forward across boundary to 128
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 128);

    --test_it; // back across boundary to 127
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 127);

    ++test_it; // forward to 128
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 128);

    ++test_it; // forward across 61 empty blocks to 4095
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 4095);

    --test_it; // back across 61 empty blocks to 128
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 128);

    ++test_it; // forward to 4095
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 4095);

    ++test_it; // reaches end()
    EXPECT_EQ(test_it, store.end());

    --test_it; // steps back from end() to last element 4095
    EXPECT_EQ(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*test_it), 4095);
}

/// @brief Verifies empty store invariants and boundary stepping across default, reserved,
/// and fully-drained stores.
TEST(basic_entity_store_test, empty_store_all_configurations_stepping_stress)
{
    // Case A: Default constructed store with zero chunks / zero capacity
    {
        auto empty_store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
        EXPECT_EQ(empty_store.size(), 0);
        EXPECT_EQ(empty_store.capacity(), 0);
        EXPECT_TRUE(empty_store.empty());
        EXPECT_EQ(empty_store.begin(), empty_store.end());
        EXPECT_EQ(empty_store.cbegin(), empty_store.cend());

        auto step_it = empty_store.begin();
        ++step_it;
        EXPECT_EQ(step_it, empty_store.end());
        --step_it;
        EXPECT_EQ(step_it, empty_store.begin());

        auto iter_count = 0;
        for ([[maybe_unused]] const auto ent : empty_store)
        {
            ++iter_count;
        }
        EXPECT_EQ(iter_count, 0);
    }

    // Case B: Reserved store with 4096 capacity but 0 elements
    {
        auto reserved_store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>(4096);
        EXPECT_EQ(reserved_store.size(), 0);
        EXPECT_GE(reserved_store.capacity(), 4096);
        EXPECT_TRUE(reserved_store.empty());
        EXPECT_EQ(reserved_store.begin(), reserved_store.end());
        EXPECT_EQ(reserved_store.cbegin(), reserved_store.cend());

        auto iter_count = 0;
        for ([[maybe_unused]] const auto ent : reserved_store)
        {
            ++iter_count;
        }
        EXPECT_EQ(iter_count, 0);
    }

    // Case C: Store where entities were allocated across multiple blocks and then all released
    {
        auto drained_store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
        constexpr auto alloc_count = size_t{256};
        auto temp_entities = tempest::vector<tempest::ecs::entity>{};
        temp_entities.reserve(alloc_count);

        for (size_t idx = 0; idx < alloc_count; ++idx)
        {
            temp_entities.push_back(drained_store.acquire());
        }
        EXPECT_EQ(drained_store.size(), alloc_count);

        for (size_t idx = 0; idx < alloc_count; ++idx)
        {
            drained_store.release(temp_entities[idx]);
        }
        EXPECT_EQ(drained_store.size(), 0);
        EXPECT_TRUE(drained_store.empty());
        EXPECT_EQ(drained_store.begin(), drained_store.end());
        EXPECT_EQ(drained_store.cbegin(), drained_store.cend());

        auto iter_count = 0;
        for ([[maybe_unused]] const auto ent : drained_store)
        {
            ++iter_count;
        }
        EXPECT_EQ(iter_count, 0);
    }
}

/// @brief Verifies full capacity saturation of basic_entity_store, testing that all 4096 entities
/// are visited without infinite loops or skips during forward and reverse traversal.
TEST(basic_entity_store_test, full_capacity_saturation_stress)
{
    // Setup: Fill entire chunk of 4096 entities (64 blocks * 64 bits = 4096 contiguous set bits)
    auto store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
    constexpr size_t capacity_limit = 4096;
    auto allocated = tempest::vector<tempest::ecs::entity>{};
    allocated.reserve(capacity_limit);

    for (size_t idx = 0; idx < capacity_limit; ++idx)
    {
        allocated.push_back(store.acquire());
    }

    ASSERT_EQ(store.size(), capacity_limit);

    // Act: Forward traversal
    auto forward_count = size_t{0};
    auto last_seen_id = size_t{0};
    for (auto it = store.begin(); it != store.end(); ++it)
    {
        const auto entity_id = static_cast<size_t>(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*it));
        if (forward_count > 0)
        {
            EXPECT_EQ(entity_id, last_seen_id + 1);
        }
        else
        {
            EXPECT_EQ(entity_id, 0);
        }
        last_seen_id = entity_id;
        ++forward_count;
    }
    EXPECT_EQ(forward_count, capacity_limit);
    EXPECT_EQ(last_seen_id, capacity_limit - 1);

    // Act: Reverse traversal
    auto reverse_count = size_t{0};
    auto rev_it = store.end();
    auto expected_next_id = capacity_limit - 1;
    while (rev_it != store.begin())
    {
        --rev_it;
        const auto entity_id = static_cast<size_t>(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*rev_it));
        EXPECT_EQ(entity_id, expected_next_id);
        if (expected_next_id > 0)
        {
            --expected_next_id;
        }
        ++reverse_count;
    }
    EXPECT_EQ(reverse_count, capacity_limit);
}

/// @brief Empirical oracle generator and differential test: creates a deterministic pseudorandom
/// distribution of allocated/released entities across 2048 slots and validates both forward and
/// reverse iterators against a ground-truth oracle.
TEST(basic_entity_store_test, randomized_sparse_oracle_fuzz_stress)
{
    // Setup: Deterministic linear congruential generator
    auto lcg_state = uint64_t{0x123456789ABCDEF0ULL};
    auto lcg_next = [&lcg_state]() -> uint64_t {
        lcg_state = (lcg_state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return lcg_state;
    };

    auto store = tempest::ecs::basic_entity_store<tempest::ecs::entity, 4096, uint64_t>();
    constexpr size_t total_candidates = 2048;
    auto allocated = tempest::vector<tempest::ecs::entity>{};
    allocated.reserve(total_candidates);

    for (size_t idx = 0; idx < total_candidates; ++idx)
    {
        allocated.push_back(store.acquire());
    }

    // Oracle tracking of surviving indices
    auto oracle = tempest::vector<size_t>{};
    for (size_t idx = 0; idx < total_candidates; ++idx)
    {
        // 25% retention probability using PRNG bits
        const auto retain = ((lcg_next() >> 32) & 0x3ULL) == 0ULL;
        if (retain)
        {
            oracle.push_back(idx);
        }
        else
        {
            store.release(allocated[idx]);
        }
    }

    ASSERT_EQ(store.size(), oracle.size());

    // Act: Forward iteration against oracle
    auto forward_visited = tempest::vector<size_t>{};
    for (const auto ent : store)
    {
        forward_visited.push_back(
            static_cast<size_t>(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(ent)));
    }

    // Assert forward matching
    ASSERT_EQ(forward_visited.size(), oracle.size());
    for (size_t idx = 0; idx < oracle.size(); ++idx)
    {
        EXPECT_EQ(forward_visited[idx], oracle[idx]);
    }

    // Act: Reverse iteration against oracle
    auto reverse_visited = tempest::vector<size_t>{};
    auto rev_it = store.end();
    while (rev_it != store.begin())
    {
        --rev_it;
        reverse_visited.push_back(
            static_cast<size_t>(tempest::ecs::entity_traits<tempest::ecs::entity>::as_entity(*rev_it)));
    }

    // Assert reverse matching
    ASSERT_EQ(reverse_visited.size(), oracle.size());
    for (size_t idx = 0; idx < oracle.size(); ++idx)
    {
        EXPECT_EQ(reverse_visited[idx], oracle[oracle.size() - 1 - idx]);
    }
}



