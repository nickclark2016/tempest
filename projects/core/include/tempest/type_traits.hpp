#ifndef tempest_core_type_traits_hpp
#define tempest_core_type_traits_hpp

#include <tempest/int.hpp>

/// @file type_traits.hpp
/// @brief Contains type traits for compile-time type information.
///
/// This file implements part of the standard library's metaprogramming library.  It provides
/// compile-time type information about types.

namespace tempest
{
    template <typename T>
    class reference_wrapper;

    /// @brief Type of nullptr.
    using nullptr_t = decltype(nullptr);

    /// @brief Type wrapping a static constant of a given type.
    /// @tparam T
    /// @tparam v
    template <typename T, T v>
    struct integral_constant
    {
        /// @brief The wrapped value.
        static constexpr T value = v;

        /// @brief The wrapped value type.
        using value_type = T;

        /// @brief The type of the integral_constant.
        using type = integral_constant;

        /// @brief Conversion operator to the wrapped value type.
        constexpr operator value_type() const noexcept;

        /// @brief Function call operator returning the wrapped value.
        constexpr value_type operator()() const noexcept;
    };

    template <typename T, T v>
    constexpr integral_constant<T, v>::operator value_type() const noexcept
    {
        return value;
    }

    template <typename T, T v>
    constexpr typename integral_constant<T, v>::value_type integral_constant<T, v>::operator()() const noexcept
    {
        return value;
    }

    /// @brief Alias to boolean integral constant.
    /// @tparam B Value of the boolean integral constant.
    template <bool B>
    using bool_constant = integral_constant<bool, B>;

    /// @brief Alias to true boolean integral constant.
    using true_type = bool_constant<true>;

    /// @brief Alias to false boolean integral constant.
    using false_type = bool_constant<false>;

    /// @brief Type trait to conditionally select a type.
    /// @tparam T Type to select if the condition is true.
    /// @tparam F Type to select if the condition is false.
    /// @tparam B Boolean condition.
    template <bool B, typename T, typename F>
    struct conditional
    {
        using type = T;
    };

    /// @brief Type trait to conditionally select a type.
    /// @tparam T Type to select if the condition is true.
    /// @tparam F Type to select if the condition is false.
    template <typename T, typename F>
    struct conditional<false, T, F>
    {
        using type = F;
    };

    /// @brief Type trait to conditionally select a type.
    /// @tparam B Boolean condition.
    /// @tparam T Type to select if the condition is true.
    /// @tparam F Type to select if the condition is false.
    template <bool B, typename T, typename F>
    using conditional_t = typename conditional<B, T, F>::type;

    /// @brief Logical AND of multiple type traits.
    template <typename...>
    struct conjunction : true_type
    {
    };

    /// @brief Logical AND of multiple type traits.
    /// @tparam B Type trait to check.
    template <typename B>
    struct conjunction<B> : B
    {
    };

    /// @brief Logical AND of multiple type traits.
    /// @tparam B Type trait to check.
    /// @tparam Bs Type traits to check.
    template <typename B, typename... Bs>
    struct conjunction<B, Bs...> : conditional_t<bool(B::value), conjunction<Bs...>, B>
    {
    };

    /// @brief Logical AND of multiple type traits.
    template <typename... Bs>
    inline constexpr bool conjunction_v = conjunction<Bs...>::value;

    /// @brief Logical OR of multiple type traits.
    template <typename...>
    struct disjunction : false_type
    {
    };

    /// @brief Logical OR of multiple type traits.
    /// @tparam B Type trait to check.
    template <typename B>
    struct disjunction<B> : B
    {
    };

    /// @brief Logical OR of multiple type traits.
    /// @tparam B Type trait to check.
    /// @tparam Bs Type traits to check.
    template <typename B, typename... Bs>
    struct disjunction<B, Bs...> : conditional_t<bool(B::value), B, disjunction<Bs...>>
    {
    };

    /// @brief Logical OR of multiple type traits.
    template <typename... Bs>
    inline constexpr bool disjunction_v = disjunction<Bs...>::value;

    /// @brief Logical NOT of a type trait.
    /// @tparam B Type trait to negate.
    template <typename B>
    struct negation : bool_constant<!B::value>
    {
    };

    /// @brief Logical NOT of a type trait.
    template <typename B>
    inline constexpr bool negation_v = negation<B>::value;

    // Type properties

    /// @brief Type trait to check if a type is const.
    /// @tparam T Type to check if is const.
    template <typename T>
    struct is_const : false_type
    {
    };

    /// @brief Type trait to check if a type is const.
    /// @tparam T Type to check if is const.
    template <typename T>
    struct is_const<const T> : true_type
    {
    };

    /// @brief Type trait to check if a type is const.
    /// @tparam T Type to check if is const.
    template <typename T>
    inline constexpr bool is_const_v = is_const<T>::value;

    /// @brief Type trait to check if a type is volatile.
    /// @tparam T Type to check if is volatile.
    template <typename T>
    struct is_volatile : false_type
    {
    };

    /// @brief Type trait to check if a type is volatile.
    /// @tparam T Type to check if is volatile.
    template <typename T>
    struct is_volatile<volatile T> : true_type
    {
    };

    /// @brief Type trait to check if a type is volatile.
    /// @tparam T Type to check if is volatile.
    template <typename T>
    inline constexpr bool is_volatile_v = is_volatile<T>::value;

    /// @brief Type trait to check if a type is trivial
    /// @tparam T Type to check if is trivial.
    template <typename T>
    struct is_trivial : bool_constant<__is_trivial(T)>
    {
    };

    /// @brief Type trait to check if a type is trivial
    /// @tparam T Type to check if is trivial.
    template <typename T>
    inline constexpr bool is_trivial_v = is_trivial<T>::value;

    /// @brief Type trait to check if a type is trivially copyable.
    /// @tparam T Type to check if is trivially copyable.
    template <typename T>
    struct is_trivially_copyable : bool_constant<__is_trivially_copyable(T)>
    {
    };

    /// @brief Type trait to check if a type is trivially copyable.
    /// @tparam T Type to check if is trivially copyable.
    template <typename T>
    inline constexpr bool is_trivially_copyable_v = is_trivially_copyable<T>::value;

    /// @brief Type trait to check if a type is standard layout.
    /// @tparam T Type to check if is standard layout.
    template <typename T>
    struct is_standard_layout : bool_constant<__is_standard_layout(T)>
    {
    };

    /// @brief Type trait to check if a type is standard layout.
    /// @tparam T Type to check if is standard layout.
    template <typename T>
    inline constexpr bool is_standard_layout_v = is_standard_layout<T>::value;

    /// @brief Type trait to check if a type has unique object representations.
    /// @tparam T Type to check if has unique object representations.
    template <typename T>
    struct has_unique_object_representations : bool_constant<__has_unique_object_representations(T)>
    {
    };

    /// @brief Type trait to check if a type has unique object representations.
    /// @tparam T Type to check if has unique object representations.
    template <typename T>
    inline constexpr bool has_unique_object_representations_v = has_unique_object_representations<T>::value;

    // Const-volatile modifications

    /// @brief Remove the const qualifier from a type.
    /// @tparam T Type to remove the const qualifier from.
    template <typename T>
    struct remove_const
    {
        /// @brief The type without the const qualifier.
        using type = T;
    };

    /// @brief Remove the const qualifier from a type.
    /// @tparam T Type to remove the const qualifier from.
    template <typename T>
    struct remove_const<const T>
    {
        /// @brief The type without the const qualifier.
        using type = T;
    };

    /// @brief Remove the const qualifier from a type.
    /// @tparam T Type to remove the const qualifier from.
    template <typename T>
    using remove_const_t = typename remove_const<T>::type;

    /// @brief Remove the volatile qualifier from a type.
    /// @tparam T Type to remove the volatile qualifier from.
    template <typename T>
    struct remove_volatile
    {
        /// @brief The type without the volatile qualifier.
        using type = T;
    };

    /// @brief Remove the volatile qualifier from a type.
    /// @tparam T Type to remove the volatile qualifier from.
    template <typename T>
    struct remove_volatile<volatile T>
    {
        /// @brief The type without the volatile qualifier.
        using type = T;
    };

    /// @brief Remove the volatile qualifier from a type.
    /// @tparam T Type to remove the volatile qualifier from.
    template <typename T>
    using remove_volatile_t = typename remove_volatile<T>::type;

    /// @brief Remove the const and volatile qualifiers from a type.
    /// @tparam T Type to remove the const and volatile qualifiers from.
    template <typename T>
    struct remove_cv
    {
        /// @brief The type without the const and volatile qualifiers.
        using type = typename remove_const<typename remove_volatile<T>::type>::type;
    };

    /// @brief Remove the const and volatile qualifiers from a type.
    /// @tparam T Type to remove the const and volatile qualifiers from.
    template <typename T>
    using remove_cv_t = typename remove_cv<T>::type;

    // Const-volatile modifications

    /// @brief Adds the const qualifier to a type.
    /// @tparam T Type to add the const qualifier to.
    template <typename T>
    struct add_const
    {
        /// @brief The type with the const qualifier.
        using type = const T;
    };

    /// @brief Adds the const qualifier to a type.
    /// @tparam T Type to add the const qualifier to.
    template <typename T>
    struct add_const<const T>
    {
        /// @brief The type with the const qualifier.
        using type = const T;
    };

    /// @brief Adds the volatile qualifier to a type.
    /// @tparam T Type to add the volatile qualifier to.
    template <typename T>
    struct add_volatile
    {
        /// @brief The type with the volatile qualifier.
        using type = volatile T;
    };

    /// @brief Adds the volatile qualifier to a type.
    /// @tparam T Type to add the volatile qualifier to.
    template <typename T>
    struct add_volatile<volatile T>
    {
        /// @brief The type with the volatile qualifier.
        using type = volatile T;
    };

    /// @brief Adds the const and volatile qualifiers to a type.
    /// @tparam T Type to add the const and volatile qualifiers to.
    template <typename T>
    struct add_cv
    {
        /// @brief The type with the const and volatile qualifiers.
        using type = const volatile T;
    };

    /// @brief Adds the const and volatile qualifiers to a type.
    /// @tparam T Type to add the const and volatile qualifiers to.
    template <typename T>
    struct add_cv<const T>
    {
        /// @brief The type with the const and volatile qualifiers.
        using type = const volatile T;
    };

    /// @brief Adds the const and volatile qualifiers to a type.
    /// @tparam T Type to add the const and volatile qualifiers to.
    template <typename T>
    struct add_cv<volatile T>
    {
        /// @brief The type with the const and volatile qualifiers.
        using type = const volatile T;
    };

    /// @brief Adds the const and volatile qualifiers to a type.
    /// @tparam T Type to add the const and volatile qualifiers to.
    template <typename T>
    struct add_cv<const volatile T>
    {
        /// @brief The type with the const and volatile qualifiers.
        using type = const volatile T;
    };

    /// @brief Adds the const qualifier to a type.
    /// @tparam T Type to add the const qualifier to.
    template <typename T>
    using add_const_t = typename add_const<T>::type;

    /// @brief Adds the volatile qualifier to a type.
    /// @tparam T Type to add the volatile qualifier to.
    template <typename T>
    using add_volatile_t = typename add_volatile<T>::type;

    /// @brief Adds the const and volatile qualifiers to a type.
    /// @tparam T Type to add the const and volatile qualifiers to.
    template <typename T>
    using add_cv_t = typename add_cv<T>::type;

    namespace detail
    {
        template <typename T>
        struct is_integral_helper : false_type
        {
        };

        template <>
        struct is_integral_helper<bool> : true_type
        {
        };

        template <>
        struct is_integral_helper<char> : true_type
        {
        };

        template <>
        struct is_integral_helper<signed char> : true_type
        {
        };

        template <>
        struct is_integral_helper<unsigned char> : true_type
        {
        };

        template <>
        struct is_integral_helper<wchar_t> : true_type
        {
        };

        template <>
        struct is_integral_helper<char16_t> : true_type
        {
        };

        template <>
        struct is_integral_helper<char32_t> : true_type
        {
        };

        template <>
        struct is_integral_helper<short> : true_type
        {
        };

