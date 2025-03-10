#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

#include <gtest/gtest.h>

TEST(type_traits, integral_constant_int)
{
    tempest::integral_constant<int, 42> ic;
    EXPECT_EQ(42, ic.value);
    EXPECT_EQ(42, ic());
    EXPECT_EQ(42, static_cast<int>(ic));
}

TEST(type_traits, bool_constant)
{
    tempest::bool_constant<true> bc;
    EXPECT_TRUE(bc.value);
    EXPECT_TRUE(bc());
    EXPECT_TRUE(static_cast<bool>(bc));
}

TEST(type_traits, true_type)
{
    tempest::true_type tt;
    EXPECT_TRUE(tt.value);
    EXPECT_TRUE(tt());
    EXPECT_TRUE(static_cast<bool>(tt));
}

TEST(type_traits, false_type)
{
    tempest::false_type ft;
    EXPECT_FALSE(ft.value);
    EXPECT_FALSE(ft());
    EXPECT_FALSE(static_cast<bool>(ft));
}

TEST(type_traits, is_fundamental)
{
    auto result = tempest::is_fundamental<int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_fundamental<float>::value;
    EXPECT_TRUE(result);

    result = tempest::is_fundamental<void>::value;
    EXPECT_TRUE(result);

    result = tempest::is_fundamental<int*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_fundamental<int&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_fundamental<int&&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_fundamental<int[]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_fundamental<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_fundamental<int()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_fundamental<int (*)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_fundamental<int (&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_fundamental<int (&&)()>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_integral)
{
    auto result = tempest::is_integral<int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_integral<void>::value;
    EXPECT_FALSE(result);

    // Check all integral types
    result = tempest::is_integral<bool>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<char>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<signed char>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<unsigned char>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<short>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<unsigned short>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<unsigned int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<long>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<unsigned long>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<long long>::value;
    EXPECT_TRUE(result);

    result = tempest::is_integral<unsigned long long>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_floating_point)
{
    auto result = tempest::is_floating_point<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_floating_point<float>::value;
    EXPECT_TRUE(result);

    result = tempest::is_floating_point<void>::value;
    EXPECT_FALSE(result);

    result = tempest::is_floating_point<double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_floating_point<long double>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_void)
{
    auto result = tempest::is_void<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_void<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_void<void>::value;
    EXPECT_TRUE(result);

    result = tempest::is_void<double>::value;
    EXPECT_FALSE(result);

    result = tempest::is_void<long double>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_same)
{
    auto result = tempest::is_same<int, int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_same<int, const int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_same<int, volatile int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_same<int, const volatile int>::value;
    EXPECT_FALSE(result);

    // Check to make sure the same type with same cv qualifiers is true
    result = tempest::is_same<const int, const int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<volatile int, volatile int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<const volatile int, const volatile int>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, remove_const)
{
    auto result = tempest::is_same<int, tempest::remove_const<const int>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_const<int>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_const<volatile int>::type>::value;
    EXPECT_FALSE(result);

    result = tempest::is_same<int, tempest::remove_const<const volatile int>::type>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, remove_volatile)
{
    auto result = tempest::is_same<int, tempest::remove_volatile<volatile int>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_volatile<int>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_volatile<const int>::type>::value;
    EXPECT_FALSE(result);

    result = tempest::is_same<int, tempest::remove_volatile<const volatile int>::type>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, remove_cv)
{
    auto result = tempest::is_same<int, tempest::remove_cv<const volatile int>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_cv<int>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_cv<const int>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_cv<volatile int>::type>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_lvalue_reference)
{
    auto result = tempest::is_lvalue_reference<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_lvalue_reference<int&>::value;
    EXPECT_TRUE(result);

    result = tempest::is_lvalue_reference<int&&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_lvalue_reference<int*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_lvalue_reference<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_lvalue_reference<int (*)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_lvalue_reference<int (&)()>::value;
    EXPECT_TRUE(result);

    result = tempest::is_lvalue_reference<int (&&)()>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_rvalue_reference)
{
    auto result = tempest::is_rvalue_reference<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_rvalue_reference<int&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_rvalue_reference<int&&>::value;
    EXPECT_TRUE(result);

    result = tempest::is_rvalue_reference<int*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_rvalue_reference<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_rvalue_reference<int (*)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_rvalue_reference<int (&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_rvalue_reference<int (&&)()>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_reference)
{
    auto result = tempest::is_reference<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_reference<int&>::value;
    EXPECT_TRUE(result);

    result = tempest::is_reference<int&&>::value;
    EXPECT_TRUE(result);

    result = tempest::is_reference<int*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_reference<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_reference<int (*)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_reference<int (&)()>::value;
    EXPECT_TRUE(result);

    result = tempest::is_reference<int (&&)()>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_array)
{
    auto result = tempest::is_array<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_array<int&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_array<int&&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_array<int*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_array<int[5]>::value;
    EXPECT_TRUE(result);

    result = tempest::is_array<int (*)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_array<int (&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_array<int (&&)()>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_enum)
{
    enum class Enum
    {
        A,
        B,
        C
    };
    auto result = tempest::is_enum<Enum>::value;
    EXPECT_TRUE(result);

    result = tempest::is_enum<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_enum<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_enum<void>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_union)
{
    union Union {
        int a;
        float b;
    };
    auto result = tempest::is_union<Union>::value;
    EXPECT_TRUE(result);

    result = tempest::is_union<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_union<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_union<void>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_scalar)
{
    auto result = tempest::is_scalar<int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_scalar<float>::value;
    EXPECT_TRUE(result);

    result = tempest::is_scalar<void>::value;
    EXPECT_FALSE(result);

    result = tempest::is_scalar<int*>::value;
    EXPECT_TRUE(result);

    result = tempest::is_scalar<int&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_scalar<int&&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_scalar<int[]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_scalar<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_scalar<int (*)()>::value;
    EXPECT_TRUE(result);

    result = tempest::is_scalar<int (&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_scalar<int (&&)()>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_function)
{
    auto result = tempest::is_function<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<void>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<int*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<int&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<int&&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<int[]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<int (*)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<int (&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<int (&&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_function<void()>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_object)
{
    auto result = tempest::is_object<int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_object<float>::value;
    EXPECT_TRUE(result);

    result = tempest::is_object<void>::value;
    EXPECT_FALSE(result);

    result = tempest::is_object<int*>::value;
    EXPECT_TRUE(result);

    result = tempest::is_object<int&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_object<int&&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_object<int[]>::value;
    EXPECT_TRUE(result);

    result = tempest::is_object<int[5]>::value;
    EXPECT_TRUE(result);

    result = tempest::is_object<int (*)()>::value;
    EXPECT_TRUE(result);

    result = tempest::is_object<int (&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_object<int (&&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_object<void()>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_compound)
{
    auto result = tempest::is_compound<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_compound<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_compound<void>::value;
    EXPECT_FALSE(result);

    result = tempest::is_compound<int*>::value;
    EXPECT_TRUE(result);

    result = tempest::is_compound<int&>::value;
    EXPECT_TRUE(result);

    result = tempest::is_compound<int&&>::value;
    EXPECT_TRUE(result);

    result = tempest::is_compound<int[]>::value;
    EXPECT_TRUE(result);

    result = tempest::is_compound<int[5]>::value;
    EXPECT_TRUE(result);

    result = tempest::is_compound<int (*)()>::value;
    EXPECT_TRUE(result);

    result = tempest::is_compound<int (&)()>::value;
    EXPECT_TRUE(result);

    result = tempest::is_compound<int (&&)()>::value;
    EXPECT_TRUE(result);

    result = tempest::is_compound<void()>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_member_pointer)
{
    auto result = tempest::is_member_pointer<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<void>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<int*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<int&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<int&&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<int[]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<int (*)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<int (&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<int (&&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_pointer<void()>::value;
    EXPECT_FALSE(result);

    struct S
    {
    };
    result = tempest::is_member_pointer<int S::*>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_member_object_pointer)
{
    auto result = tempest::is_member_object_pointer<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<void>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<int*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<int&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<int&&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<int[]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<int (*)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<int (&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<int (&&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_object_pointer<void()>::value;
    EXPECT_FALSE(result);

    struct S
    {
    };
    result = tempest::is_member_object_pointer<int S::*>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_member_function_pointer)
{
    auto result = tempest::is_member_function_pointer<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<void>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<int*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<int&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<int&&>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<int[]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<int (*)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<int (&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<int (&&)()>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<void()>::value;
    EXPECT_FALSE(result);

    struct S
    {
    };
    result = tempest::is_member_function_pointer<int S::*>::value;
    EXPECT_FALSE(result);

    result = tempest::is_member_function_pointer<int (S::*)()>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_empty)
{
    struct Empty
    {
    };
    auto result = tempest::is_empty<Empty>::value;
    EXPECT_TRUE(result);

    struct NonEmpty
    {
        int a;
    };
    result = tempest::is_empty<NonEmpty>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_trivial)
{
    struct Trivial
    {
    };
    auto result = tempest::is_trivial<Trivial>::value;
    EXPECT_TRUE(result);

    struct NonTrivial
    {
        NonTrivial()
        {
        }
    };
    result = tempest::is_trivial<NonTrivial>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_trivially_copyable)
{
    struct TriviallyCopyable
    {
    };
    auto result = tempest::is_trivially_copyable<TriviallyCopyable>::value;
    EXPECT_TRUE(result);

    struct NonTriviallyCopyable
    {
        NonTriviallyCopyable(const NonTriviallyCopyable&)
        {
        }
    };
    result = tempest::is_trivially_copyable<NonTriviallyCopyable>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_standard_layout)
{
    struct StandardLayout
    {
        int a;
    };
    auto result = tempest::is_standard_layout<StandardLayout>::value;
    EXPECT_TRUE(result);

    struct NonStandardLayout
    {
        int a;
        virtual void foo()
        {
        }
    };
    result = tempest::is_standard_layout<NonStandardLayout>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, has_unique_object_representations)
{
    struct UniqueObjectRepresentations
    {
        unsigned int a;
        unsigned int b;
    };
    auto result = tempest::has_unique_object_representations<UniqueObjectRepresentations>::value;
    EXPECT_TRUE(result);

    struct NonUniqueObjectRepresentations
    {
        unsigned char a;
        unsigned short s;
        unsigned int i;
    };
    result = tempest::has_unique_object_representations<NonUniqueObjectRepresentations>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_polymorphic)
{
    struct Polymorphic
    {
        virtual void foo()
        {
        }
    };
    auto result = tempest::is_polymorphic<Polymorphic>::value;
    EXPECT_TRUE(result);

    struct NonPolymorphic
    {
    };
    result = tempest::is_polymorphic<NonPolymorphic>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_abstract)
{
    struct Abstract
    {
        virtual void foo() = 0;
    };
    auto result = tempest::is_abstract<Abstract>::value;
    EXPECT_TRUE(result);

    struct NonAbstract
    {
        virtual void foo()
        {
        }
    };
    result = tempest::is_abstract<NonAbstract>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_final)
{
    struct Final final
    {
    };
    auto result = tempest::is_final<Final>::value;
    EXPECT_TRUE(result);

    struct NonFinal
    {
    };
    result = tempest::is_final<NonFinal>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_aggregate)
{
    struct Aggregate
    {
        int a;
        int b;
    };
    auto result = tempest::is_aggregate<Aggregate>::value;
    EXPECT_TRUE(result);

    struct NonAggregate
    {
        NonAggregate()
        {
        }
    };
    result = tempest::is_aggregate<NonAggregate>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_signed)
{
    auto result = tempest::is_signed<int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_signed<unsigned int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_signed<float>::value;
    EXPECT_TRUE(result);

    result = tempest::is_signed<double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_signed<long double>::value;
    EXPECT_TRUE(result);

    struct Foo
    {
    };
    result = tempest::is_signed<Foo>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_unsigned)
{
    auto result = tempest::is_unsigned<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_unsigned<unsigned int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_unsigned<float>::value;
    EXPECT_FALSE(result);

    result = tempest::is_unsigned<double>::value;
    EXPECT_FALSE(result);

    result = tempest::is_unsigned<long double>::value;
    EXPECT_FALSE(result);

    struct Foo
    {
    };
    result = tempest::is_unsigned<Foo>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_bounded_array)
{
    auto result = tempest::is_bounded_array<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_bounded_array<int[]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_bounded_array<int[5]>::value;
    EXPECT_TRUE(result);

    result = tempest::is_bounded_array<int[5][5]>::value;
    EXPECT_TRUE(result);

    result = tempest::is_bounded_array<int[][5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_bounded_array<int[][5][5]>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_unbounded_array)
{
    auto result = tempest::is_unbounded_array<int>::value;
    EXPECT_FALSE(result);

    result = tempest::is_unbounded_array<int[]>::value;
    EXPECT_TRUE(result);

    result = tempest::is_unbounded_array<int[5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_unbounded_array<int[5][5]>::value;
    EXPECT_FALSE(result);

    result = tempest::is_unbounded_array<int[][5]>::value;
    EXPECT_TRUE(result);

    result = tempest::is_unbounded_array<int[][5][5]>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, add_pointer)
{
    auto result = tempest::is_same<int*, tempest::add_pointer<int>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int*, tempest::add_pointer<int*>::type>::value;
    EXPECT_FALSE(result);

    result = tempest::is_same<int**, tempest::add_pointer<int*>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int**, tempest::add_pointer<int**>::type>::value;
    EXPECT_FALSE(result);

    result = tempest::is_same<int***, tempest::add_pointer<int**>::type>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, remove_pointer)
{
    auto result = tempest::is_same<int, tempest::remove_pointer<int*>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_pointer<int>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_pointer<int**>::type>::value;
    EXPECT_FALSE(result);

    result = tempest::is_same<int*, tempest::remove_pointer<int**>::type>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<int, tempest::remove_pointer<int***>::type>::value;
    EXPECT_FALSE(result);

    result = tempest::is_same<int**, tempest::remove_pointer<int***>::type>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_constructible)
{
    struct Foo
    {
        Foo(int)
        {
        }
    };
    auto result = tempest::is_constructible<Foo, int>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_constructible<Foo, Baz>::value;
    EXPECT_FALSE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_constructible<Bar>::value;
    EXPECT_FALSE(result);

    result = tempest::is_constructible<Baz>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_trivially_constructible)
{
    struct Foo
    {
        Foo(int)
        {
        }
    };
    auto result = tempest::is_trivially_constructible<Foo, int>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_trivially_constructible<Foo, Baz>::value;
    EXPECT_FALSE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_trivially_constructible<Bar>::value;
    EXPECT_FALSE(result);

    result = tempest::is_trivially_constructible<Baz>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_nothrow_constructible)
{
    struct Foo
    {
        Foo(int) noexcept
        {
        }
    };
    auto result = tempest::is_nothrow_constructible<Foo, int>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() noexcept = default;
    };
    result = tempest::is_nothrow_constructible<Foo, Baz>::value;
    EXPECT_FALSE(result);

    struct Bar
    {
        Bar() noexcept = delete;
    };
    result = tempest::is_nothrow_constructible<Bar>::value;
    EXPECT_FALSE(result);

    result = tempest::is_nothrow_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux() noexcept(false) = default;
    };
    result = tempest::is_nothrow_constructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_default_constructible)
{
    struct Foo
    {
        Foo(int)
        {
        }
    };
    auto result = tempest::is_default_constructible<Foo>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_default_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_default_constructible<Bar>::value;
    EXPECT_FALSE(result);

    // Check with default argument in constructor
    struct Qux
    {
        Qux(int = 0)
        {
        }
    };
    result = tempest::is_default_constructible<Qux>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_trivially_default_constructible)
{
    struct Foo
    {
        Foo(int)
        {
        }
    };
    auto result = tempest::is_trivially_default_constructible<Foo>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_trivially_default_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_trivially_default_constructible<Bar>::value;
    EXPECT_FALSE(result);

    // Check with default argument in constructor
    struct Qux
    {
        Qux(int = 0)
        {
        }
    };
    result = tempest::is_trivially_default_constructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_nothrow_default_constructible)
{
    struct Foo
    {
        Foo(int) noexcept
        {
        }
    };
    auto result = tempest::is_nothrow_default_constructible<Foo>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() noexcept = default;
    };
    result = tempest::is_nothrow_default_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() noexcept = delete;
    };
    result = tempest::is_nothrow_default_constructible<Bar>::value;
    EXPECT_FALSE(result);

    struct Qux
    {
        Qux() noexcept(false) = default;
    };
    result = tempest::is_nothrow_default_constructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_copy_constructible)
{
    struct Foo
    {
        Foo(const Foo&)
        {
        }
    };
    auto result = tempest::is_copy_constructible<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_copy_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_copy_constructible<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux(const Qux&) = delete;
    };
    result = tempest::is_copy_constructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_trivially_copy_constructible)
{
    struct Foo
    {
        Foo(const Foo&)
        {
        }
    };
    auto result = tempest::is_trivially_copy_constructible<Foo>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_trivially_copy_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_trivially_copy_constructible<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux(const Qux&) = delete;
    };
    result = tempest::is_trivially_copy_constructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_nothrow_copy_constructible)
{
    struct Foo
    {
        Foo(const Foo&) noexcept
        {
        }
    };
    auto result = tempest::is_nothrow_copy_constructible<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() noexcept = default;
    };
    result = tempest::is_nothrow_copy_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() noexcept = delete;
    };
    result = tempest::is_nothrow_copy_constructible<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux(const Qux&) noexcept(false) = default;
    };
    result = tempest::is_nothrow_copy_constructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_move_constructible)
{
    struct Foo
    {
        Foo(Foo&&)
        {
        }
    };
    auto result = tempest::is_move_constructible<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_move_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_move_constructible<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux(Qux&&) = delete;
    };
    result = tempest::is_move_constructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_trivially_move_constructible)
{
    struct Foo
    {
        Foo(Foo&&)
        {
        }
    };
    auto result = tempest::is_trivially_move_constructible<Foo>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_trivially_move_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_trivially_move_constructible<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux(Qux&&) = delete;
    };
    result = tempest::is_trivially_move_constructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_nothrow_move_constructible)
{
    struct Foo
    {
        Foo(Foo&&) noexcept
        {
        }
    };
    auto result = tempest::is_nothrow_move_constructible<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() noexcept = default;
    };
    result = tempest::is_nothrow_move_constructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() noexcept = delete;
    };
    result = tempest::is_nothrow_move_constructible<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux(Qux&&) noexcept(false) = default;
    };
    result = tempest::is_nothrow_move_constructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_assignable)
{
    struct Foo
    {
        Foo& operator=(const Foo&)
        {
            return *this;
        }
    };
    auto result = tempest::is_assignable<Foo, Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_assignable<Foo, Baz>::value;
    EXPECT_FALSE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_assignable<Bar, Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux& operator=(Qux&&) = delete;
    };
    result = tempest::is_assignable<Qux, Qux>::value;
    EXPECT_FALSE(result);

    // Assign from a type not matching the assignment operator
    struct Quux
    {
        Quux& operator=(int)
        {
            return *this;
        }
    };
    result = tempest::is_assignable<Quux, Foo>::value;
    EXPECT_FALSE(result);

    // Assign from a type matching the assignment operator
    struct Quuux
    {
        Quuux& operator=(int)
        {
            return *this;
        }
    };
    result = tempest::is_assignable<Quuux, int>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_trivially_assignable)
{
    struct Foo
    {
        Foo& operator=(const Foo&)
        {
            return *this;
        }
    };
    auto result = tempest::is_trivially_assignable<Foo, Foo>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_trivially_assignable<Foo, Baz>::value;
    EXPECT_FALSE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_trivially_assignable<Bar, Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux& operator=(Qux&&) = delete;
    };
    result = tempest::is_trivially_assignable<Qux, Qux>::value;
    EXPECT_FALSE(result);

    // Assign from a type not matching the assignment operator
    struct Quux
    {
        Quux& operator=(int)
        {
            return *this;
        }
    };
    result = tempest::is_trivially_assignable<Quux, Foo>::value;
    EXPECT_FALSE(result);

    // Assign from a type matching the assignment operator
    struct Quuux
    {
        Quuux& operator=(int)
        {
            return *this;
        }
    };
    result = tempest::is_trivially_assignable<Quuux, int>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_nothrow_assignable)
{
    struct Foo
    {
        Foo& operator=(const Foo&) noexcept
        {
            return *this;
        }
    };
    auto result = tempest::is_nothrow_assignable<Foo, Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() noexcept = default;
    };
    result = tempest::is_nothrow_assignable<Foo, Baz>::value;
    EXPECT_FALSE(result);

    struct Bar
    {
        Bar() noexcept = delete;
    };
    result = tempest::is_nothrow_assignable<Bar, Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux& operator=(Qux&&) noexcept = delete;
    };
    result = tempest::is_nothrow_assignable<Qux, Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_copy_assignable)
{
    struct Foo
    {
        Foo& operator=(const Foo&)
        {
            return *this;
        }
    };
    auto result = tempest::is_copy_assignable<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_copy_assignable<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_copy_assignable<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux& operator=(Qux&&) = delete;
    };
    result = tempest::is_copy_assignable<Qux>::value;
    EXPECT_FALSE(result);

    struct Quux
    {
        Quux& operator=(const Quux&) = delete;
    };
    result = tempest::is_copy_assignable<Quux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_trivially_copy_assignable)
{
    struct Foo
    {
        Foo& operator=(const Foo&)
        {
            return *this;
        }
    };
    auto result = tempest::is_trivially_copy_assignable<Foo>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_trivially_copy_assignable<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_trivially_copy_assignable<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux& operator=(Qux&&) = delete;
    };
    result = tempest::is_trivially_copy_assignable<Qux>::value;
    EXPECT_FALSE(result);

    struct Quux
    {
        Quux& operator=(const Quux&) = delete;
    };
    result = tempest::is_trivially_copy_assignable<Quux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_nothrow_copy_assignable)
{
    struct Foo
    {
        Foo& operator=(const Foo&) noexcept
        {
            return *this;
        }
    };
    auto result = tempest::is_nothrow_copy_assignable<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() noexcept = default;
    };
    result = tempest::is_nothrow_copy_assignable<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() noexcept = delete;
    };
    result = tempest::is_nothrow_copy_assignable<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux& operator=(Qux&&) noexcept = delete;
    };
    result = tempest::is_nothrow_copy_assignable<Qux>::value;
    EXPECT_FALSE(result);

    struct Quux
    {
        Quux& operator=(const Quux&) noexcept = delete;
    };
    result = tempest::is_nothrow_copy_assignable<Quux>::value;
    EXPECT_FALSE(result);

