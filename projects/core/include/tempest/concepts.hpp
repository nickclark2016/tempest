#ifndef tempest_core_concepts_hpp
#define tempest_core_concepts_hpp

#include <tempest/type_traits.hpp>

namespace tempest
{
    /// @brief Concept for types that are integral.
    /// @tparam T Type to check
    template <typename T>
    concept integral = is_integral_v<T>;

    /// @brief Concept for types that are destructible.
    /// @tparam T Type to check
    template <typename T>
    concept destructible = is_nothrow_destructible_v<T>;

    /// @brief Concept for types that are constructible given a set of argument types.
    /// @tparam T Type to check
    /// @tparam Args Argument types
    template <typename T, typename... Args>
    concept constructible_from = destructible<T> && is_constructible_v<T, Args...>;

    /// @brief Concept for types that can be converted to another type.
    /// @tparam From Type to convert from
    /// @tparam To Type to convert to
    template <typename From, typename To>
    concept convertible_to = is_convertible_v<From, To> && requires { static_cast<To>(declval<From>()); };

    /// @brief Concept for types that can be moved constructed.
    /// @tparam T Type to check
    template <typename T>
    concept move_constructible = constructible_from<T, T> && convertible_to<T, T>;

    /// @brief Concept for types that are the same.
    /// @tparam T Type to check
    /// @tparam U Type to check against
    template <typename T, typename U>
    concept same_as = is_same_v<T, U> && is_same_v<U, T>;

    /// @brief Concept for types that have a common reference type.
    /// @tparam T Type to check
    /// @tparam U Type to check against
    template <typename T, typename U>
    concept common_reference_with =
        same_as<common_reference_t<T, U>, common_reference_t<U, T>> && convertible_to<T, common_reference_t<T, U>> &&
        convertible_to<U, common_reference_t<T, U>>;

    /// @brief Checks if a type is assignable from.
    /// @tparam L Type to assign to
    /// @tparam R Type to assign from
    template <typename L, typename R>
    concept assignable_from =
        is_lvalue_reference_v<L> && common_reference_with<const remove_reference_t<L>&, remove_reference_t<R>&> &&
        requires(L l, R&& r) {
            { l = forward<R>(r) } -> same_as<L>;
        };

    /// @brief Concept for types that can be copied constructed.
    /// @tparam T Type to check
    template <typename T>
    concept copy_constructible = move_constructible<T> && constructible_from<T, T&> && convertible_to<T&, T> &&
                                 constructible_from<T, const T&> && convertible_to<const T&, T> &&
                                 constructible_from<T, const T> && convertible_to<const T, T>;

    /// @brief Concept for swappable types.
    /// @tparam T Type to check
    template <typename T>
    concept swappable = is_swappable_v<T>;

    /// @brief Concept for movable types.
    /// @tparam T Type to check
    template <typename T>
    concept movable = is_object_v<T> && move_constructible<T> && assignable_from<T&, T> && swappable<T>;

    /// @brief Concept for copyable types.
    /// @tparam T Type to check
    template <typename T>
    concept copyable = copy_constructible<T> && movable<T> && assignable_from<T&, T&> &&
                       assignable_from<T&, const T&> && assignable_from<T&, const T>;

    /// @brief Concept for default initializable types.
    /// @tparam T Type to check
    template <typename T>
    concept default_initializable = constructible_from<T> && requires {
        T{};     // Makes sure value initialization, direct-list initialization is well formed.
        ::new T; // Makes sure T default initializable is well formed.
    };

    /// @brief Concept for semiregular types.
    /// @tparam T Type to check
    template <typename T>
    concept semiregular = copyable<T> && default_initializable<T>;

    namespace detail
    {
        template <typename T>
        using with_reference = T&;

        template <typename T>
        concept can_reference = requires(T t) { typename with_reference<T>; };
    }; // namespace detail

    template <typename T>
    concept dereferenceable = requires(T& t) {
        { *t } -> detail::can_reference;
    };

    namespace detail
    {
        template <typename T>
        concept boolean_testable = requires(T&& t) {
            { !static_cast<T&&>(t) } -> convertible_to<bool>;
        } && convertible_to<T, bool>;

        template <typename T, typename U>
        concept weakly_eq_cmp_with = requires(const remove_reference_t<T>& t, const remove_reference_t<U>& u) {
            { t == u } -> boolean_testable;
            { t != u } -> boolean_testable;
            { u == t } -> boolean_testable;
            { u != t } -> boolean_testable;
        };
    } // namespace detail

    template <typename T>
    concept equality_comparable = detail::weakly_eq_cmp_with<T, T>;

    template <typename T, typename U>
    concept equality_comparable_with =
        equality_comparable<T> && equality_comparable<U> &&
        common_reference_with<const remove_reference_t<T>&, const remove_reference_t<U>&> &&
        equality_comparable<common_reference_t<const remove_reference_t<T>&, const remove_reference_t<U>&>> &&
        detail::weakly_eq_cmp_with<T, U>;

    template <typename T, typename U>
    concept partially_ordered_with = requires(const remove_reference_t<T>& t, const remove_reference_t<U>& u) {
        { t < u } -> detail::boolean_testable;
        { t > u } -> detail::boolean_testable;
        { t <= u } -> detail::boolean_testable;
        { t >= u } -> detail::boolean_testable;
        { u < t } -> detail::boolean_testable;
        { u > t } -> detail::boolean_testable;
        { u <= t } -> detail::boolean_testable;
        { u >= t } -> detail::boolean_testable;
    };

    template <typename T>
    concept totally_ordered = equality_comparable<T> && partially_ordered_with<T, T>;

    template <typename T, typename U>
    concept totally_ordered_with =
        totally_ordered<T> && totally_ordered<U> && equality_comparable_with<T, U> &&
        totally_ordered<common_reference_t<const remove_reference_t<T>&, const remove_reference_t<U>&>> &&
        partially_ordered_with<T, U>;

    template <typename T>
    concept signed_integral = integral<T> && is_signed_v<T>;

    template <typename T>
    concept unsigned_integral = integral<T> && !signed_integral<T>;

    template <typename T, typename U>
    concept derived_from = is_base_of_v<U, T>;

    template <typename F, typename... Args>
    concept invocable = requires(F&& f, Args&&... args) { invoke(forward<F>(f), forward<Args>(args)...); };

    template <typename F, typename... Args>
    concept regular_invocable = invocable<F, Args...>;
} // namespace tempest

#endif // tempest_core_concepts_hpp