        template <>
        struct is_integral_helper<unsigned short> : true_type
        {
        };

        template <>
        struct is_integral_helper<int> : true_type
        {
        };

        template <>
        struct is_integral_helper<unsigned int> : true_type
        {
        };

        template <>
        struct is_integral_helper<long> : true_type
        {
        };

        template <>
        struct is_integral_helper<unsigned long> : true_type
        {
        };

        template <>
        struct is_integral_helper<long long> : true_type
        {
        };

        template <>
        struct is_integral_helper<unsigned long long> : true_type
        {
        };

        template <typename T>
        struct is_floating_point_helper : false_type
        {
        };

        template <>
        struct is_floating_point_helper<float> : true_type
        {
        };

        template <>
        struct is_floating_point_helper<double> : true_type
        {
        };

        template <>
        struct is_floating_point_helper<long double> : true_type
        {
        };
    } // namespace detail

    /// @brief Type trait to check if two types are the same.
    /// @tparam T Left hand type.
    /// @tparam U Right hand type.
    template <typename T, typename U>
    struct is_same : false_type
    {
    };

    /// @brief Type trait to check if two types are the same.
    /// @tparam T Type to check, specialization for the same type.
    template <typename T>
    struct is_same<T, T> : true_type
    {
    };

    /// @brief Type trait to check if two types are the same.
    /// @tparam T Left hand type.
    /// @tparam U Right hand type.
    template <typename T, typename U>
    inline constexpr bool is_same_v = is_same<T, U>::value;

    /// @brief Type trait to check if type is base of another type.
    /// @tparam Base Base type.
    /// @tparam Derived Derived type.
    template <typename Base, typename Derived>
    struct is_base_of : bool_constant<__is_base_of(Base, Derived)>
    {
    };

    /// @brief Type trait to check if type is base of another type.
    /// @tparam Base Base type.
    /// @tparam Derived Derived type.
    template <typename Base, typename Derived>
    inline constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

    /// @brief Type trait to check if a type is void.
    /// @tparam T Type to check if is void.
    template <typename T>
    struct is_void : is_same<void, remove_cv_t<T>>
    {
    };

    /// @brief Type trait to check if a type is void.
    /// @tparam T Type to check if is void.
    template <typename T>
    inline constexpr bool is_void_v = is_void<T>::value;

    /// @brief Type trait to check if a type is nullptr_t.
    /// @tparam T Type to check if is nullptr_t.
    template <typename T>
    struct is_null_pointer : is_same<nullptr_t, remove_cv_t<T>>
    {
    };

    /// @brief Type trait to check if a type is nullptr_t.
    /// @tparam T Type to check if is nullptr_t.
    template <typename T>
    inline constexpr bool is_null_pointer_v = is_null_pointer<T>::value;

    /// @brief Type trait to check if a type is an integral type.
    /// @tparam T Type to check if is an integral type.
    template <typename T>
    struct is_integral : detail::is_integral_helper<remove_cv_t<T>>
    {
    };

    /// @brief Type trait to check if a type is an integral type.
    /// @tparam T Type to check if is an integral type.
    template <typename T>
    inline constexpr bool is_integral_v = is_integral<T>::value;

    /// @brief Type trait to check if a type is a floating point type.
    /// @tparam T Type to check if is a floating point type.
    template <typename T>
    struct is_floating_point : detail::is_floating_point_helper<remove_cv_t<T>>
    {
    };

    /// @brief Type trait to check if a type is a floating point type.
    /// @tparam T Type to check if is a floating point type.
    template <typename T>
    inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

    /// @brief Type trait to check if a type is an arithmetic type.
    /// @tparam T Type to check if is an arithmetic type.
    template <typename T>
    struct is_arithmetic : bool_constant<is_integral_v<T> || is_floating_point_v<T>>
    {
    };

    /// @brief Type trait to check if a type is an arithmetic type.
    /// @tparam T Type to check if is an arithmetic type.
    template <typename T>
    inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

    /// @brief Type trait to check if a type is a fundamental type.
    /// @tparam T Type to check if is a fundamental type.
    template <typename T>
    struct is_fundamental : bool_constant<is_arithmetic_v<T> || is_void_v<T> || is_null_pointer_v<T>>
    {
    };

    /// @brief Type trait to check if a type is a fundamental type.
    /// @tparam T Type to check if is a fundamental type.
    template <typename T>
    inline constexpr bool is_fundamental_v = is_fundamental<T>::value;

    /// @brief Type trait to check if a type is a fundamental type.
    /// @tparam T Type to check if is a fundamental type.
    template <typename T>
    struct is_pointer : false_type
    {
    };

    /// @brief Type trait to check if a type is a pointer.
    /// @tparam T Type to check if is a pointer.
    template <typename T>
    struct is_pointer<T*> : true_type
    {
    };

    /// @brief Type trait to check if a type is a pointer.
    /// @tparam T Type to check if is a pointer.
    template <typename T>
    inline constexpr bool is_pointer_v = is_pointer<T>::value;

    /// @brief Type trait to check if a type is an lvalue reference.
    /// @tparam T Type to check if is an lvalue reference.
    template <typename T>
    struct is_lvalue_reference : false_type
    {
    };

    /// @brief Type trait to check if a type is an lvalue reference.
    /// @tparam T Type to check if is an lvalue reference.
    template <typename T>
    struct is_lvalue_reference<T&> : true_type
    {
    };

    /// @brief Type trait to check if a type is an lvalue reference.
    /// @tparam T Type to check if is an lvalue reference.
    template <typename T>
    inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

    /// @brief Type trait to check if a type is an rvalue reference.
    /// @tparam T Type to check if is an rvalue reference.
    template <typename T>
    struct is_rvalue_reference : false_type
    {
    };

    /// @brief Type trait to check if a type is an rvalue reference.
    /// @tparam T Type to check if is an rvalue reference.
    template <typename T>
    struct is_rvalue_reference<T&&> : true_type
    {
    };

    /// @brief Type trait to check if a type is an rvalue reference.
    /// @tparam T Type to check if is an rvalue reference.
    template <typename T>
    inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

    namespace detail
    {
        /// @brief Helper type trait to check if a type is a member pointer.
        /// @tparam T Type to check if is a member pointer.
        template <typename T>
        struct is_member_pointer_helper : false_type
        {
        };

        /// @brief Helper type trait to check if a type is a member pointer.
        /// @tparam T Type to check if is a member pointer.
        template <typename T, typename U>
        struct is_member_pointer_helper<T U::*> : true_type
        {
        };
    } // namespace detail

    /// @brief Type trait to check if a type is a member pointer.
    /// @tparam T Type to check if is a member pointer.
    template <typename T>
    struct is_member_pointer : detail::is_member_pointer_helper<remove_cv_t<T>>
    {
    };

    /// @brief Type trait to check if a type is a member pointer.
    /// @tparam T Type to check if is a member pointer.
    template <typename T>
    inline constexpr bool is_member_pointer_v = is_member_pointer<T>::value;

    /// @brief Type trait to check if a type is an array.
    /// @tparam T Type to check if is an array.
    template <typename T>
    struct is_array : false_type
    {
    };

    /// @brief Type trait to check if a type is an array.
    /// @tparam T Type to check if is an array.
    template <typename T>
    struct is_array<T[]> : true_type
    {
    };

    /// @brief Type trait to check if a type is an array.
    /// @tparam T Type to check if is an array.
    /// @tparam N Size of the array.
    template <typename T, size_t N>
    struct is_array<T[N]> : true_type
    {
    };

    /// @brief Type trait to check if a type is an array.
    /// @tparam T Type to check if is an array.
    template <typename T>
    inline constexpr bool is_array_v = is_array<T>::value;

    /// @brief Type trait to check if a type is a reference.
    /// @tparam T Type to check if is a reference.
    template <typename T>
    struct is_reference : bool_constant<is_lvalue_reference_v<T> || is_rvalue_reference_v<T>>
    {
    };

    /// @brief Type trait to check if a type is a reference.
    /// @tparam T Type to check if is a reference.
    template <typename T>
    inline constexpr bool is_reference_v = is_reference<T>::value;

    /// @brief Type trait to check if a type is an enum.
    /// @tparam T Type to check if is an enum.
    template <typename T>
    struct is_enum : bool_constant<__is_enum(T)>
    {
    };

    /// @brief Type trait to check if a type is an enum.
    /// @tparam T Type to check if is an enum.
    template <typename T>
    inline constexpr bool is_enum_v = is_enum<T>::value;

    /// @brief Type trait to check if a type is a union.
    /// @tparam T Type to check if is a union.
    template <typename T>
    struct is_union : bool_constant<__is_union(T)>
    {
    };

    /// @brief Type trait to check if a type is a union.
    /// @tparam T Type to check if is a union.
    template <typename T>
    inline constexpr bool is_union_v = is_union<T>::value;

    /// @brief Type trait to check if a type is a scalar.
    /// @tparam T Type to check if is a scalar.
    template <typename T>
    struct is_scalar : bool_constant<is_arithmetic_v<T> || is_enum_v<T> || is_pointer_v<T> || is_member_pointer_v<T>>
    {
    };