#ifndef _MSC_VER
    struct Quuux
    {
        Quuux& operator=(const Quuux&) noexcept(false) = default;
    };
    result = tempest::is_nothrow_copy_assignable<Quuux>::value;
    EXPECT_FALSE(result);
#endif

    struct Quuuux
    {
        Quuuux& operator=(const Quuuux&) noexcept(false)
        {
            return *this;
        }
    };
    result = tempest::is_nothrow_copy_assignable<Quuuux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_move_assignable)
{
    struct Foo
    {
        Foo& operator=(Foo&&)
        {
            return *this;
        }
    };
    auto result = tempest::is_move_assignable<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_move_assignable<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_move_assignable<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux& operator=(Qux&&) = delete;
    };
    result = tempest::is_move_assignable<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_trivially_move_assignable)
{
    struct Foo
    {
        Foo& operator=(Foo&&)
        {
            return *this;
        }
    };
    auto result = tempest::is_trivially_move_assignable<Foo>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() = default;
    };
    result = tempest::is_trivially_move_assignable<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = delete;
    };
    result = tempest::is_trivially_move_assignable<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux& operator=(Qux&&) = delete;
    };
    result = tempest::is_trivially_move_assignable<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_nothrow_move_assignable)
{
    struct Foo
    {
        Foo& operator=(Foo&&) noexcept
        {
            return *this;
        }
    };
    auto result = tempest::is_nothrow_move_assignable<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() noexcept = default;
    };
    result = tempest::is_nothrow_move_assignable<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() noexcept = delete;
    };
    result = tempest::is_nothrow_move_assignable<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        Qux& operator=(Qux&&) noexcept = delete;
    };
    result = tempest::is_nothrow_move_assignable<Qux>::value;
    EXPECT_FALSE(result);

