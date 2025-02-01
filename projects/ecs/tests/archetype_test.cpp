#include <tempest/archetype.hpp>

#include <cstring>
#include <gtest/gtest.h>

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
        .baz = 3.14f,
        .quux = 'q',
    };

    foo f2 = {
        .bar = 2,
        .baz = 6.28f,
        .quux = 'r',
    };

    auto dst1 = storage.element_at(0);
    std::memcpy(dst1, &f1, sizeof(f1));

    auto dst2 = storage.element_at(1);
    std::memcpy(dst2, &f2, sizeof(f2));

    ASSERT_TRUE(std::memcmp(&f1, dst1, sizeof(f1)) == 0);
    ASSERT_TRUE(std::memcmp(&f2, dst2, sizeof(f2)) == 0);

    storage.copy(0, 1);
    ASSERT_TRUE(std::memcmp(dst1, dst2, sizeof(foo)) == 0);
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

    float* f1 = reinterpret_cast<float*>(archetype.element_at(e1, 0));
    *f1 = 3.14f;
    float* f2 = reinterpret_cast<float*>(archetype.element_at(e2, 0));
    *f2 = 6.28f;

    ASSERT_EQ(3.14f, *reinterpret_cast<float*>(archetype.element_at(e1, 0)));
    ASSERT_EQ(6.28f, *reinterpret_cast<float*>(archetype.element_at(e2, 0)));

    ASSERT_TRUE(archetype.erase(e1));

    // Ensure the value got moved
    ASSERT_EQ(6.28f, *reinterpret_cast<float*>(archetype.element_at(e2, 0)));

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
    tempest::ecs::basic_archetype_registry reg;
    auto entity = reg.create<int, float>();

    reg.replace<int>(entity, 3);
    reg.replace<float>(entity, 3.14f);

    ASSERT_EQ(reg.size(), 1);
    ASSERT_EQ(reg.get<int>(entity), 3);
    ASSERT_EQ(reg.get<float>(entity), 3.14f);
}

TEST(basic_archetype_registry, create_initialized)
{
    tempest::ecs::basic_archetype_registry reg;
    auto entity = reg.create_initialized<int, float>(3, 3.14f);
    ASSERT_EQ(reg.size(), 1);
    ASSERT_EQ(reg.get<int>(entity), 3);
    ASSERT_EQ(reg.get<float>(entity), 3.14f);
}

TEST(basic_archetype_registry, create_swapped)
{
    tempest::ecs::basic_archetype_registry reg;
    auto entity = reg.create<float, int>();
    reg.replace<int>(entity, 3);
    reg.replace<float>(entity, 3.14f);
    ASSERT_EQ(reg.size(), 1);
    ASSERT_EQ(reg.get<int>(entity), 3);
    ASSERT_EQ(reg.get<float>(entity), 3.14f);
}

TEST(basic_archetype_registry, create_and_assign)
{
    tempest::ecs::basic_archetype_registry reg;
    auto entity = reg.create<int, float>();

    reg.assign_or_replace<int>(entity, 3);
    reg.assign_or_replace<float>(entity, 3.14f);

    reg.assign<char>(entity, 'c');

    ASSERT_EQ(reg.size(), 1);
    ASSERT_EQ(reg.get<int>(entity), 3);
    ASSERT_EQ(reg.get<float>(entity), 3.14f);
    ASSERT_EQ(reg.get<char>(entity), 'c');
}

TEST(basic_archetype_registry, has_component)
{
    tempest::ecs::basic_archetype_registry reg;
    auto entity = reg.create<int, float>();

    ASSERT_TRUE(reg.has<int>(entity));
    ASSERT_TRUE(reg.has<float>(entity));
    ASSERT_FALSE(reg.has<char>(entity));
}

TEST(basic_archetype_registry, remove_component)
{
    tempest::ecs::basic_archetype_registry reg;
    auto entity = reg.create<int, float>();
    reg.remove<int>(entity);
    ASSERT_FALSE(reg.has<int>(entity));
    ASSERT_TRUE(reg.has<float>(entity));
}

TEST(basic_archetype_registry, try_get_component_with_component)
{
    tempest::ecs::basic_archetype_registry reg;
    auto entity = reg.create<int, float>();
    reg.replace<int>(entity, 3);
    reg.replace<float>(entity, 3.14f);
    ASSERT_EQ(reg.try_get<int>(entity), &reg.get<int>(entity));
    ASSERT_EQ(reg.try_get<float>(entity), &reg.get<float>(entity));
}

TEST(basic_archetype_registry, try_get_component_with_failure)
{
    tempest::ecs::basic_archetype_registry reg;
    auto entity = reg.create<int, float>();
    reg.replace<int>(entity, 3);
    reg.replace<float>(entity, 3.14f);
    ASSERT_EQ(reg.try_get<char>(entity), nullptr);
}