    /// @brief Type trait to check if a type is a scalar.
    /// @tparam T Type to check if is a scalar.
    template <typename T>
    inline constexpr bool is_scalar_v = is_scalar<T>::value;

#ifdef __clang__
    /// @brief Type trait to check if a type is a function.
    /// @tparam T Type to check if is a function.
    template <typename T>
    struct is_function : bool_constant<__is_function(T)>
    {
    };
#else

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4180)
#endif

    /// @brief Type trait to check if a type is a function.
    /// @tparam T Type to check if is a function.
    template <typename T>
    struct is_function : bool_constant<!is_const_v<const T> && !is_reference_v<T>>
    {
    };

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif

    /// @brief Type trait to check if a type is a function.
    /// @tparam T Type to check if is a function.
    template <typename T>
    inline constexpr bool is_function_v = is_function<T>::value;

    /// @brief Type trait to check if a type is a class.
    /// @tparam T Type to check if is a class.
    template <typename T>
    struct is_class : bool_constant<__is_class(T)>
    {
    };

    /// @brief Type trait to check if a type is a class.
    /// @tparam T Type to check if is a class.
    template <typename T>
    inline constexpr bool is_class_v = is_class<T>::value;

    /// @brief Type trait to check if a type is an object.
    /// @tparam T Type to check if is an object.
    template <typename T>
    struct is_object : bool_constant<is_scalar_v<T> || is_array_v<T> || is_union_v<T> || is_class_v<T>>
    {
    };

    /// @brief Type trait to check if a type is an object.
    /// @tparam T Type to check if is an object.
    template <typename T>
    inline constexpr bool is_object_v = is_object<T>::value;

    /// @brief Type trait to check if a type is a compound type.
    /// @tparam T Type to check if is a compound type.
    template <typename T>
    struct is_compound : bool_constant<!is_fundamental_v<T>>
    {
    };

    /// @brief Type trait to check if a type is a compound type.
    /// @tparam T Type to check if is a compound type.
    template <typename T>
    inline constexpr bool is_compound_v = is_compound<T>::value;

    namespace detail
    {
        /// @brief Helper type trait to check if a type is a member function pointer.
        /// @tparam T Type to check if is a member function pointer.
        template <typename T>
        struct is_member_function_pointer_helper : false_type
        {
        };

        /// @brief Helper type trait to check if a type is a member function pointer.
        /// @tparam T Type to check if is a member function pointer.
        template <typename T, typename U>
        struct is_member_function_pointer_helper<T U::*> : is_function<T>
        {
        };
    } // namespace detail

    /// @brief Type trait to check if a type is a member function pointer.
    /// @tparam T Type to check if is a member function pointer.
    template <typename T>
    struct is_member_function_pointer : detail::is_member_function_pointer_helper<remove_cv_t<T>>
    {
    };

    /// @brief Type trait to check if a type is a member function pointer.
    /// @tparam T Type to check if is a member function pointer.
    template <typename T>
    inline constexpr bool is_member_function_pointer_v = is_member_function_pointer<T>::value;

    /// @brief Type trait to check if a type is a member object pointer.
    /// @tparam T Type to check if is a member object pointer.
    template <typename T>
    struct is_member_object_pointer : bool_constant<is_member_pointer_v<T> && !is_member_function_pointer_v<T>>
    {
    };

    /// @brief Type trait to check if a type is a member object pointer.
    /// @tparam T Type to check if is a member object pointer.
    template <typename T>
    inline constexpr bool is_member_object_pointer_v = is_member_object_pointer<T>::value;

    /// @brief Type trait to check if a type is empty.
    /// @tparam T Type to check if is empty.
    template <typename T>
    struct is_empty : bool_constant<__is_empty(T)>
    {
    };

    /// @brief Type trait to check if a type is empty.
    /// @tparam T Type to check if is empty.
    template <typename T>
    inline constexpr bool is_empty_v = is_empty<T>::value;

    /// @brief Type trait to check if a type is polymorphic.
    /// @tparam T Type to check if is polymorphic.
    template <typename T>
    struct is_polymorphic : bool_constant<__is_polymorphic(T)>
    {
    };

    /// @brief Type trait to check if a type is polymorphic.
    /// @tparam T Type to check if is polymorphic.
    template <typename T>
    inline constexpr bool is_polymorphic_v = is_polymorphic<T>::value;

    /// @brief Type trait to check if a type is abstract.
    /// @tparam T Type to check if is abstract.
    template <typename T>
    struct is_abstract : bool_constant<__is_abstract(T)>
    {
    };

    /// @brief Type trait to check if a type is abstract.
    /// @tparam T Type to check if is abstract.
    template <typename T>
    inline constexpr bool is_abstract_v = is_abstract<T>::value;

    /// @brief Type trait to check if a type is final.
    /// @tparam T Type to check if is final.
    template <typename T>
    struct is_final : bool_constant<__is_final(T)>
    {
    };

    /// @brief Type trait to check if a type is final.
    /// @tparam T Type to check if is final.
    template <typename T>
    inline constexpr bool is_final_v = is_final<T>::value;

    /// @brief Type trait to check if a type is an aggregate.
    /// @tparam T Type to check if is an aggregate.
    template <typename T>
    struct is_aggregate : bool_constant<__is_aggregate(T)>
    {
    };

    /// @brief Type trait to check if a type is an aggregate.
    /// @tparam T Type to check if is an aggregate.
    template <typename T>
    inline constexpr bool is_aggregate_v = is_aggregate<T>::value;

    namespace detail
    {
        /// @brief Helper type trait to check if a type is a signed type.
        /// @tparam T Type to check if is a signed type.
        template <typename T, bool = is_integral_v<T>>
        struct signed_helper
        {
            using base_type = remove_cv_t<T>;

            static constexpr bool is_signed = base_type(-1) < base_type(0);
            static constexpr bool is_unsigned = !is_signed;
        };

        /// @brief Helper type trait to check if a type is a signed type.
        /// @tparam T Type to check if is a signed type.
        template <typename T>
        struct signed_helper<T, false>
        {
            static constexpr bool is_signed = is_floating_point_v<T>;
            static constexpr bool is_unsigned = false;
        };
    } // namespace detail

    /// @brief Type trait to check if a type is signed.
    /// @tparam T Type to check if is signed.
    template <typename T>
    struct is_signed : bool_constant<detail::signed_helper<T>::is_signed>
    {
    };

    /// @brief Type trait to check if a type is signed.
    /// @tparam T Type to check if is signed.
    template <typename T>
    inline constexpr bool is_signed_v = is_signed<T>::value;

    /// @brief Type trait to check if a type is unsigned.
    /// @tparam T Type to check if is unsigned.
    template <typename T>
    struct is_unsigned : bool_constant<detail::signed_helper<T>::is_unsigned>
    {
    };

    /// @brief Type trait to check if a type is unsigned.
    /// @tparam T Type to check if is unsigned.
    template <typename T>
    inline constexpr bool is_unsigned_v = is_unsigned<T>::value;

    /// @brief Type trait to check if a type is a bounded array.
    /// @tparam T Type to check if is a bounded array.
    template <typename T>
    struct is_bounded_array : false_type
    {
    };

    /// @brief Type trait to check if a type is a bounded array.
    /// @tparam T Type to check if is a bounded array.
    /// @tparam N Size of the array.
    template <typename T, size_t N>
    struct is_bounded_array<T[N]> : true_type
    {
    };

    /// @brief Type trait to check if a type is a bounded array.
    /// @tparam T Type to check if is a bounded array.
    template <typename T>
    inline constexpr bool is_bounded_array_v = is_bounded_array<T>::value;

    /// @brief Type trait to check if a type is an unbounded array.
    /// @tparam T Type to check if is an unbounded array.
    template <typename T>
    struct is_unbounded_array : false_type
    {
    };

    /// @brief Type trait to check if a type is an unbounded array.
    /// @tparam T Type to check if is an unbounded array.
    template <typename T>
    struct is_unbounded_array<T[]> : true_type
    {
    };

    /// @brief Type trait to check if a type is an unbounded array.
    /// @tparam T Type to check if is an unbounded array.
    template <typename T>
    inline constexpr bool is_unbounded_array_v = is_unbounded_array<T>::value;

    /// @brief Type trait to remove the reference from a type.
    /// @tparam T Type to remove the reference from.
    template <typename T>
    struct remove_reference
    {
        /// @brief The type without the reference.
        using type = T;
    };

    /// @brief Type trait to remove the reference from a type.
    /// @tparam T Type to remove the reference from.
    template <typename T>
    struct remove_reference<T&>
    {
        /// @brief The type without the reference.
        using type = T;
    };

    /// @brief Type trait to remove the reference from a type.
    /// @tparam T Type to remove the reference from.
    template <typename T>
    struct remove_reference<T&&>
    {
        /// @brief The type without the reference.
        using type = T;
    };

    /// @brief Type trait to remove the reference from a type.
    /// @tparam T Type to remove the reference from.
    template <typename T>
    using remove_reference_t = typename remove_reference<T>::type;

    /// @brief Type trait to add an lvalue reference to a type.
    /// @tparam T Type to add an lvalue reference to.
    template <typename T>
    struct add_lvalue_reference
    {
        /// @brief The type with an lvalue reference.
        using type = T&;
    };

    /// @brief Type trait to add an lvalue reference to a type.
    /// @tparam T Type to add an lvalue reference to.
    template <typename T>
    struct add_lvalue_reference<T&>
    {
        /// @brief The type with an lvalue reference.
        using type = T&;
    };

    /// @brief Type trait to add an lvalue reference to a type.
    /// @tparam T Type to add an lvalue reference to.
    template <typename T>
    using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

    /// @brief Type trait to add an rvalue reference to a type.
    /// @tparam T Type to add an rvalue reference to.
    template <typename T>
    struct add_rvalue_reference
    {
        /// @brief The type with an rvalue reference.
        using type = T&&;
    };

    /// @brief Type trait to add an rvalue reference to a type.
    /// @tparam T Type to add an rvalue reference to.
    template <typename T>
    struct add_rvalue_reference<T&>
    {
        /// @brief The type with an rvalue reference.
        using type = T&;
    };

    /// @brief Type trait to add an rvalue reference to a type.
    /// @tparam T Type to add an rvalue reference to.
    template <typename T>
    using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

    /// @brief Type trait to remove const-volatile modifiers and references from a type.
    /// @tparam T Type to remove const-volatile modifiers and references from.
    template <typename T>
    struct remove_cvref
    {
        /// @brief The type without const-volatile modifiers and references.
        using type = remove_cv_t<remove_reference_t<T>>;
    };

    /// @brief Type trait to remove const-volatile modifiers and references from a type.
    /// @tparam T Type to remove const-volatile modifiers and references from.
    template <typename T>
    using remove_cvref_t = typename remove_cvref<T>::type;

    /// @brief Type trait that is an alias to the type it is instantiated with.
    /// @tparam T Type to create an alias for.
    template <typename T>
    struct type_identity
    {
        using type = T;
    };

    /// @brief Type trait that is an alias to the type it is instantiated with.
    /// @tparam T Type to create an alias for.
    template <typename T>
    using type_identity_t = typename type_identity<T>::type;

    template <typename... T>
    struct make_void
    {
        using type = void;
    };

    template <typename... Ts>
    using void_t = typename make_void<Ts...>::type;

    namespace detail
    {
        template <typename T, typename = void>
        struct add_pointer_impl
        {
            using type = T;
        };

        template <typename T>
        struct add_pointer_impl<T, void_t<remove_reference_t<T>*>>
        {
            using type = remove_reference_t<T>*;
        };
    } // namespace detail

    /// @brief Type trait to add a pointer to a type.
    /// @tparam T Type to add a pointer to.
    template <typename T>
    struct add_pointer
    {
        /// @brief The type with a pointer.
        using type = typename detail::add_pointer_impl<T>::type;
    };

    /// @brief Type trait to add a pointer to a type.
    /// @tparam T Type to add a pointer to.
    template <typename T>
    using add_pointer_t = typename add_pointer<T>::type;

    /// @brief Type trait to remove the pointer from a type.
    /// @tparam T Type to remove the pointer from.
    template <typename T>
    struct remove_pointer
    {
        /// @brief The type without the pointer.
        using type = T;
    };

    /// @brief Type trait to remove the pointer from a type.
    /// @tparam T Type to remove the pointer from.
    template <typename T>
    struct remove_pointer<T*>
    {
        /// @brief The type without the pointer.
        using type = T;
    };

    /// @brief Type trait to remove the pointer from a type.
    /// @tparam T Type to remove the pointer from.
    template <typename T>
    struct remove_pointer<const T*>
    {
        /// @brief The type without the pointer.
        using type = T;
    };

    /// @brief Type trait to remove the pointer from a type.
    /// @tparam T Type to remove the pointer from.
    template <typename T>
    struct remove_pointer<volatile T*>
    {
        /// @brief The type without the pointer.
        using type = T;
    };

    /// @brief Type trait to remove the pointer from a type.
    /// @tparam T Type to remove the pointer from.
    template <typename T>
    struct remove_pointer<const volatile T*>
    {
        /// @brief The type without the pointer.
        using type = T;
    };

    /// @brief Type trait to remove the pointer from a type.
    /// @tparam T Type to remove the pointer from.
    template <typename T>
    using remove_pointer_t = typename remove_pointer<T>::type;

    /// @brief Type trait to remove extent from a type.
    /// @tparam T Type to remove extent from.
    template <typename T>
    struct remove_extent
    {
        /// @brief The type without the extent.
        using type = T;
    };

    /// @brief Type trait to remove extent from a type.
    /// @tparam T Type to remove extent from.
    template <typename T>
    struct remove_extent<T[]>
    {
        /// @brief The type without the extent.
        using type = T;
    };

    /// @brief Type trait to remove extent from a type.
    /// @tparam T Type to remove extent from.
    /// @tparam N Size of the extent.
    template <typename T, size_t N>
    struct remove_extent<T[N]>
    {
        /// @brief The type without the extent.
        using type = T;
    };

    /// @brief Type trait to remove extent from a type.
    /// @tparam T Type to remove extent from.
    template <typename T>
    using remove_extent_t = typename remove_extent<T>::type;

    /// @brief Type trait to remove all extents from a type.
    /// @tparam T Type to remove all extents from.
    template <typename T>
    struct remove_all_extents
    {
        /// @brief The type without all extents.
        using type = T;
    };

    /// @brief Type trait to remove all extents from a type.
    /// @tparam T Type to remove all extents from.
    template <typename T>
    struct remove_all_extents<T[]>
    {
        /// @brief The type without all extents.
        using type = typename remove_all_extents<T>::type;
    };

    /// @brief Type trait to remove all extents from a type.
    /// @tparam T Type to remove all extents from.
    /// @tparam N Size of the extent.
    template <typename T, size_t N>
    struct remove_all_extents<T[N]>
    {
        /// @brief The type without all extents.
        using type = typename remove_all_extents<T>::type;
    };

    /// @brief Type trait to remove all extents from a type.
    /// @tparam T Type to remove all extents from.
    template <typename T>
    using remove_all_extents_t = typename remove_all_extents<T>::type;

    /// @brief Type trait to check if a type is constructible from a set of arguments.
    /// @tparam T Type to check if is constructible.
    /// @tparam Args Types of the arguments.
    template <typename T, typename... Args>
    struct is_constructible : bool_constant<__is_constructible(T, Args...)>
    {
    };

    /// @brief Type trait to check if a type is constructible from a set of arguments.
    /// @tparam T Type to check if is constructible.
    /// @tparam Args Types of the arguments.
    template <typename T, typename... Args>
    inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

    /// @brief Type trait to check if a type is trivially constructible from a set of arguments.
    /// @tparam T Type to check if is trivially constructible.
    /// @tparam Args Types of the arguments.
    template <typename T, typename... Args>
    struct is_trivially_constructible : bool_constant<__is_trivially_constructible(T, Args...)>
    {
    };

    /// @brief Type trait to check if a type is trivially constructible from a set of arguments.
    /// @tparam T Type to check if is trivially constructible.
    /// @tparam Args Types of the arguments.
    template <typename T, typename... Args>
    inline constexpr bool is_trivially_constructible_v = is_trivially_constructible<T, Args...>::value;

    /// @brief Type trait to check if a type is nothrow constructible from a set of arguments.
    /// @tparam T Type to check if is nothrow constructible.
    /// @tparam Args Types of the arguments.
    template <typename T, typename... Args>
    struct is_nothrow_constructible : bool_constant<__is_nothrow_constructible(T, Args...)>
    {
    };

    /// @brief Type trait to check if a type is nothrow constructible from a set of arguments.
    /// @tparam T Type to check if is nothrow constructible.
    /// @tparam Args Types of the arguments.
    template <typename T, typename... Args>
    inline constexpr bool is_nothrow_constructible_v = is_nothrow_constructible<T, Args...>::value;

    /// @brief Type trait to check if a type is default constructible.
    /// @tparam T Type to check if is default constructible.
    template <typename T>
    struct is_default_constructible : bool_constant<is_constructible_v<T>>
    {
    };

    /// @brief Type trait to check if a type is default constructible.
    /// @tparam T Type to check if is default constructible.
    template <typename T>
    inline constexpr bool is_default_constructible_v = is_default_constructible<T>::value;

    /// @brief Type trait to check if a type is trivially default constructible.
    /// @tparam T Type to check if is trivially default constructible.
    template <typename T>
    struct is_trivially_default_constructible : bool_constant<is_trivially_constructible_v<T>>
    {
    };

    /// @brief Type trait to check if a type is trivially default constructible.
    /// @tparam T Type to check if is trivially default constructible.
    template <typename T>
    inline constexpr bool is_trivially_default_constructible_v = is_trivially_default_constructible<T>::value;

    /// @brief Type trait to check if a type is nothrow default constructible.
    /// @tparam T Type to check if is nothrow default constructible.
    template <typename T>
    struct is_nothrow_default_constructible : bool_constant<is_nothrow_constructible_v<T>>
    {
    };

    /// @brief Type trait to check if a type is nothrow default constructible.
    /// @tparam T Type to check if is nothrow default constructible.
    template <typename T>
    inline constexpr bool is_nothrow_default_constructible_v = is_nothrow_default_constructible<T>::value;

    /// @brief Type trait to check if a type is copy constructible.
    /// @tparam T Type to check if is copy constructible.
    template <typename T>
    struct is_copy_constructible : bool_constant<is_constructible_v<T, add_lvalue_reference_t<const T>>>
    {
    };

    /// @brief Type trait to check if a type is copy constructible.
    /// @tparam T Type to check if is copy constructible.
    template <typename T>
    inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

    /// @brief Type trait to check if a type is trivially copy constructible.
    /// @tparam T Type to check if is trivially copy constructible.
    template <typename T>
    struct is_trivially_copy_constructible
        : bool_constant<is_trivially_constructible_v<T, add_lvalue_reference_t<const T>>>
    {
    };

    /// @brief Type trait to check if a type is trivially copy constructible.
    /// @tparam T Type to check if is trivially copy constructible.
    template <typename T>
    inline constexpr bool is_trivially_copy_constructible_v = is_trivially_copy_constructible<T>::value;

    /// @brief Type trait to check if a type is nothrow copy constructible.
    /// @tparam T Type to check if is nothrow copy constructible.
    template <typename T>
    struct is_nothrow_copy_constructible : bool_constant<is_nothrow_constructible_v<T, add_lvalue_reference_t<const T>>>
    {
    };

    /// @brief Type trait to check if a type is nothrow copy constructible.
    /// @tparam T Type to check if is nothrow copy constructible.
    template <typename T>
    inline constexpr bool is_nothrow_copy_constructible_v = is_nothrow_copy_constructible<T>::value;

    /// @brief Type trait to check if a type is move constructible.
    /// @tparam T Type to check if is move constructible.
    template <typename T>
    struct is_move_constructible : bool_constant<is_constructible_v<T, add_rvalue_reference_t<T>>>
    {
    };

    /// @brief Type trait to check if a type is move constructible.
    /// @tparam T Type to check if is move constructible.
    template <typename T>
    inline constexpr bool is_move_constructible_v = is_move_constructible<T>::value;

    /// @brief Type trait to check if a type is trivially move constructible.
    /// @tparam T Type to check if is trivially move constructible.
    template <typename T>
    struct is_trivially_move_constructible : bool_constant<is_trivially_constructible_v<T, add_rvalue_reference_t<T>>>
    {
    };

    /// @brief Type trait to check if a type is trivially move constructible.
    /// @tparam T Type to check if is trivially move constructible.
    template <typename T>
    inline constexpr bool is_trivially_move_constructible_v = is_trivially_move_constructible<T>::value;

    /// @brief Type trait to check if a type is nothrow move constructible.
    /// @tparam T Type to check if is nothrow move constructible.
    template <typename T>
    struct is_nothrow_move_constructible : bool_constant<is_nothrow_constructible_v<T, add_rvalue_reference_t<T>>>
    {
    };

    /// @brief Type trait to check if a type is nothrow move constructible.
    /// @tparam T Type to check if is nothrow move constructible.
    template <typename T>
    inline constexpr bool is_nothrow_move_constructible_v = is_nothrow_move_constructible<T>::value;

    /// @brief Type trait to check if a type is assignable from another type.
    /// @tparam T Type to check if is assignable to.
    /// @tparam U Type to check if is assignable from.
    template <typename T, typename U>
    struct is_assignable : bool_constant<__is_assignable(T, U)>
    {
    };

    /// @brief Type trait to check if a type is assignable from another type.
    /// @tparam T Type to check if is assignable to.
    /// @tparam U Type to check if is assignable from.
    template <typename T, typename U>
    inline constexpr bool is_assignable_v = is_assignable<T, U>::value;

    /// @brief Type trait to check if a type is trivially assignable from another type.
    /// @tparam T Type to check if is trivially assignable to.
    /// @tparam U Type to check if is trivially assignable from.
    template <typename T, typename U>
    struct is_trivially_assignable : bool_constant<__is_trivially_assignable(T, U)>
    {
    };

    /// @brief Type trait to check if a type is trivially assignable from another type.
    /// @tparam T Type to check if is trivially assignable to.
    /// @tparam U Type to check if is trivially assignable from.
    template <typename T, typename U>
    inline constexpr bool is_trivially_assignable_v = is_trivially_assignable<T, U>::value;

    /// @brief Type trait to check if a type is nothrow assignable from another type.
    /// @tparam T Type to check if is nothrow assignable to.
    /// @tparam U Type to check if is nothrow assignable from.
    template <typename T, typename U>
    struct is_nothrow_assignable : bool_constant<__is_nothrow_assignable(T, U)>
    {
    };

    /// @brief Type trait to check if a type is nothrow assignable from another type.
    /// @tparam T Type to check if is nothrow assignable to.
    /// @tparam U Type to check if is nothrow assignable from.
    template <typename T, typename U>
    inline constexpr bool is_nothrow_assignable_v = is_nothrow_assignable<T, U>::value;

    /// @brief Type trait to check if a type is copy assignable.
    /// @tparam T Type to check if is copy assignable.
    template <typename T>
    struct is_copy_assignable
        : bool_constant<is_assignable_v<add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>>>
    {
    };

    /// @brief Type trait to check if a type is copy assignable.
    /// @tparam T Type to check if is copy assignable.
    template <typename T>
    inline constexpr bool is_copy_assignable_v = is_copy_assignable<T>::value;

    /// @brief Type trait to check if a type is trivially copy assignable.
    /// @tparam T Type to check if is trivially copy assignable.
    template <typename T>
    struct is_trivially_copy_assignable
        : bool_constant<is_trivially_assignable_v<add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>>>
    {
    };

    /// @brief Type trait to check if a type is trivially copy assignable.
    /// @tparam T Type to check if is trivially copy assignable.
    template <typename T>
    inline constexpr bool is_trivially_copy_assignable_v = is_trivially_copy_assignable<T>::value;

    /// @brief Type trait to check if a type is nothrow copy assignable.
    /// @tparam T Type to check if is nothrow copy assignable.
    ///
    /// @note This trait is not fully compliant with the C++ standard on MSVC.
    template <typename T>
    struct is_nothrow_copy_assignable
        : bool_constant<is_nothrow_assignable_v<add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>>>
    {
    };

    /// @brief Type trait to check if a type is nothrow copy assignable.
    /// @tparam T Type to check if is nothrow copy assignable.
    ///
    /// @note This trait is not fully compliant with the C++ standard on MSVC.
    template <typename T>
    inline constexpr bool is_nothrow_copy_assignable_v = is_nothrow_copy_assignable<T>::value;

    /// @brief Type trait to check if a type is move assignable.
    /// @tparam T Type to check if is move assignable.
    template <typename T>
    struct is_move_assignable : bool_constant<is_assignable_v<add_lvalue_reference_t<T>, add_rvalue_reference_t<T>>>
    {
    };

    /// @brief Type trait to check if a type is move assignable.
    /// @tparam T Type to check if is move assignable.
    template <typename T>
    inline constexpr bool is_move_assignable_v = is_move_assignable<T>::value;

    /// @brief Type trait to check if a type is trivially move assignable.
    /// @tparam T Type to check if is trivially move assignable.
    template <typename T>
    struct is_trivially_move_assignable
        : bool_constant<is_trivially_assignable_v<add_lvalue_reference_t<T>, add_rvalue_reference_t<T>>>
    {
    };

    /// @brief Type trait to check if a type is trivially move assignable.
    /// @tparam T Type to check if is trivially move assignable.
    template <typename T>
    inline constexpr bool is_trivially_move_assignable_v = is_trivially_move_assignable<T>::value;

    /// @brief Type trait to check if a type is nothrow move assignable.
    /// @tparam T Type to check if is nothrow move assignable.
    ///
    /// @note This trait is not fully compliant with the C++ standard on MSVC.
    template <typename T>
    struct is_nothrow_move_assignable
        : bool_constant<is_nothrow_assignable_v<add_lvalue_reference_t<T>, add_rvalue_reference_t<T>>>
    {
    };

    /// @brief Type trait to check if a type is nothrow move assignable.
    /// @tparam T Type to check if is nothrow move assignable.
    ///
    /// @note This trait is not fully compliant with the C++ standard on MSVC.
    template <typename T>
    inline constexpr bool is_nothrow_move_assignable_v = is_nothrow_move_assignable<T>::value;

    /// @brief Type trait to check if a type is destructible.
    /// @tparam T Type to check if is destructible.
    template <typename T>
    struct is_destructible : bool_constant<__is_destructible(T)>
    {
    };

    /// @brief Type trait to check if a type is destructible.
    /// @tparam T Type to check if is destructible.
    template <typename T>
    inline constexpr bool is_destructible_v = is_destructible<T>::value;

    /// @brief Type trait to check if a type is trivially destructible.
    /// @tparam T Type to check if is trivially destructible.
    template <typename T>