#ifndef _MSC_VER
    struct Quux
    {
        Quux& operator=(Quux&&) noexcept(false) = default;
    };
    result = tempest::is_nothrow_move_assignable<Quux>::value;
    EXPECT_FALSE(result);
#endif

    struct Quuux
    {
        Quuux& operator=(Quuux&&) noexcept(false)
        {
            return *this;
        }
    };
    result = tempest::is_nothrow_move_assignable<Quuux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_destructible)
{
    struct Foo
    {
        ~Foo()
        {
        }
    };
    auto result = tempest::is_destructible<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() = delete;
    };
    result = tempest::is_destructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = default;
    };
    result = tempest::is_destructible<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        ~Qux() = delete;
    };
    result = tempest::is_destructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_trivially_destructible)
{
    struct Foo
    {
        ~Foo()
        {
        }
    };
    auto result = tempest::is_trivially_destructible<Foo>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        Baz() = delete;
    };
    result = tempest::is_trivially_destructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() = default;
    };
    result = tempest::is_trivially_destructible<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        ~Qux() = delete;
    };
    result = tempest::is_trivially_destructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_nothrow_destructible)
{
    struct Foo
    {
        ~Foo() noexcept
        {
        }
    };
    auto result = tempest::is_nothrow_destructible<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        Baz() noexcept = delete;
    };
    result = tempest::is_nothrow_destructible<Baz>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        Bar() noexcept = default;
    };
    result = tempest::is_nothrow_destructible<Bar>::value;
    EXPECT_TRUE(result);

    struct Qux
    {
        ~Qux() noexcept = delete;
    };
    result = tempest::is_nothrow_destructible<Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, has_virtual_destructor)
{
    struct Foo
    {
        virtual ~Foo()
        {
        }
    };
    auto result = tempest::has_virtual_destructor<Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        ~Baz() = default;
    };
    result = tempest::has_virtual_destructor<Baz>::value;
    EXPECT_FALSE(result);

    struct Bar
    {
        ~Bar() = delete;
    };
    result = tempest::has_virtual_destructor<Bar>::value;
    EXPECT_FALSE(result);
}

