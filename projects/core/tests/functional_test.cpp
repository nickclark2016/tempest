#include <tempest/functional.hpp>

#include <gtest/gtest.h>

TEST(functional, invoke_lambda_non_void)
{
    auto foo = [](int a, int b) { return a + b; };

    auto invoke_result = tempest::invoke(foo, 1, 2);
    auto invoke_result_r = tempest::invoke_r<int>(foo, 1, 2);

    EXPECT_EQ(invoke_result, 3);
    EXPECT_EQ(invoke_result_r, 3);
}

TEST(functional, invoke_lambda_void)
{
    auto foo = []([[maybe_unused]] int a, [[maybe_unused]] int b) {};

    tempest::invoke(foo, 1, 2);
    tempest::invoke_r<void>(foo, 1, 2);

    // Intentionally no EXPECT_EQ, just checking that it compiles
}

TEST(functional, invoke_member_function_non_void)
{
    struct Foo
    {
        int add(int a, int b)
        {
            return a + b;
        }
    };

    Foo foo;

    auto invoke_result = tempest::invoke(&Foo::add, foo, 1, 2);
    auto invoke_result_r = tempest::invoke_r<int>(&Foo::add, foo, 1, 2);
}

TEST(functional, invoke_member_function_void)
{
    struct Foo
    {
        void add([[maybe_unused]] int a, [[maybe_unused]] int b)
        {
        }
    };

    Foo foo;

    tempest::invoke(&Foo::add, foo, 1, 2);
    tempest::invoke_r<void>(&Foo::add, foo, 1, 2);

    // Intentionally no EXPECT_EQ, just checking that it compiles
}

TEST(functional, invoke_static_member_function_non_void)
{
    struct Foo
    {
        static int add(int a, int b)
        {
            return a + b;
        }
    };

    auto invoke_result = tempest::invoke(&Foo::add, 1, 2);
    auto invoke_result_r = tempest::invoke_r<int>(&Foo::add, 1, 2);

    EXPECT_EQ(invoke_result, 3);
    EXPECT_EQ(invoke_result_r, 3);
}

TEST(functional, invoke_static_member_function_void)
{
    struct Foo
    {
        static void add([[maybe_unused]] int a, [[maybe_unused]] int b)
        {
        }
    };

    tempest::invoke(&Foo::add, 1, 2);
    tempest::invoke_r<void>(&Foo::add, 1, 2);

    // Intentionally no EXPECT_EQ, just checking that it compiles
}

TEST(functional, reference_wrapper_ref)
{
    int i = 42;

    auto r = tempest::ref(i);

    EXPECT_EQ(r.get(), 42);
    EXPECT_EQ(&r.get(), &i);
    int& actual = r.get();
    EXPECT_EQ(actual, 42);
}

TEST(functional, reference_wrapper_cref)
{
    int i = 42;

    auto r = tempest::cref(i);

    EXPECT_EQ(r.get(), 42);
    EXPECT_EQ(&r.get(), &i);
    const int& actual = r.get();
    EXPECT_EQ(actual, 42);
}

TEST(functional, reference_wrapper_call_operator)
{
    struct Foo
    {
        int operator()(int a, int b)
        {
            return a + b;
        }
    };

    Foo foo;

    auto r = tempest::ref(foo);

    auto result = r(1, 2);
    EXPECT_EQ(result, 3);
}

TEST(functional, function_default_constructor)
{
    tempest::function<int(int, int)> f;

    EXPECT_FALSE(f);
}

TEST(functional, function_constructor_lambda)
{
    tempest::function<int(int, int)> f = [](int a, int b) { return a + b; };

    EXPECT_TRUE(f);
    EXPECT_EQ(f(1, 2), 3);
}

TEST(functional, function_copy_constructor_empty)
{
    tempest::function<int(int, int)> f1;
    tempest::function<int(int, int)> f2 = f1;

    EXPECT_FALSE(f2);
}

TEST(functional, function_copy_constructor_from_lambda)
{
    tempest::function<int(int, int)> f1 = [](int a, int b) { return a + b; };
    tempest::function<int(int, int)> f2 = f1;

    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(1, 2), 3);
}

TEST(functional, function_move_constructor_from_empty)
{
    tempest::function<int(int, int)> f1;
    tempest::function<int(int, int)> f2 = tempest::move(f1);

    EXPECT_FALSE(f2);
}

TEST(functional, function_move_constructor_from_lambda)
{
    tempest::function<int(int, int)> f1 = [](int a, int b) { return a + b; };
    tempest::function<int(int, int)> f2 = tempest::move(f1);

    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(1, 2), 3);

    EXPECT_FALSE(f1);
}