#if defined(_MSC_VER) && !defined(__clang__)
    struct is_trivially_destructible : bool_constant<__has_trivial_destructor(T)>
#elif defined(__clang__) || defined(__GNUG__)
    struct is_trivially_destructible : bool_constant<__is_trivially_destructible(T)>
#else
#error "Compiler not supported."
#endif
    {
    };

    /// @brief Type trait to check if a type is trivially destructible.
    /// @tparam T Type to check if is trivially destructible.
    template <typename T>
    inline constexpr bool is_trivially_destructible_v = is_trivially_destructible<T>::value;

    /// @brief Type trait to check if a type is nothrow destructible.
    /// @tparam T Type to check if is nothrow destructible.
    template <typename T>
    struct is_nothrow_destructible : bool_constant<__is_nothrow_destructible(T)>
    {
    };

    /// @brief Type trait to check if a type is nothrow destructible.
    /// @tparam T Type to check if is nothrow destructible.
    template <typename T>
    inline constexpr bool is_nothrow_destructible_v = is_nothrow_destructible<T>::value;

    /// @brief Type trait to check if a type has a virtual destructor.
    /// @tparam T Type to check if has a virtual destructor.
    template <typename T>
    struct has_virtual_destructor : bool_constant<__has_virtual_destructor(T)>
    {
    };

    /// @brief Type trait to check if a type has a virtual destructor.
    /// @tparam T Type to check if has a virtual destructor.
    template <typename T>
    inline constexpr bool has_virtual_destructor_v = has_virtual_destructor<T>::value;

    /// @brief Converts any type T to a reference type
    /// @tparam T Type to convert to a reference type.
    ///
    /// @note Implementation note: This function will always static_assert false if called in an evaluated context. This
    /// is to prevent an ill-formed implementation via ODR violation.
    template <typename T>
    inline add_rvalue_reference_t<T> declval() noexcept
    {
        static_assert(false, "declval is not allowed in an evaluated context.");
    }

    /// @brief Type trait used to perform the type conversion when a type is passed by value to a function.
    /// @tparam T Type to decay.
    template <typename T>
    struct decay
    {
      private:
        using U = remove_reference_t<T>;

      public:
        /// @brief The decayed type.
        using type = conditional_t<is_array_v<U>, add_pointer_t<remove_extent_t<U>>,
                                   conditional_t<is_function_v<U>, add_pointer_t<U>, remove_cv_t<U>>>;
    };

    /// @brief Type trait used to perform the type conversion when a type is passed by value to a function.
    /// @tparam T Type to decay.
    template <typename T>
    using decay_t = typename decay<T>::type;

    template <bool B, typename T = void>
    struct enable_if
    {
    };

    template <typename T>
    struct enable_if<true, T>
    {
        using type = T;
    };

    template <bool B, typename T = void>
    using enable_if_t = typename enable_if<B, T>::type;

    template <typename T>
        requires(is_nothrow_move_constructible_v<T> && is_nothrow_move_assignable_v<T>)
    inline constexpr void swap(T& a,
                               T& b) noexcept(is_nothrow_move_constructible_v<T> && is_nothrow_move_assignable_v<T>);

    template <typename T>
    constexpr T&& forward(remove_reference_t<T>& t) noexcept;

    template <typename T>
    constexpr T&& forward(remove_reference_t<T>&& t) noexcept;

    namespace detail
    {
        template <typename T, typename U>
        concept is_swappable_with_impl = requires(T& t, U& u) {
            swap(t, u);
            swap(u, t);
        };

        template <typename T, typename U>
        concept swap_cannot_throw_impl = requires(T& t, U& u) {
            noexcept(swap(t, u));
            noexcept(swap(u, t));
        };
    } // namespace detail

    /// @brief Concept to check if two types are swappable with each other.
    /// @tparam T Type to check if is swappable with U.
    /// @tparam U Type to check if is swappable with T.
    template <typename T, typename U>
    struct is_swappable_with : bool_constant<detail::is_swappable_with_impl<T, U>>
    {
    };

    /// @brief Concept to check if two types are swappable with each other.
    /// @tparam T Type to check if is swappable with U.
    /// @tparam U Type to check if is swappable with T.
    template <typename T, typename U>
    inline constexpr bool is_swappable_with_v = is_swappable_with<T, U>::value;

    /// @brief Type trait to check if a type is swappable.
    /// @tparam T Type to check if is swappable.
    template <typename T>
    struct is_swappable : is_swappable_with<T, T>
    {
    };

    /// @brief Type trait to check if a type is swappable.
    /// @tparam T Type to check if is swappable.
    template <typename T>
    inline constexpr bool is_swappable_v = is_swappable<T>::value;

    /// @brief Type trait to check if a type is nothrow swappable with another type.
    /// @tparam T Type to check if is nothrow swappable with U.
    /// @tparam U Type to check if is nothrow swappable with T.
    template <typename T, typename U>
    struct is_nothrow_swappable_with
        : bool_constant<detail::swap_cannot_throw_impl<T, U> &&
                        is_swappable_with_v<add_lvalue_reference_t<T>, add_lvalue_reference_t<U>>>
    {
    };

    /// @brief Type trait to check if a type is nothrow swappable with another type.
    /// @tparam T Type to check if is nothrow swappable with U.
    /// @tparam U Type to check if is nothrow swappable with T.
    template <typename T, typename U>
    inline constexpr bool is_nothrow_swappable_with_v = is_nothrow_swappable_with<T, U>::value;

    /// @brief Type trait to check if a type is nothrow swappable.
    /// @tparam T Type to check if is nothrow swappable.
    template <typename T>
    struct is_nothrow_swappable : is_nothrow_swappable_with<T, T>
    {
    };

    /// @brief Type trait to check if a type is nothrow swappable.
    /// @tparam T Type to check if is nothrow swappable.
    template <typename T>
    inline constexpr bool is_nothrow_swappable_v = is_nothrow_swappable<T>::value;

    namespace detail
    {
        template <typename T>
        auto test_returnable(int) -> decltype(void(static_cast<T (*)()>(nullptr)), true_type{});

        template <typename>
        auto test_returnable(...) -> false_type;

        template <typename From, typename To>
        auto test_implicitly_convertable(int) -> decltype(void(declval<void (&)(To)>()(declval<From>())), true_type{});

        template <typename, typename>
        auto test_implicitly_convertable(...) -> false_type;

        template <typename From, typename To>
        struct is_convertible_fallback : bool_constant<(decltype(test_returnable<To>(0))::value &&
                                                        decltype(test_implicitly_convertable<From, To>(0))::value) ||
                                                       (is_void_v<From> && is_void_v<To>)>
        {
        };
    } // namespace detail