namespace
{
    // Swappable types and functions
    struct SwappableType1
    {
        int a;
    };
    struct SwappableType2
    {
        int b;
    };

    void swap(SwappableType1& lhs, SwappableType1& rhs)
    {
        tempest::swap(lhs.a, rhs.a);
    }

    void swap(SwappableType2& lhs, SwappableType2& rhs)
    {
        tempest::swap(lhs.b, rhs.b);
    }

    // Swap with each other
    void swap(SwappableType1& lhs, SwappableType2& rhs)
    {
        tempest::swap(lhs.a, rhs.b);
    }

    void swap(SwappableType2& lhs, SwappableType1& rhs)
    {
        tempest::swap(lhs.b, rhs.a);
    }

    // Nothrow swappable types and functions
    struct NothrowSwappableType1
    {
    };
    struct NothrowSwappableType2
    {
    };

    void swap(NothrowSwappableType1&, NothrowSwappableType1&) noexcept
    {
    }
    void swap(NothrowSwappableType2&, NothrowSwappableType2&) noexcept
    {
    }
    // Swap with each other
    void swap(NothrowSwappableType1&, NothrowSwappableType2&) noexcept
    {
    }
    void swap(NothrowSwappableType2&, NothrowSwappableType1&) noexcept
    {
    }
} // namespace