TEST(functional, function_constructor_lambda_with_capture)
{
    int a = 1;
    int b = 2;

    tempest::function<int()> f = [&]() { return a + b; };

    EXPECT_TRUE(f);
    EXPECT_EQ(f(), 3);
}

TEST(functional, function_constructor_lambda_with_large_capture)
{
    std::vector<int> v(1000, 42);

    tempest::function<int()> f = [=]() { return v[0]; };

    EXPECT_TRUE(f);
    EXPECT_EQ(f(), 42);
}

TEST(functional, function_constructor_static_member)
{
    struct Foo
    {
        static int add(int a, int b)
        {
            return a + b;
        }
    };

    tempest::function<int(int, int)> f = &Foo::add;

    EXPECT_TRUE(f);
    EXPECT_EQ(f(1, 2), 3);
}

TEST(functional, function_assign_lambda_to_empty)
{
    tempest::function<int(int, int)> f(nullptr);
    f = [](int a, int b) { return a + b; };

    EXPECT_TRUE(f);
    EXPECT_EQ(f(1, 2), 3);
}

TEST(functional, function_assign_lambda_to_lambda)
{
    tempest::function<int(int, int)> f = [](int a, int b) { return a + b; };
    f = [](int a, int b) { return a * b; };

    EXPECT_TRUE(f);
    EXPECT_EQ(f(2, 3), 6);
}

TEST(functional, function_assign_lambda_to_static_member)
{
    struct Foo
    {
        static int add(int a, int b)
        {
            return a + b;
        }
    };

    tempest::function<int(int, int)> f = &Foo::add;
    f = [](int a, int b) { return a * b; };

    EXPECT_TRUE(f);
    EXPECT_EQ(f(2, 3), 6);
}

TEST(functional, function_assign_static_member_to_lambda)
{
    struct Foo
    {
        static int add(int a, int b)
        {
            return a + b;
        }
    };

    tempest::function<int(int, int)> f = [](int a, int b) { return a * b; };
    f = &Foo::add;

    EXPECT_TRUE(f);
    EXPECT_EQ(f(2, 3), 5);
}

TEST(functional, function_assign_empty_to_lambda)
{
    tempest::function<int(int, int)> f = [](int a, int b) { return a + b; };
    f = nullptr;

    EXPECT_FALSE(f);
}

TEST(functional, function_assign_empty_to_static_member)
{
    struct Foo
    {
        static int add(int a, int b)
        {
            return a + b;
        }
    };

    tempest::function<int(int, int)> f = &Foo::add;
    f = nullptr;

    EXPECT_FALSE(f);
}

TEST(functional, function_assign_empty_to_empty)
{
    tempest::function<int(int, int)> f1;
    tempest::function<int(int, int)> f2;
    f2 = f1;

    EXPECT_FALSE(f2);
}

TEST(functional, function_assign_empty_to_empty_move)
{
    tempest::function<int(int, int)> f1;
    tempest::function<int(int, int)> f2;
    f2 = tempest::move(f1);

    EXPECT_FALSE(f2);
}

TEST(functional, function_assign_lambda_to_empty_move)
{
    tempest::function<int(int, int)> f1 = [](int a, int b) { return a + b; };
    tempest::function<int(int, int)> f2;
    f2 = tempest::move(f1);

    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(1, 2), 3);
}

TEST(functional, function_assign_lambda_to_lambda_move)
{
    tempest::function<int(int, int)> f1 = [](int a, int b) { return a + b; };
    tempest::function<int(int, int)> f2 = [](int a, int b) { return a * b; };
    f2 = tempest::move(f1);

    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(1, 2), 3);
}

TEST(functional, function_assign_lambda_to_lambda_with_large_capture)
{
    std::vector<int> v(1000, 42);

    tempest::function<int()> f1 = [=]() { return v[0]; };
    tempest::function<int()> f2 = [=]() { return v[1]; };
    f2 = f1;

    EXPECT_TRUE(f2);
    EXPECT_EQ(f2(), 42);
}

TEST(functional, function_template_deduction_guides)
{
    auto lambda = [](int a, int b) { return a + b; };

    tempest::function f1 = lambda;
    tempest::function<int(int, int)> f2(lambda);

    EXPECT_TRUE(f1);
    EXPECT_TRUE(f2);
    EXPECT_EQ(f1(1, 2), 3);
    EXPECT_EQ(f2(1, 2), 3);
}