TEST(basic_archetype_registry, remove_lots_of_components)
{
    tempest::ecs::basic_archetype_registry reg;
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
    tempest::ecs::basic_archetype_registry reg;
    auto e1 = reg.create<int, float>();
    auto e2 = reg.create<int, float, char>();
    auto e3 = reg.create<int, float, char, double>();
    auto e4 = reg.create<int, float, char, double, short>();
    auto e5 = reg.create<int, float, char, double, short, long>();
    auto e6 = reg.create<int, float, char, double, short, long, long long>();
    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<float>(e1, 3.14f);
    reg.assign_or_replace<int>(e2, 2);
    reg.assign_or_replace<float>(e2, 6.28f);
    reg.assign_or_replace<char>(e2, 'c');
    reg.assign_or_replace<int>(e3, 3);
    reg.assign_or_replace<float>(e3, 9.42f);
    reg.assign_or_replace<char>(e3, 'd');
    reg.assign_or_replace<double>(e3, 1.0);
    reg.assign_or_replace<int>(e4, 4);
    reg.assign_or_replace<float>(e4, 12.56f);
    reg.assign_or_replace<char>(e4, 'e');
    reg.assign_or_replace<double>(e4, 2.0);
    reg.assign_or_replace<short>(e4, 1);
    reg.assign_or_replace<int>(e5, 5);
    reg.assign_or_replace<float>(e5, 15.70f);
    reg.assign_or_replace<char>(e5, 'f');
    reg.assign_or_replace<double>(e5, 3.0);
    reg.assign_or_replace<short>(e5, 2);
    reg.assign_or_replace<long>(e5, 1);
    reg.assign_or_replace<int>(e6, 6);
    reg.assign_or_replace<float>(e6, 18.84f);
    reg.assign_or_replace<char>(e6, 'g');
    reg.assign_or_replace<double>(e6, 4.0);
    reg.assign_or_replace<short>(e6, 3);
    reg.assign_or_replace<long>(e6, 2);
    reg.assign_or_replace<long long>(e6, 1);

    ASSERT_EQ(reg.get<int>(e1), 1);
    ASSERT_EQ(reg.get<float>(e1), 3.14f);
    ASSERT_EQ(reg.get<int>(e2), 2);
    ASSERT_EQ(reg.get<float>(e2), 6.28f);
    ASSERT_EQ(reg.get<char>(e2), 'c');
    ASSERT_EQ(reg.get<int>(e3), 3);
    ASSERT_EQ(reg.get<float>(e3), 9.42f);
    ASSERT_EQ(reg.get<char>(e3), 'd');
    ASSERT_EQ(reg.get<double>(e3), 1.0);
    ASSERT_EQ(reg.get<int>(e4), 4);
    ASSERT_EQ(reg.get<float>(e4), 12.56f);
    ASSERT_EQ(reg.get<char>(e4), 'e');
    ASSERT_EQ(reg.get<double>(e4), 2.0);
    ASSERT_EQ(reg.get<short>(e4), 1);
    ASSERT_EQ(reg.get<int>(e5), 5);
    ASSERT_EQ(reg.get<float>(e5), 15.70f);
    ASSERT_EQ(reg.get<char>(e5), 'f');
    ASSERT_EQ(reg.get<double>(e5), 3.0);
    ASSERT_EQ(reg.get<short>(e5), 2);
    ASSERT_EQ(reg.get<long>(e5), 1);
    ASSERT_EQ(reg.get<int>(e6), 6);
    ASSERT_EQ(reg.get<float>(e6), 18.84f);
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
    tempest::ecs::basic_archetype_registry reg;
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

    auto func = [&sum](int i) { sum += i; };
    reg.each(func);

    ASSERT_EQ(21, sum);
}

TEST(basic_archetype_registry, each_single_component_no_match)
{
    tempest::ecs::basic_archetype_registry reg;
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
    auto func = [&sum](float f) { sum += f; };
    reg.each(func);
    ASSERT_EQ(0.0f, sum);
}