TEST(type_traits, is_swappable_with)
{
    struct Foo
    {
        Foo() = default;
    };
    struct Bar
    {
        Bar() = default;
    };
    auto result = tempest::is_swappable_with<Foo, Bar>::value;
    EXPECT_FALSE(result);

    result = tempest::is_swappable_with<SwappableType1, SwappableType2>::value;
    EXPECT_FALSE(result);

    result = tempest::is_swappable_with<SwappableType2, SwappableType1>::value;
    EXPECT_FALSE(result);

    {
        // Swap 1 and 1
        SwappableType1 a{.a = 1};
        SwappableType1 b{.a = 2};

        swap(a, b);

        EXPECT_EQ(a.a, 2);
        EXPECT_EQ(b.a, 1);
    }

    {
        // Swap 1 and 2
        SwappableType1 a{.a = 1};
        SwappableType2 b{.b = 2};

        swap(a, b);

        EXPECT_EQ(a.a, 2);
        EXPECT_EQ(b.b, 1);
    }

    {
        // Swap 2 and 1
        SwappableType2 a{.b = 1};
        SwappableType1 b{.a = 2};

        swap(a, b);

        EXPECT_EQ(a.b, 2);
        EXPECT_EQ(b.a, 1);
    }

    {
        // Swap 2 and 2
        SwappableType2 a{.b = 1};
        SwappableType2 b{.b = 2};

        swap(a, b);

        EXPECT_EQ(a.b, 2);
        EXPECT_EQ(b.b, 1);
    }

    {
        // Swap 1 and 1
        NothrowSwappableType1 a;
        NothrowSwappableType1 b;

        swap(a, b);
    }

    {
        // Swap 1 and 2
        NothrowSwappableType1 a;
        NothrowSwappableType2 b;

        swap(a, b);
    }

    {
        // Swap 2 and 1
        NothrowSwappableType2 a;
        NothrowSwappableType1 b;

        swap(a, b);
    }

    {
        // Swap 2 and 2
        NothrowSwappableType2 a;
        NothrowSwappableType2 b;

        swap(a, b);
    }
}

