#include <gtest/gtest.h>

#include <tempest/meta.hpp>

struct Foo
{
};

struct Bar
{
};

TEST(tempest_meta, test_type_info)
{
    auto foo_type_info = tempest::core::type_info(std::in_place_type_t<Foo>{});
    auto bar_type_info = tempest::core::type_info(std::in_place_type_t<Bar>{});

    ASSERT_EQ(foo_type_info.name(), "Foo");
    ASSERT_EQ(bar_type_info.name(), "Bar");
    ASSERT_NE(foo_type_info.hash(), bar_type_info.hash());
    ASSERT_NE(foo_type_info.index(), bar_type_info.index());
}

TEST(tempest_meta, test_type_info_with_const)
{
    auto foo_type_info = tempest::core::type_id<Foo>();
    auto cfoo_type_info = tempest::core::type_id<const Foo>();

    ASSERT_EQ(foo_type_info.name(), "Foo");
    ASSERT_EQ(cfoo_type_info.name(), "Foo");
    ASSERT_EQ(foo_type_info.index(), cfoo_type_info.index());
    ASSERT_EQ(foo_type_info.hash(), cfoo_type_info.hash());
}