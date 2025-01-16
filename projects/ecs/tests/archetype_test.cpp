#include <tempest/archetype.hpp>

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