TEST(type_traits, is_nothrow_swappable_with)
{
    struct Foo
    {
        Foo() = default;
    };
    struct Bar
    {
        Bar() = default;
    };
    auto result = tempest::is_nothrow_swappable_with<Foo, Bar>::value;
    EXPECT_FALSE(result);

    result = tempest::is_nothrow_swappable_with<NothrowSwappableType1, NothrowSwappableType2>::value;
    EXPECT_FALSE(result);

    result = tempest::is_nothrow_swappable_with<NothrowSwappableType2, NothrowSwappableType1>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_swappable)
{
    struct Foo
    {
        Foo() = default;
    };
    auto result = tempest::is_swappable<Foo>::value;
    EXPECT_TRUE(result);

    result = tempest::is_swappable<SwappableType1>::value;
    EXPECT_TRUE(result);

    result = tempest::is_swappable<SwappableType2>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_nothrow_swappable)
{
    struct Foo
    {
        Foo() = default;
    };
    auto result = tempest::is_nothrow_swappable<Foo>::value;
    EXPECT_TRUE(result);

    result = tempest::is_nothrow_swappable<NothrowSwappableType1>::value;
    EXPECT_TRUE(result);

    result = tempest::is_nothrow_swappable<NothrowSwappableType2>::value;
    EXPECT_TRUE(result);

    // Types without nothrow assignment operator and move constructor can't be nothrow swappable
    struct Bar
    {
        Bar() = default;
        Bar& operator=(Bar&&) noexcept(false)
        {
            return *this;
        }
    };
    result = tempest::is_nothrow_swappable<Bar>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, alignment_of)
{
    auto result = tempest::alignment_of<int>::value;
    EXPECT_EQ(result, alignof(int));

    result = tempest::alignment_of<float>::value;
    EXPECT_EQ(result, alignof(float));

    result = tempest::alignment_of<double>::value;
    EXPECT_EQ(result, alignof(double));

    result = tempest::alignment_of<long double>::value;
    EXPECT_EQ(result, alignof(long double));

    struct Foo
    {
    };
    result = tempest::alignment_of<Foo>::value;
    EXPECT_EQ(result, alignof(Foo));
}

TEST(type_traits, rank)
{
    auto result = tempest::rank<int>::value;
    EXPECT_EQ(result, 0);

    result = tempest::rank<int[]>::value;
    EXPECT_EQ(result, 1);

    result = tempest::rank<int[5]>::value;
    EXPECT_EQ(result, 1);

    result = tempest::rank<int[][5]>::value;
    EXPECT_EQ(result, 2);

    result = tempest::rank<int[5][5]>::value;
    EXPECT_EQ(result, 2);

    result = tempest::rank<int[][5][5]>::value;
    EXPECT_EQ(result, 3);
}