#if defined(_MSC_VER)
    /// @brief Type trait to check if a type is convertible to another type.
    /// @tparam From Type to check if is convertible from.
    /// @tparam To Type to check if is convertible to.
    template <typename From, typename To>
    struct is_convertible : bool_constant<__is_convertible_to(From, To)>
    {
    };
#elif defined(__clang__)
    /// @brief Type trait to check if a type is convertible to another type.
    /// @tparam From Type to check if is convertible from.
    /// @tparam To Type to check if is convertible to.
    template <typename From, typename To>
    struct is_convertible : bool_constant<__is_convertible(From, To)>
    {
    };
#else
    /// @brief Type trait to check if a type is convertible to another type.
    /// @tparam From Type to check if is convertible from.
    /// @tparam To Type to check if is convertible to.
    template <typename From, typename To>
    struct is_convertible : detail::is_convertible_fallback<From, To>
    {
    };
#endif

    /// @brief Type trait to check if a type is convertible to another type.
    /// @tparam From Type to check if is convertible from.
    /// @tparam To Type to check if is convertible to.
    template <typename From, typename To>
    inline constexpr bool is_convertible_v = is_convertible<From, To>::value;

    namespace detail
    {
        template <typename From, typename To>
        struct is_nothrow_convertible_fallback : conjunction<is_void<From>, is_void<To>>
        {
        };

        template <typename From, typename To>
            requires requires {
                static_cast<To (*)()>(nullptr);
                { declval<void (&)(To) noexcept>()(declval<From>()) } noexcept;
            }
        struct is_nothrow_convertible_fallback<From, To> : true_type
        {
        };
    } // namespace detail

#if defined(_MSC_VER)
    /// @brief Type trait to check if a type is nothrow convertible to another type.
    /// @tparam From Type to check if is nothrow convertible from.
    /// @tparam To Type to check if is nothrow convertible to.
    template <typename From, typename To>
    struct is_nothrow_convertible : detail::is_nothrow_convertible_fallback<From, To>
    {
    };
#else
    /// @brief Type trait to check if a type is nothrow convertible to another type.
    /// @tparam From Type to check if is nothrow convertible from.
    /// @tparam To Type to check if is nothrow convertible to.
    template <typename From, typename To>
    struct is_nothrow_convertible : bool_constant<__is_nothrow_convertible(From, To)>
    {
    };
#endif

    /// @brief Type trait to check if a type is nothrow convertible to another type.
    /// @tparam From Type to check if is nothrow convertible from.
    /// @tparam To Type to check if is nothrow convertible to.
    template <typename From, typename To>
    inline constexpr bool is_nothrow_convertible_v = is_nothrow_convertible<From, To>::value;

#if defined(_MSC_VER) && !defined(__clang__)
    /// @brief Type trait to check if two types are layout compatible.
    /// @tparam T Type to check if is layout compatible with U.
    /// @tparam U Type to check if is layout compatible with T.
    template <typename T, typename U>
    struct is_layout_compatible : bool_constant<__is_layout_compatible(T, U)>
    {
    };

    /// @brief Type trait to check if two types are layout compatible.
    /// @tparam T Type to check if is layout compatible with U.
    /// @tparam U Type to check if is layout compatible with T.
    template <typename T, typename U>
    inline constexpr bool is_layout_compatible_v = is_layout_compatible<T, U>::value;

    /// @brief Type trait to check if a pointer is interconvertible with its base type.
    /// @tparam Base Base type to check if is interconvertible with Derived.
    /// @tparam Derived Derived type to check if is interconvertible with Base.
    template <typename Base, typename Derived>
    struct is_pointer_interconvertible_base_of : bool_constant<__is_pointer_interconvertible_base_of(Base, Derived)>
    {
    };

    /// @brief Type trait to check if a pointer is interconvertible with its base type.
    /// @tparam Base Base type to check if is interconvertible with Derived.
    /// @tparam Derived Derived type to check if is interconvertible with Base.
    template <typename Base, typename Derived>
    inline constexpr bool is_pointer_interconvertible_base_of_v =
        is_pointer_interconvertible_base_of<Base, Derived>::value;