TEST(basic_archetype_registry, each_multiple_components_single_component_match)
{
    tempest::ecs::basic_archetype_registry reg;
    auto e1 = reg.create<int, float>();
    auto e2 = reg.create<int, float>();
    auto e3 = reg.create<int, float>();
    auto e4 = reg.create<int, float>();
    auto e5 = reg.create<int, float>();
    auto e6 = reg.create<int, float>();
    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<float>(e1, 3.14f);
    reg.assign_or_replace<int>(e2, 2);
    reg.assign_or_replace<float>(e2, 6.28f);
    reg.assign_or_replace<int>(e3, 3);
    reg.assign_or_replace<float>(e3, 9.42f);
    reg.assign_or_replace<int>(e4, 4);
    reg.assign_or_replace<float>(e4, 12.56f);
    reg.assign_or_replace<int>(e5, 5);
    reg.assign_or_replace<float>(e5, 15.70f);
    reg.assign_or_replace<int>(e6, 6);
    reg.assign_or_replace<float>(e6, 18.84f);
    // Check to make sure each entity has the correct value
    ASSERT_EQ(1, reg.get<int>(e1));
    ASSERT_EQ(3.14f, reg.get<float>(e1));
    ASSERT_EQ(2, reg.get<int>(e2));
    ASSERT_EQ(6.28f, reg.get<float>(e2));
    ASSERT_EQ(3, reg.get<int>(e3));
    ASSERT_EQ(9.42f, reg.get<float>(e3));
    ASSERT_EQ(4, reg.get<int>(e4));
    ASSERT_EQ(12.56f, reg.get<float>(e4));
    ASSERT_EQ(5, reg.get<int>(e5));
    ASSERT_EQ(15.70f, reg.get<float>(e5));
    ASSERT_EQ(6, reg.get<int>(e6));
    ASSERT_EQ(18.84f, reg.get<float>(e6));

    // Each on a lambda that matches the entity
    int sum = 0;

    auto func = [&sum](int i) { sum += i; };
    reg.each(func);

    ASSERT_EQ(21, sum);

    float fsum = 0.0f;
    auto func2 = [&fsum](float f) { fsum += f; };
    reg.each(func2);

    ASSERT_FLOAT_EQ(65.94f, fsum);
}

TEST(basic_archetype_registry, each_multiple_components_with_multiple_match_and_extra_components)
{
    tempest::ecs::basic_archetype_registry reg;
    auto e1 = reg.create<int, float, char>();
    auto e2 = reg.create<int, float, char>();
    auto e3 = reg.create<int, float, char>();
    auto e4 = reg.create<int, float, char>();
    auto e5 = reg.create<int, float, char>();
    auto e6 = reg.create<int, float, char>();
    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<float>(e1, 3.14f);
    reg.assign_or_replace<char>(e1, 'a');
    reg.assign_or_replace<int>(e2, 2);
    reg.assign_or_replace<float>(e2, 6.28f);
    reg.assign_or_replace<char>(e2, 'b');
    reg.assign_or_replace<int>(e3, 3);
    reg.assign_or_replace<float>(e3, 9.42f);
    reg.assign_or_replace<char>(e3, 'c');
    reg.assign_or_replace<int>(e4, 4);
    reg.assign_or_replace<float>(e4, 12.56f);
    reg.assign_or_replace<char>(e4, 'd');
    reg.assign_or_replace<int>(e5, 5);
    reg.assign_or_replace<float>(e5, 15.70f);
    reg.assign_or_replace<char>(e5, 'e');
    reg.assign_or_replace<int>(e6, 6);
    reg.assign_or_replace<float>(e6, 18.84f);
    reg.assign_or_replace<char>(e6, 'f');
    // Check to make sure each entity has the correct value
    ASSERT_EQ(1, reg.get<int>(e1));
    ASSERT_EQ(3.14f, reg.get<float>(e1));
    ASSERT_EQ('a', reg.get<char>(e1));
    ASSERT_EQ(2, reg.get<int>(e2));
    ASSERT_EQ(6.28f, reg.get<float>(e2));
    ASSERT_EQ('b', reg.get<char>(e2));
    ASSERT_EQ(3, reg.get<int>(e3));
    ASSERT_EQ(9.42f, reg.get<float>(e3));
    ASSERT_EQ('c', reg.get<char>(e3));
    ASSERT_EQ(4, reg.get<int>(e4));
    ASSERT_EQ(12.56f, reg.get<float>(e4));
    ASSERT_EQ('d', reg.get<char>(e4));
    ASSERT_EQ(5, reg.get<int>(e5));
    ASSERT_EQ(15.70f, reg.get<float>(e5));
    ASSERT_EQ(6, reg.get<int>(e6));
    ASSERT_EQ(18.84f, reg.get<float>(e6));

    // Each on a lambda that matches the entity
    int isum = 0;
    float fsum = 0.0f;

    auto func = [&isum, &fsum](int i, float f) {
        isum += i;
        fsum += f;
    };

    reg.each(func);
    ASSERT_EQ(21, isum);
    ASSERT_FLOAT_EQ(65.94f, fsum);
}

TEST(basic_archetype_registry, each_has_single_component_test_against_multiple)
{
    tempest::ecs::basic_archetype_registry reg;

    auto e1 = reg.create<int>();
    auto e2 = reg.create<int>();

    reg.assign_or_replace<int>(e1, 1);
    reg.assign_or_replace<int>(e2, 2);

    float fsum = 0.0f;

    auto func = [&fsum](float f) { fsum += f; };

    reg.each(func);

    ASSERT_EQ(0.0f, fsum);
}