TEST(type_traits, extent)
{
    auto result = tempest::extent<int, 0>::value;
    EXPECT_EQ(result, 0);

    result = tempest::extent<int[], 0>::value;
    EXPECT_EQ(result, 0);

    result = tempest::extent<int[5], 0>::value;
    EXPECT_EQ(result, 5);

    result = tempest::extent<int[][5], 0>::value;
    EXPECT_EQ(result, 0);

    result = tempest::extent<int[][5], 1>::value;
    EXPECT_EQ(result, 5);

    result = tempest::extent<int[5][5], 0>::value;
    EXPECT_EQ(result, 5);

    result = tempest::extent<int[5][5], 1>::value;
    EXPECT_EQ(result, 5);

    result = tempest::extent<int[5][5], 2>::value;
    EXPECT_EQ(result, 0);

    result = tempest::extent<int[][5][5], 0>::value;
    EXPECT_EQ(result, 0);

    result = tempest::extent<int[][5][5], 1>::value;
    EXPECT_EQ(result, 5);

    result = tempest::extent<int[][5][5], 2>::value;
    EXPECT_EQ(result, 5);
}

TEST(type_traits, is_base_of)
{
    struct Base
    {
    };
    struct Derived : Base
    {
    };
    auto result = tempest::is_base_of<Base, Derived>::value;
    EXPECT_TRUE(result);

    struct Unrelated
    {
    };
    result = tempest::is_base_of<Base, Unrelated>::value;
    EXPECT_FALSE(result);

    struct Final final
    {
    };
    result = tempest::is_base_of<Base, Final>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_convertible)
{
    struct Foo
    {
    };
    struct Bar
    {
        operator Foo() const
        {
            return Foo();
        }
    };
    auto result = tempest::is_convertible<Bar, Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        operator Foo() const = delete;
    };
    result = tempest::is_convertible<Baz, Foo>::value;
    EXPECT_FALSE(result);

    struct Qux
    {
        operator Foo() const
        {
            return Foo();
        };
    };
    result = tempest::is_convertible<Qux, Foo>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_convertible_fallback)
{
    struct Foo
    {
    };
    struct Bar
    {
        operator Foo() const
        {
            return Foo();
        }
    };
    auto result = tempest::detail::is_convertible_fallback<Bar, Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        operator Foo() const = delete;
    };
    result = tempest::detail::is_convertible_fallback<Baz, Foo>::value;
    EXPECT_FALSE(result);

    struct Qux
    {
        operator Foo() const
        {
            return Foo();
        };
    };
    result = tempest::detail::is_convertible_fallback<Qux, Foo>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_nothrow_convertible)
{
    struct Foo
    {
    };
    struct Bar
    {
        operator Foo() const noexcept
        {
            return Foo();
        }
    };
    auto result = tempest::is_nothrow_convertible<Bar, Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        operator Foo() const noexcept = delete;
    };
    result = tempest::is_nothrow_convertible<Baz, Foo>::value;
    EXPECT_FALSE(result);

    struct Qux
    {
        operator Foo() const noexcept
        {
            return Foo();
        };
    };
    result = tempest::is_nothrow_convertible<Qux, Foo>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, is_nothrow_convertible_fallback)
{
    struct Foo
    {
    };
    struct Bar
    {
        operator Foo() const noexcept
        {
            return Foo();
        }
    };
    auto result = tempest::detail::is_nothrow_convertible_fallback<Bar, Foo>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        operator Foo() const noexcept = delete;
    };
    result = tempest::detail::is_nothrow_convertible_fallback<Baz, Foo>::value;
    EXPECT_FALSE(result);

    struct Qux
    {
        operator Foo() const noexcept
        {
            return Foo();
        };
    };
    result = tempest::detail::is_nothrow_convertible_fallback<Qux, Foo>::value;
    EXPECT_TRUE(result);
}

#if defined(_MSC_VER) && !defined(__clang__)

TEST(type_traits, is_layout_compatible)
{
    struct Foo
    {
        int a;
        float b;
    };
    struct Bar
    {
        int a;
        float b;
    };
    auto result = tempest::is_layout_compatible<Foo, Bar>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        int a;
        float b;
        char c;
    };
    result = tempest::is_layout_compatible<Foo, Baz>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_pointer_interconvertible_base_of)
{
    struct Foo
    {
    };

    struct Bar
    {
    };

    class Baz : Foo, public Bar
    {
        int x;
    };

    class NonStdLayout : public Baz
    {
        int y;
    };

    auto result = tempest::is_pointer_interconvertible_base_of<Foo, Baz>::value;
    EXPECT_TRUE(result);

#ifndef _MSC_VER
    result = tempest::is_pointer_interconvertible_base_of<Bar, Baz>::value;
    EXPECT_TRUE(result);
#endif

    result = tempest::is_pointer_interconvertible_base_of<Bar, NonStdLayout>::value;
    EXPECT_FALSE(result);

    result = tempest::is_pointer_interconvertible_base_of<NonStdLayout, NonStdLayout>::value;
    EXPECT_TRUE(result);
}

#endif