#endif

    namespace detail
    {
        struct invoke_member_function_ref
        {
        };
        struct invoke_member_function_deref
        {
        };
        struct invoke_member_object_ref
        {
        };
        struct invoke_member_object_deref
        {
        };
        struct invoke_other
        {
        };

        template <typename T>
        struct success_type
        {
            using type = T;
        };

        struct failure_type
        {
        };

        template <typename T, typename Tag>
        struct result_of_success : success_type<T>
        {
            using invoke_type = Tag;
        };

        struct result_of_member_function_ref_impl
        {
            template <typename Fn, typename Tp, typename... Args>
            static result_of_success<decltype((declval<Tp>().*declval<Fn>())(declval<Args>()...)),
                                     invoke_member_function_ref>
            test(int);

            template <typename...>
            static failure_type test(...);
        };

        template <typename MemPtr, typename Arg, typename... Args>
        struct result_of_member_function_ref : private result_of_member_function_ref_impl
        {
            using type = decltype(test<MemPtr, Arg, Args...>(0));
        };

        struct result_of_member_function_deref_impl
        {
            template <typename Fn, typename Tp, typename... Args>
            static result_of_success<decltype(((*declval<Tp>()).*declval<Fn>())(declval<Args>()...)),
                                     invoke_member_function_deref>
            test(int);

            template <typename...>
            static failure_type test(...);
        };

        template <typename MemPtr, typename Arg, typename... Args>
        struct result_of_member_function_deref : private result_of_member_function_deref_impl
        {
            using type = decltype(test<MemPtr, Arg, Args...>(0));
        };

        template <typename MemPtr, typename Arg, typename... Args>
        struct result_of_member_function;

        template <typename Res, typename T, typename Arg, typename... Args>
        struct result_of_member_function<Res T::*, Arg, Args...>
        {
            using arg_val = remove_cvref_t<Arg>;
            using mem_ptr = Res T::*;
            using type = typename conditional_t<is_base_of<T, arg_val>::value,
                                                result_of_member_function_ref<mem_ptr, Arg, Args...>,
                                                result_of_member_function_deref<mem_ptr, Arg, Args...>>::type;
        };

        struct result_of_member_object_ref_impl
        {
            template <typename MemPtr, typename Tp>
            static result_of_success<decltype(declval<Tp>().*declval<MemPtr>()), invoke_member_object_ref> test(int);

            template <typename...>
            static failure_type test(...);
        };

        template <typename MemPtr, typename Arg>
        struct result_of_member_object_ref : private result_of_member_object_ref_impl
        {
            using type = decltype(test<MemPtr, Arg>(0));
        };

        struct result_of_member_object_deref_impl
        {
            template <typename MemPtr, typename Tp>
            static result_of_success<decltype((*declval<Tp>()).*declval<MemPtr>()), invoke_member_object_deref> test(
                int);

            template <typename...>
            static failure_type test(...);
        };

        template <typename MemPtr, typename Arg>
        struct result_of_member_object_deref : private result_of_member_object_deref_impl
        {
            using type = decltype(test<MemPtr, Arg>(0));
        };

        template <typename MemPtr, typename Arg>
        struct result_of_member_object;

        template <typename Res, typename T, typename Arg>
        struct result_of_member_object<Res T::*, Arg>
        {
            using arg_val = remove_cvref_t<Arg>;
            using mem_ptr = Res T::*;
            using type = typename conditional_t<disjunction_v<is_same<arg_val, T>, is_base_of<T, arg_val>>,
                                                result_of_member_object_ref<mem_ptr, Arg>,
                                                result_of_member_function_deref<mem_ptr, Arg>>::type;
        };

        template <typename T, typename U = remove_cvref_t<T>>
        struct invoke_unwrap
        {
            using type = T;
        };

        template <typename T, typename U>
        struct invoke_unwrap<T, reference_wrapper<U>>
        {
            using type = U&;
        };

        template <bool, bool, typename Fn, typename... Args>
        struct result_of_impl
        {
            using type = failure_type;
        };

        template <typename MemPtr, typename Arg>
        struct result_of_impl<true, false, MemPtr, Arg>
            : public result_of_member_object<decay_t<MemPtr>, typename invoke_unwrap<Arg>::type>
        {
        };

        template <typename MemPtr, typename Arg, typename... Args>
        struct result_of_impl<false, true, MemPtr, Arg, Args...>
            : public result_of_member_function<decay_t<MemPtr>, typename invoke_unwrap<Arg>::type, Args...>
        {
        };

        struct result_of_other_impl
        {
            template <typename Fn, typename... Args>
            static result_of_success<decltype(declval<Fn>()(declval<Args>()...)), invoke_other> test(int);

            template <typename...>
            static failure_type test(...);
        };

        template <typename Fn, typename... Args>
        struct result_of_impl<false, false, Fn, Args...> : private result_of_other_impl
        {
            using type = decltype(test<Fn, Args...>(0));
        };

        template <typename Fn, typename... Args>
        struct invoke_result_impl
            : public result_of_impl<is_member_object_pointer_v<remove_reference_t<Fn>>,
                                    is_member_function_pointer_v<remove_reference_t<Fn>>, Fn, Args...>::type
        {
        };

        template <typename Fn, typename... Args>
        using invoke_result_impl_t = typename invoke_result_impl<Fn, Args...>::type;

        template <typename Result, typename Ret, bool = is_void_v<Ret>, typename = void>
        struct is_invocable_impl : false_type
        {
            using nothrow_conv = false_type;
        };

        template <typename Result, typename Ret>
        struct is_invocable_impl<Result, Ret, true, void_t<typename Result::type>> : true_type
        {
            using nothrow_conv = true_type;
        };

        template <typename Result, typename Ret>
        struct is_invocable_impl<Result, Ret, false, void_t<typename Result::type>>
        {
          private:
            using result_t = typename Result::type;
            static result_t s_get() noexcept;

            template <typename Tp>
            static void s_conv(type_identity_t<Tp>) noexcept;

            template <typename Tp, bool nothrow = noexcept(s_conv<Tp>(s_get())),
                      typename = decltype(s_conv<Tp>(s_get())), bool dangle = false>
            static bool_constant<nothrow && !dangle> test(int);

            template <typename Tp, bool = false>
            static false_type test(...);

          public:
            using type = decltype(test<Ret, true>(1));

            using nothrow_conv = decltype(test<Ret>(1));
        };

        template <typename Fn, typename ArgTypes>
        struct is_invocable_helper : is_invocable_impl<invoke_result_impl<Fn, ArgTypes>, void>::type
        {
        };

        template <typename Fn, typename T, typename... Args>
        constexpr bool is_nothrow_callable(invoke_member_function_ref)
        {
            using U = typename invoke_unwrap<T>::type;
            return noexcept((tempest::declval<U>().*tempest::declval<Fn>())(tempest::declval<Args>()...));
        }

        template <typename Fn, typename T, typename... Args>
        constexpr bool is_nothrow_callable(invoke_member_function_deref)
        {
            using U = typename invoke_unwrap<T>::type;
            return noexcept(((*tempest::declval<U>()).*tempest::declval<Fn>())(tempest::declval<Args>()...));
        }

        template <typename Fn, typename T, typename... Args>
        constexpr bool is_nothrow_callable(invoke_member_object_ref)
        {
            using U = typename invoke_unwrap<T>::type;
            return noexcept(tempest::declval<U>().*tempest::declval<Fn>());
        }

        template <typename Fn, typename T, typename... Args>
        constexpr bool is_nothrow_callable(invoke_member_object_deref)
        {
            using U = typename invoke_unwrap<T>::type;
            return noexcept((*tempest::declval<U>()).*tempest::declval<Fn>());
        }

        template <typename Fn, typename... Args>
        constexpr bool is_nothrow_callable(invoke_other)
        {
            return noexcept(tempest::declval<Fn>()(tempest::declval<Args>()...));
        }

        template <typename Result, typename Fn, typename... Args>
        struct is_nothrow_call : bool_constant<is_nothrow_callable<Fn, Args...>(typename Result::invoke_type{})>
        {
        };

        template <typename Fn, typename... Args>
        using is_nothrow_call_helper = is_nothrow_call<invoke_result_impl<Fn, Args...>, Fn, Args...>;
    } // namespace detail

    /// @brief Type trait to get the result of invoking a function with a set of arguments.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Fn, typename... Args>
    struct invoke_result : public detail::invoke_result_impl<Fn, Args...>
    {
    };

    /// @brief Type trait to get the result of invoking a function with a set of arguments.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Fn, typename... Args>
    using invoke_result_t = typename invoke_result<Fn, Args...>::type;

    /// @brief Type trait to get if the function is invocable with a set of arguments.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Fn, typename... Args>
    struct is_invocable : public detail::is_invocable_impl<detail::invoke_result_impl<Fn, Args...>, void>::type
    {
    };

    /// @brief Type trait to get if the function is invocable with a set of arguments.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Fn, typename... Args>
    inline constexpr bool is_invocable_v = is_invocable<Fn, Args...>::value;

    /// @brief Type trait to get if the function is invocable with a set of arguments and returns a specific type.
    /// @tparam Ret Return type of the function.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Ret, typename Fn, typename... Args>
    struct is_invocable_r : public detail::is_invocable_impl<detail::invoke_result_impl<Fn, Args...>, Ret>::type
    {
    };

    /// @brief Type trait to get if the function is invocable with a set of arguments and returns a specific type.
    /// @tparam Ret Return type of the function.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Ret, typename Fn, typename... Args>
    inline constexpr bool is_invocable_r_v = is_invocable_r<Ret, Fn, Args...>::value;

    /// @brief Type trait to get if the function is nothrow invocable with a set of arguments.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Fn, typename... Args>
    struct is_nothrow_invocable : conjunction<detail::is_invocable_impl<detail::invoke_result_impl<Fn, Args...>, void>,
                                              detail::is_nothrow_call_helper<Fn, Args...>>::type
    {
    };

    /// @brief Type trait to get if the function is nothrow invocable with a set of arguments.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Fn, typename... Args>
    inline constexpr bool is_nothrow_invocable_v = is_nothrow_invocable<Fn, Args...>::value;

    namespace detail
    {
        template <typename Result, typename Ret>
        using is_nothrow_invocable_impl = typename is_invocable_impl<Result, Ret>::nothrow_conv;
    }

    /// @brief Type trait to get if the function is nothrow invocable with a set of arguments and returns a specific
    /// type.
    /// @tparam Ret Return type of the function.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Ret, typename Fn, typename... Args>
    struct is_nothrow_invocable_r
        : conjunction<detail::is_nothrow_invocable_impl<detail::invoke_result_impl<Fn, Args...>, Ret>,
                      detail::is_nothrow_call_helper<Fn, Args...>>::type
    {
    };

    /// @brief Type trait to get if the function is nothrow invocable with a set of arguments and returns a specific
    /// type.
    /// @tparam Ret Return type of the function.
    /// @tparam Fn Function type to invoke.
    /// @tparam ...Args Argument types to pass to the function.
    template <typename Ret, typename Fn, typename... Args>
    inline constexpr bool is_nothrow_invocable_r_v = is_nothrow_invocable_r<Ret, Fn, Args...>::value;

    namespace detail
    {
        template <typename T1, typename T2>
        using conditional_result_t = decltype(false ? tempest::declval<T1>() : tempest::declval<T2>());

        template <typename T1, typename T2>
        struct const_lvalue_conditional_operator
        {
        };

        template <typename T1, typename T2>
            requires requires { typename conditional_result_t<const T1&, const T2&>; }
        struct const_lvalue_conditional_operator<T1, T2>
        {
            using type = remove_cvref_t<conditional_result_t<const T1&, const T2&>>;
        };

        template <typename T1, typename T2, typename = void>
        struct decayed_conditional_operator : const_lvalue_conditional_operator<T1, T2>
        {
        };

        template <typename T1, typename T2>
        struct decayed_conditional_operator<T1, T2, void_t<conditional_result_t<T1, T2>>>
        {
            using type = decay_t<conditional_result_t<T1, T2>>;
        };
    } // namespace detail

    /// @brief Type trait to get the common type of a set of types.
    /// @tparam ...
    template <typename...>
    struct common_type;

    template <typename... Ts>
    using common_type_t = typename common_type<Ts...>::type;

    template <>
    struct common_type<>
    {
    };

    /// @brief Type trait to get the common type of a set of types.
    /// @tparam T Type to get the common type of.
    template <typename T>
    struct common_type<T> : common_type<T, T>
    {
    };

    namespace detail
    {
        template <typename T1, typename T2, typename DecayedT1 = decay_t<T1>, typename DecayedT2 = decay_t<T2>>
        struct common_type_impl_2 : common_type<DecayedT1, DecayedT2>
        {
        };

        template <typename T1, typename T2>
        struct common_type_impl_2<T1, T2, T1, T2> : decayed_conditional_operator<T1, T2>
        {
        };
    } // namespace detail

    /// @brief Type trait to get the common type of a set of types.
    /// @tparam T1 Type to get the common type of.
    /// @tparam T2 Type to get the common type of.
    template <typename T1, typename T2>
    struct common_type<T1, T2> : detail::common_type_impl_2<T1, T2>
    {
    };

    namespace detail
    {
        template <typename Void, typename T1, typename T2, typename... Ts>
        struct common_type_impl_multitype
        {
        };

        template <typename T1, typename T2, typename... Ts>
        struct common_type_impl_multitype<void_t<typename common_type<T1, T2>::type>, T1, T2, Ts...>
            : common_type<typename common_type<T1, T2>::type, Ts...>
        {
        };
    } // namespace detail

    /// @brief Type trait to get the common type of a set of types.
    /// @tparam T1 Type to get the common type of.
    /// @tparam T2 Type to get the common type of.
    /// @tparam ...Ts Types to get the common type of.
    template <typename T1, typename T2, typename... Ts>
    struct common_type<T1, T2, Ts...> : detail::common_type_impl_multitype<void, T1, T2, Ts...>
    {
    };

    namespace detail
    {
        template <typename T>
        T returns_same_type() noexcept;
    }

    template <typename, typename, template <typename> typename, template <typename> typename>
    struct basic_common_reference
    {
    };

    namespace detail
    {
        template <typename T>
        struct copy_cv_qual_impl
        {
            template <typename U>
            using type = U;
        };

        template <typename T>
        struct copy_cv_qual_impl<const T>
        {
            template <typename U>
            using type = const U;
        };

        template <typename T>
        struct copy_cv_qual_impl<volatile T>
        {
            template <typename U>
            using type = volatile U;
        };

        template <typename T>
        struct copy_cv_qual_impl<const volatile T>
        {
            template <typename U>
            using type = const volatile U;
        };

        template <typename From, typename To>
        using copy_cv_qual = copy_cv_qual_impl<From>::template type<To>;

        template <typename T>
        struct add_ref_qualifiers
        {
            template <typename U>
            using type = copy_cv_qual<T, U>;
        };

        template <typename T>
        struct add_ref_qualifiers<T&>
        {
            template <typename U>
            using type = add_lvalue_reference_t<copy_cv_qual<T, U>>;
        };

        template <typename T>
        struct add_ref_qualifiers<T&&>
        {
            template <typename U>
            using type = add_rvalue_reference_t<copy_cv_qual<T, U>>;
        };

        template <typename T1, typename T2>
        using conditional_ref_result_t = decltype(false ? returns_same_type<T1>() : returns_same_type<T2>());
    } // namespace detail

    template <typename...>
    struct common_reference;

    template <typename... Ts>
    using common_reference_t = typename common_reference<Ts...>::type;

    template <>
    struct common_reference<>
    {
    };

    template <typename T>
    struct common_reference<T>
    {
        using type = T;
    };

    namespace detail
    {
        template <typename T1, typename T2>
        struct common_reference_impl_2_base : common_type<T1, T2>
        {
        };

        template <typename T1, typename T2>
            requires requires { typename conditional_ref_result_t<T1, T2>; }
        struct common_reference_impl_2_base<T1, T2>
        {
            using type = conditional_ref_result_t<T1, T2>;
        };

        template <typename T1, typename T2>
        using bcr_specialization =
            basic_common_reference<remove_cvref_t<T1>, remove_cvref_t<T2>, add_ref_qualifiers<T1>::template type,
                                   add_ref_qualifiers<T2>::template type>::type;

        template <typename T1, typename T2>
        struct common_reference_impl_2_bcr : common_reference_impl_2_base<T1, T2>
        {
        };

        template <typename T1, typename T2>
            requires requires { typename bcr_specialization<T1, T2>; }
        struct common_reference_impl_2_bcr<T1, T2>
        {
            using type = bcr_specialization<T1, T2>;
        };

        template <typename T1, typename T2>
            requires is_lvalue_reference_v<conditional_ref_result_t<copy_cv_qual<T1, T2>&, copy_cv_qual<T2, T1>&>>
        using double_lvalue_common_ref_t = conditional_ref_result_t<copy_cv_qual<T1, T2>&, copy_cv_qual<T2, T1>&>;

        template <typename T1, typename T2>
        struct common_reference_impl_2_refs
        {
        };

        template <typename T1, typename T2>
            requires requires { typename double_lvalue_common_ref_t<T1, T2>; }
        struct common_reference_impl_2_refs<T1&, T2&>
        {
            using type = double_lvalue_common_ref_t<T1, T2>;
        };

        template <typename T1, typename T2>
            requires is_convertible_v<T1&&, double_lvalue_common_ref_t<const T1, T2>>
        struct common_reference_impl_2_refs<T1&&, T2&>
        {
            using type = double_lvalue_common_ref_t<const T1, T2>;
        };

        template <typename T1, typename T2>
            requires is_convertible_v<T2&&, double_lvalue_common_ref_t<const T2, T1>>
        struct common_reference_impl_2_refs<T1&, T2&&>
        {
            using type = double_lvalue_common_ref_t<const T2, T1>;
        };

        template <typename T1, typename T2>
        using double_rvalue_common_ref = remove_reference_t<double_lvalue_common_ref_t<T1, T2>>&&;

        template <typename T1, typename T2>
            requires is_convertible_v<T1&&, double_rvalue_common_ref<T1, T2>> &&
                     is_convertible_v<T2&&, double_rvalue_common_ref<T1, T2>>
        struct common_reference_impl_2_refs<T1&&, T2&&>
        {
            using type = double_rvalue_common_ref<T1, T2>;
        };

        template <typename T1, typename T2>
        using common_reference_impl_2_refs_t = common_reference_impl_2_refs<T1, T2>::type;

        template <typename T1, typename T2>
        struct common_reference_impl_2 : common_reference_impl_2_bcr<T1, T2>
        {
        };

        template <typename T1, typename T2>
            requires is_convertible_v<add_pointer_t<T1>, add_pointer_t<common_reference_impl_2_refs_t<T1, T2>>> &&
                     is_convertible_v<add_pointer_t<T2>, add_pointer_t<common_reference_impl_2_refs_t<T1, T2>>>
        struct common_reference_impl_2<T1, T2>
        {
            using type = common_reference_impl_2_refs_t<T1, T2>;
        };
    } // namespace detail

    template <typename T1, typename T2>
    struct common_reference<T1, T2> : detail::common_reference_impl_2<T1, T2>
    {
    };

    template <typename T1, typename T2, typename T3, typename... Ts>
    struct common_reference<T1, T2, T3, Ts...>
    {
    };

    template <typename T1, typename T2, typename T3, typename... Ts>
        requires requires { typename common_reference_t<T1, T2>; }
    struct common_reference<T1, T2, T3, Ts...> : common_reference<common_reference_t<T1, T2>, T3, Ts...>
    {
    };

    namespace detail
    {
        template <typename T, template <typename...> typename Tpl, typename... Ts>
        struct is_specialization_of : false_type
        {
        };

        template <typename... Ts, template <typename...> typename Tpl>
        struct is_specialization_of<Tpl<Ts...>, Tpl> : true_type
        {
        };

        template <typename T, template <typename...> typename Tpl>
        inline constexpr bool is_specialization_of_v = is_specialization_of<T, Tpl>::value;

        template <typename RefWrap, typename T, typename RefWrapQ, typename TQ>
        concept ref_wrap_common_reference_exists_with = is_specialization_of_v<RefWrap, reference_wrapper> && requires {
            typename common_reference_t<typename RefWrap::type&, TQ>;
        } && is_convertible_v<RefWrapQ, common_reference_t<typename RefWrap::type&, TQ>>;
    } // namespace detail

    template <typename RefWrap, typename T, template <typename> typename RefWrapQ, template <typename> typename TQ>
        requires(detail::ref_wrap_common_reference_exists_with<RefWrap, T, RefWrapQ<T>, TQ<T>> &&
                 !detail::ref_wrap_common_reference_exists_with<T, RefWrap, TQ<T>, RefWrapQ<T>>)
    struct basic_common_reference<RefWrap, T, RefWrapQ, TQ>
    {
        using type = common_reference_t<typename RefWrap::type&, TQ<T>>;
    };

    template <typename T, typename RefWrap, template <typename> typename TQ, template <typename> typename RefWrapQ>
        requires(detail::ref_wrap_common_reference_exists_with<RefWrap, T, RefWrapQ<T>, TQ<T>> &&
                 !detail::ref_wrap_common_reference_exists_with<T, RefWrap, TQ<T>, RefWrapQ<T>>)
    struct basic_common_reference<T, RefWrap, TQ, RefWrapQ>
    {
        using type = common_reference_t<typename RefWrap::type&, TQ<T>>;
    };

    // template <typename T, typename U, template <typename> typename TQual, template <typename> typename UQual>
    // struct basic_common_reference
    // {
    // };

    // namespace detail
    // {
    //     template <typename T, bool IsConst, bool IsVolatile>
    //     struct cv_selector;

    //     template <typename T>
    //     struct cv_selector<T, false, false>
    //     {
    //         using type = T;
    //     };

    //     template <typename T>
    //     struct cv_selector<T, true, false>
    //     {
    //         using type = const T;
    //     };

    //     template <typename T>
    //     struct cv_selector<T, false, true>
    //     {
    //         using type = volatile T;
    //     };

    //     template <typename T>
    //     struct cv_selector<T, true, true>
    //     {
    //         using type = const volatile T;
    //     };

    //     template <typename T, typename U>
    //     struct copy_cv
    //     {
    //         using type = typename cv_selector<U, is_const_v<T>, is_volatile_v<T>>::type;
    //     };

    //     template <typename T, typename U>
    //     using copy_cv_t = typename copy_cv<T, U>::type;

    //     template <typename T>
    //     struct xref
    //     {
    //         template <typename U>
    //         using type = copy_cv_t<T, U>;
    //     };

    //     template <typename T>
    //     struct xref<T&>
    //     {
    //         template <typename U>
    //         using type = copy_cv_t<T, U>&;
    //     };

    //     template <typename T>
    //     struct xref<T&&>
    //     {
    //         template <typename U>
    //         using type = copy_cv_t<T, U>&&;
    //     };

    //     template <typename T, typename U>
    //     using basic_common_ref_impl = basic_common_reference<remove_cvref_t<T>, remove_cvref_t<U>,
    //                                                          xref<T>::template type, xref<U>::template type>::type;
    // } // namespace detail

    // /// @brief Type trait to get the common reference type of a set of types.
    // /// @tparam ...T Types to get the common reference type of.
    // template <typename... T>
    // struct common_reference;

    // /// @brief Type trait to get the common reference type of a set of types.
    // template <>
    // struct common_reference<>
    // {
    // };

    // /// @brief Type trait to get the common reference type of a set of types.
    // /// @tparam T Type to get the common reference type of.
    // template <typename T>
    // struct common_reference<T>
    // {
    //     using type = T;
    // };

    // namespace detail
    // {
    //     template <typename T, typename U>
    //     using conditional_ref_result_t = decltype(false ? declval<T (&)()>()() : declval<U (&)()>()());

    //     template <typename T, typename U>
    //     using conditional_ref_result_cvref_t = conditional_ref_result_t<copy_cv_t<T, U>&, copy_cv_t<U, T>&>;

    //     template <typename T, typename U, typename = void>
    //     struct common_ref_impl
    //     {
    //     };

    //     template <typename T, typename U>
    //     using common_ref = typename common_ref_impl<T, U>::type;

    //     template <typename T, typename U>
    //     struct common_ref_impl<T&, U&, void_t<conditional_ref_result_t<T, U>>>
    //         : enable_if<is_reference_v<conditional_ref_result_cvref_t<T, U>>, conditional_ref_result_cvref_t<T, U>>
    //     {
    //     };

    //     template <typename T, typename U>
    //     using common_ref_c = remove_reference_t<common_ref<T&, U&>>&&;

    //     template <typename T, typename U>
    //     struct common_ref_impl<
    //         T&&, U&&,
    //         enable_if_t<is_convertible_v<T&&, common_ref_c<T, U>> && is_convertible_v<U&&, common_ref_c<T, U>>>>
    //     {
    //         using type = common_ref_c<T, U>;
    //     };

    //     template <typename T, typename U>
    //     using common_ref_d = common_ref<const T&, U&>;

    //     template <typename T, typename U>
    //     struct common_ref_impl<T&&, U&, enable_if_t<is_convertible_v<T&&, common_ref_d<T, U>>>>
    //     {
    //         using type = common_ref_d<T, U>;
    //     };

    //     template <typename T, typename U>
    //     struct common_ref_impl<T&, U&&> : common_ref_impl<U&&, T&>
    //     {
    //     };

    //     template <typename T, typename U, int Bullet = 1, typename = void>
    //     struct common_reference_impl : common_reference_impl<U, T, Bullet + 1>
    //     {
    //     };
    // } // namespace detail

    // template <typename T, typename U>
    // struct common_reference<T, U> : detail::common_reference_impl<T, U>
    // {
    // };

    // namespace detail
    // {
    //     template <typename T, typename U>
    //     struct common_reference_impl<T&, U&, 1, void_t<common_ref<T&, U&>>>
    //     {
    //         using type = common_ref<T&, U&>;
    //     };

    //     template <typename T, typename U>
    //     struct common_reference_impl<T&&, U&&, 1, void_t<common_ref<T&&, U&&>>>
    //     {
    //         using type = common_ref<T&&, U&&>;
    //     };

    //     template <typename T, typename U>
    //     struct common_reference_impl<T&&, U&, 1, void_t<common_ref<T&&, U&>>>
    //     {
    //         using type = common_ref<T&&, U&>;
    //     };

    //     template <typename T, typename U>
    //     struct common_reference_impl<T&, U&&, 1, void_t<common_ref<T&, U&&>>>
    //     {
    //         using type = common_ref<T&, U&&>;
    //     };

    //     template <typename T, typename U>
    //     struct common_reference_impl<T, U, 2, void_t<basic_common_ref_impl<T, U>>>
    //     {
    //         using type = basic_common_ref_impl<T, U>;
    //     };

    //     template <typename T, typename U>
    //     struct common_reference_impl<T, U, 3, void_t<conditional_ref_result_t<T, U>>>
    //     {
    //         using type = conditional_ref_result_t<T, U>;
    //     };

    //     template <typename T, typename U>
    //     struct common_reference_impl<T, U, 4, void_t<common_type_t<T, U>>>
    //     {
    //         using type = common_type_t<T, U>;
    //     };

    //     template <typename T, typename U>
    //     struct common_reference_impl<T, U, 5, void>
    //     {
    //     };

    //     template <typename...>
    //     struct common_type_pack
    //     {
    //     };

    //     template <typename, typename, typename = void>
    //     struct common_type_fold;

    //     template <typename T, typename... Ts>
    //     struct common_type_fold<T, common_type_pack<Ts...>, void_t<typename T::type>>
    //         : public common_type<typename T::type, Ts...>
    //     {
    //     };

    //     template <typename T, typename U>
    //     struct common_type_fold<T, U, void>
    //     {
    //     };
    // } // namespace detail

    // /// @brief Type trait to get the common reference type of a set of types.
    // /// @tparam T Type to get the common reference type of.
    // /// @tparam U Type to get the common reference type of.
    // template <typename T, typename U, typename... Ts>
    // struct common_reference<T, U, Ts...>
    //     : detail::common_type_fold<common_reference<T, U>, detail::common_type_pack<Ts...>>
    // {
    // };

    // namespace detail
    // {
    //     template <typename T1, typename T2, typename... Ts>
    //     struct common_type_fold<common_reference<T1, T2>, common_type_pack<Ts...>,
    //                             void_t<typename common_reference<T1, T2>::type>>
    //         : common_reference<typename common_reference<T1, T2>::type, Ts...>
    //     {
    //     };
    // } // namespace detail

    // /// @brief Type trait to get the common reference type of a set of types.
    // /// @tparam ...T Types to get the common reference type of.
    // template <typename... T>
    // using common_reference_t = typename common_reference<T...>::type;

    /// @brief Type trait to get the underlying type of an enumeration.
    /// @tparam T Enumeration type to get the underlying type of.
    template <typename T>
        requires is_enum_v<T>
    struct underlying_type
    {
        using type = __underlying_type(T);
    };

    /// @brief Type trait to get the underlying type of an enumeration.
    /// @tparam T Enumeration type to get the underlying type of.
    template <typename T>
    using underlying_type_t = typename underlying_type<T>::type;

    template <typename T>
    struct make_signed
    {
    };

    template <>
    struct make_signed<unsigned char>
    {
        using type = signed char;
    };

    template <>
    struct make_signed<unsigned short>
    {
        using type = signed short;
    };

    template <>
    struct make_signed<unsigned int>
    {
        using type = signed int;
    };

    template <>
    struct make_signed<unsigned long>
    {
        using type = signed long;
    };

    template <>
    struct make_signed<unsigned long long>
    {
        using type = signed long long;
    };

    template <>
    struct make_signed<char>
    {
        using type = signed char;
    };

    template <>
    struct make_signed<char8_t>
    {
        using type = signed char;
    };

    template <>
    struct make_signed<wchar_t>
    {
        using type = signed short;
    };

    template <>
    struct make_signed<char16_t>
    {
        using type = signed short;
    };

    template <>
    struct make_signed<char32_t>
    {
        using type = signed int;
    };

    template <typename T>
        requires is_enum_v<T>
    struct make_signed<T>
    {
        using type = make_signed<underlying_type_t<T>>::type;
    };

    template <typename T>
    using make_signed_t = typename make_signed<T>::type;

    template <typename T>
    struct make_unsigned
    {
    };

    template <>
    struct make_unsigned<char>
    {
        using type = conditional_t<is_signed_v<char>, unsigned char, char>;
    };

    template <>
    struct make_unsigned<signed char>
    {
        using type = unsigned char;
    };

    template <>
    struct make_unsigned<signed short>
    {
        using type = unsigned short;
    };

    template <>
    struct make_unsigned<signed int>
    {
        using type = unsigned int;
    };

    template <>
    struct make_unsigned<signed long>
    {
        using type = unsigned long;
    };

    template <>
    struct make_unsigned<signed long long>
    {
        using type = unsigned long long;
    };

    template <>
    struct make_unsigned<char8_t>
    {
        using type = unsigned char;
    };

    template <>
    struct make_unsigned<wchar_t>
    {
        using type = unsigned short;
    };

    template <>
    struct make_unsigned<char16_t>
    {
        using type = unsigned short;
    };

    template <>
    struct make_unsigned<char32_t>
    {
        using type = unsigned int;
    };

    template <typename T>
        requires is_enum_v<T>
    struct make_unsigned<T>
    {
        using type = make_unsigned<underlying_type_t<T>>::type;
    };

    template <typename T>
    using make_unsigned_t = typename make_unsigned<T>::type;

    /// @brief Type trait to get the alignment of a type.
    /// @tparam T Type to get the alignment of.
    template <typename T>
    struct alignment_of : integral_constant<size_t, alignof(T)>
    {
    };

    /// @brief Type trait to get the alignment of a type.
    /// @tparam T Type to get the alignment of.
    template <typename T>
    inline constexpr size_t alignment_of_v = alignment_of<T>::value;

    /// @brief Type trait to get rank of a type.
    /// @tparam T Type to get the rank of.
    template <typename T>
    struct rank : integral_constant<size_t, 0>
    {
    };

    /// @brief Type trait to get rank of a type.
    /// @tparam T Type to get the rank of.
    template <typename T>
    struct rank<T[]> : integral_constant<size_t, rank<T>::value + 1>
    {
    };

    /// @brief Type trait to get rank of a type.
    /// @tparam T Type to get the rank of.
    template <typename T, size_t N>
    struct rank<T[N]> : integral_constant<size_t, rank<T>::value + 1>
    {
    };

    /// @brief Type trait to get rank of a type.
    /// @tparam T Type to get the rank of.
    template <typename T>
    inline constexpr size_t rank_v = rank<T>::value;

    /// @brief Type trait to get the extent of a type.
    /// @tparam T Type to get the extent of.
    /// @tparam I Index of the extent.
    template <typename T, unsigned int I = 0>
    struct extent : integral_constant<size_t, 0>
    {
    };

    /// @brief Type trait to get the extent of a type.
    /// @tparam T Type to get the extent of.
    /// @tparam N Size of the array.
    template <typename T, size_t N>
    struct extent<T[N], 0> : integral_constant<size_t, N>
    {
    };

    /// @brief Type trait to get the extent of a type.
    /// @tparam T Type to get the extent of.
    /// @tparam I Index of the extent.
    /// @tparam N Size of the array.
    template <typename T, unsigned int I, size_t N>
    struct extent<T[N], I> : integral_constant<size_t, extent<T, I - 1>::value>
    {
    };

    /// @brief Type trait to get the extent of a type.
    /// @tparam T Type to get the extent of.
    /// @tparam I Index of the extent.
    template <typename T, unsigned int I>
    struct extent<T[], I> : integral_constant<size_t, extent<T, I - 1>::value>
    {
    };

    /// @brief Type trait to get the extent of a type.
    /// @tparam T Type to get the extent of.
    /// @tparam I Index of the extent.
    template <typename T, unsigned int I>
    inline constexpr size_t extent_v = extent<T, I>::value;

    /// @brief Type trait to check if a function is being evaluated at compile-time.
    /// @return True if the function is being evaluated at compile-time, false otherwise.
    inline constexpr bool is_constant_evaluated() noexcept
    {
        return __builtin_is_constant_evaluated();
    }

    /// @brief Type trait to copy the reference of a type.
    /// @tparam T Type to copy the reference of.
    /// @tparam U Type to copy the reference to.
    template <typename T, typename U>
    struct copy_ref
    {
        using type = U;
    };

    /// @brief Type trait to copy the reference of a type.
    /// @tparam T Type to copy the reference of.
    /// @tparam U Type to copy the reference to.
    template <typename T, typename U>
    struct copy_ref<T&, U>
    {
        using type = U&;
    };

    /// @brief Type trait to copy the reference of a type.
    /// @tparam T Type to copy the reference of.
    /// @tparam U Type to copy the reference to.
    template <typename T, typename U>
    struct copy_ref<T&&, U>
    {
        using type = U&&;
    };

    /// @brief Type trait to copy the reference of a type.
    /// @tparam T Type to copy the reference of.
    /// @tparam U Type to copy the reference to.
    template <typename T, typename U>
    using copy_ref_t = typename copy_ref<T, U>::type;

    /// @brief Type trait to copy the constness of a type.
    /// @tparam T Type to copy the constness of.
    /// @tparam U Type to copy the constness to.
    template <typename T, typename U>
    struct copy_const
    {
        using type = U;
    };

    /// @brief Type trait to copy the constness of a type.
    /// @tparam T Type to copy the constness of.
    /// @tparam U Type to copy the constness to.
    template <typename T, typename U>
    struct copy_const<const T, U>
    {
        using type = const U;
    };

    /// @brief Type trait to copy the constness of a type.
    /// @tparam T Type to copy the constness of.
    /// @tparam U Type to copy the constness to.
    template <typename T, typename U>
    using copy_const_t = typename copy_const<T, U>::type;

    /// @brief Type trait to copy the volatility of a type.
    /// @tparam T Type to copy the volatility of.
    /// @tparam U Type to copy the volatility to.
    template <typename T, typename U>
    struct copy_volatile
    {
        using type = U;
    };

    /// @brief Type trait to copy the volatility of a type.
    /// @tparam T Type to copy the volatility of.
    /// @tparam U Type to copy the volatility to.
    template <typename T, typename U>
    struct copy_volatile<volatile T, U>
    {
        using type = volatile U;
    };

    /// @brief Type trait to copy the volatility of a type.
    /// @tparam T Type to copy the volatility of.
    /// @tparam U Type to copy the volatility to.
    template <typename T, typename U>
    using copy_volatile_t = typename copy_volatile<T, U>::type;

    /// @brief Type trait to copy the constness and volatility of a type.
    /// @tparam T Type to copy the constness and volatility of.
    /// @tparam U Type to copy the constness and volatility to.
    template <typename T, typename U>
    struct copy_cv
    {
        using type = copy_const_t<copy_volatile_t<T, U>, U>;
    };

    /// @brief Type trait to copy the constness and volatility of a type.
    /// @tparam T Type to copy the constness and volatility of.
    /// @tparam U Type to copy the constness and volatility to.
    template <typename T, typename U>
    using copy_cv_t = typename copy_cv<T, U>::type;

    /// @brief Type trait to copy the constness, volatility and reference of a type.
    /// @tparam T Type to copy the constness, volatility and reference of.
    /// @tparam U Type to copy the constness, volatility and reference to.
    template <typename T, typename U>
    struct copy_cvref
    {
        using type = copy_ref_t<copy_cv<T, U>, U>;
    };

    /// @brief Type trait to copy the constness, volatility and reference of a type.
    /// @tparam T Type to copy the constness, volatility and reference of.
    /// @tparam U Type to copy the constness, volatility and reference to.
    template <typename T, typename U>
    using copy_cvref_t = typename copy_cvref<T, U>::type;
} // namespace tempest

#endif // tempest_core_type_traits_hpp