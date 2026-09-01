#include <gtest/gtest.h>

#include <tempest/checked.hpp>
#include <tempest/vector.hpp>

namespace tempest::tests
{
    TEST(non_null_test, construct_from_reference)
    {
        int x = 42;
        auto nn = non_null<int>{x};

        EXPECT_EQ(*nn, 42);
        EXPECT_EQ(nn.get(), &x);

        *nn = 100;
        EXPECT_EQ(x, 100);
    }

    TEST(non_null_test, construct_from_pointer)
    {
        int x = 10;
        int* ptr = &x;
        auto nn = non_null<int>{ptr};

        EXPECT_EQ(*nn, 10);
        EXPECT_EQ(nn.get(), &x);
    }

    TEST(non_null_test, implicit_conversion_to_reference)
    {
        int x = 50;
        auto nn = non_null<int>{x};

        auto takes_ref = [](int& val) { val += 25; };

        takes_ref(nn);
        EXPECT_EQ(x, 75);
    }

    TEST(non_null_test, factory_create)
    {
        int x = 7;
        auto opt1 = non_null<int>::create(&x);
        ASSERT_TRUE(opt1.has_value());
        EXPECT_EQ(**opt1, 7);

        int* null_ptr = nullptr;
        auto opt2 = non_null<int>::create(null_ptr);
        EXPECT_FALSE(opt2.has_value());
    }

    TEST(non_null_test, factory_create_unchecked)
    {
        int x = 99;
        auto nn = non_null<int>::create_unchecked(&x);
        EXPECT_EQ(*nn, 99);
    }

    struct base
    {
        virtual ~base() = default;
        int a{1};
    };

    struct derived : base
    {
        int b{2};
    };

    TEST(non_null_test, derived_to_base_conversion)
    {
        derived d{};
        auto nn_derived = non_null<derived>{d};
        non_null<base> nn_base = nn_derived;

        EXPECT_EQ(nn_base->a, 1);
        EXPECT_EQ(nn_base.get(), &d);
    }

    TEST(non_null_test, equality_comparisons)
    {
        int a = 1;
        int b = 2;
        auto nn_a1 = non_null<int>{a};
        auto nn_a2 = non_null<int>{a};
        auto nn_b = non_null<int>{b};

        EXPECT_EQ(nn_a1, nn_a2);
        EXPECT_NE(nn_a1, nn_b);
        EXPECT_EQ(nn_a1, &a);
        EXPECT_NE(nn_a1, &b);
    }
} // namespace tempest::tests