TEST(type_traits, is_invocable)
{
    struct Foo
    {
        void operator()()
        {
        }
    };
    auto result = tempest::is_invocable<Foo>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        void operator()(int)
        {
        }
    };
    result = tempest::is_invocable<Bar>::value;
    EXPECT_FALSE(result);

    result = tempest::is_invocable<Bar, int>::value;
    EXPECT_TRUE(result);

    struct Baz
    {
        void operator()() = delete;
    };
    result = tempest::is_invocable<Baz>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_invocable_r)
{
    struct Foo
    {
        int operator()()
        {
            return 0;
        }
    };
    auto result = tempest::is_invocable_r<int, Foo>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        void operator()()
        {
        }
    };
    result = tempest::is_invocable_r<int, Bar>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        int operator()() = delete;
    };
    result = tempest::is_invocable_r<int, Baz>::value;
    EXPECT_FALSE(result);

    struct Qux
    {
        int operator()()
        {
            return 0;
        }
    };

    result = tempest::is_invocable_r<void, Qux>::value;
    EXPECT_TRUE(result); // True because it can be assigned to void

    result = tempest::is_invocable_r<Foo, Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_nothrow_invocable)
{
    struct Foo
    {
        void operator()() noexcept
        {
        }
    };
    auto result = tempest::is_nothrow_invocable<Foo>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        void operator()() noexcept(false)
        {
        }
    };
    result = tempest::is_nothrow_invocable<Bar>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        void operator()() noexcept = delete;
    };
    result = tempest::is_nothrow_invocable<Baz>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, is_nothrow_invocable_r)
{
    struct Foo
    {
        int operator()() noexcept
        {
            return 0;
        }
    };
    auto result = tempest::is_nothrow_invocable_r<int, Foo>::value;
    EXPECT_TRUE(result);

    struct Bar
    {
        void operator()() noexcept(false)
        {
        }
    };
    result = tempest::is_nothrow_invocable_r<int, Bar>::value;
    EXPECT_FALSE(result);

    struct Baz
    {
        int operator()() noexcept = delete;
    };
    result = tempest::is_nothrow_invocable_r<int, Baz>::value;
    EXPECT_FALSE(result);

    struct Qux
    {
        int operator()() noexcept
        {
            return 0;
        }
    };

    result = tempest::is_nothrow_invocable_r<void, Qux>::value;
    EXPECT_TRUE(result); // True because it can be assigned to void

    result = tempest::is_nothrow_invocable_r<Foo, Qux>::value;
    EXPECT_FALSE(result);
}

TEST(type_traits, remove_extent)
{
    auto result = std::is_same<tempest::remove_extent<int>::type, int>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_extent<int[]>::type, int>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_extent<int[5]>::type, int>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_extent<int[][5]>::type, int[5]>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_extent<int[5][5]>::type, int[5]>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_extent<int[][5][5]>::type, int[5][5]>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, remove_all_extents)
{
    auto result = std::is_same<tempest::remove_all_extents<int>::type, int>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_all_extents<int[]>::type, int>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_all_extents<int[5]>::type, int>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_all_extents<int[][5]>::type, int>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_all_extents<int[5][5]>::type, int>::value;
    EXPECT_TRUE(result);

    result = std::is_same<tempest::remove_all_extents<int[][5][5]>::type, int>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, common_type)
{
    auto result = tempest::is_same<tempest::common_type<int, int>::type, int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_type<int, float>::type, float>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_type<int, float, double>::type, double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_type<int, float, double, long double>::type, long double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_type<int, float, double, long double, char>::type, long double>::value;
    EXPECT_TRUE(result);

    result =
        tempest::is_same<tempest::common_type<int, float, double, long double, char, short>::type, long double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_type<int, float, double, long double, char, short, long>::type,
                              long double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_type<int, float, double, long double, char, short, long, long long>::type,
                              long double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<
        tempest::common_type<int, float, double, long double, char, short, long, long long, unsigned>::type,
        long double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_type<int, float, double, long double, char, short, long, long long,
                                                   unsigned, unsigned long>::type,
                              long double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_type<int, float, double, long double, char, short, long, long long,
                                                   unsigned, unsigned long, unsigned long long>::type,
                              long double>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_type<int, float, double, long double, char, short, long, long long,
                                                   unsigned, unsigned long, unsigned long long, unsigned char>::type,
                              long double>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, common_reference)
{
    class A {};
    class B : public A {};
    class C : public A {};

    auto result = tempest::is_same<tempest::common_reference<A, B>::type, A>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_reference<A, B, C>::type, A>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_reference<A&, B>::type, A>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_reference<A&, B&>::type, A&>::value;
    EXPECT_TRUE(result);

    class D : public B {};
    result = tempest::is_same<tempest::common_reference<A, B, D>::type, A>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::common_reference<B, D>::type, B>::value;
    EXPECT_TRUE(result);
}

TEST(type_traits, make_signed)
{
    auto result = tempest::is_same<tempest::make_signed<unsigned>::type, signed>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_signed<unsigned char>::type, signed char>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_signed<unsigned short>::type, signed short>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_signed<unsigned int>::type, signed int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_signed<unsigned long>::type, signed long>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_signed<unsigned long long>::type, signed long long>::value;
    EXPECT_TRUE(result);

    // Test an enumeration

    enum class Enum : unsigned int
    {
    };

    result = tempest::is_same<tempest::make_signed<Enum>::type, signed int>::value;
}

TEST(type_traits, make_unsigned)
{
    auto result = tempest::is_same<tempest::make_unsigned<signed>::type, unsigned>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_unsigned<signed char>::type, unsigned char>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_unsigned<signed short>::type, unsigned short>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_unsigned<signed int>::type, unsigned int>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_unsigned<signed long>::type, unsigned long>::value;
    EXPECT_TRUE(result);

    result = tempest::is_same<tempest::make_unsigned<signed long long>::type, unsigned long long>::value;
    EXPECT_TRUE(result);

    // Test an enumeration

    enum class Enum : signed int
    {
    };

    result = tempest::is_same<tempest::make_unsigned<Enum>::type, unsigned int>::value;
}