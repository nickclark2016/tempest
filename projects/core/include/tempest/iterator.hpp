#ifndef tempest_core_iterator_hpp
#define tempest_core_iterator_hpp

#include <tempest/concepts.hpp>

namespace tempest
{
    template <typename It>
    struct incrementable_traits
    {
    };

    template <typename It>
        requires is_object_v<It>
    struct incrementable_traits<It*>
    {
        using difference_type = ptrdiff_t;
    };

    template <typename It>
    struct incrementable_traits<const It> : incrementable_traits<It>
    {
    };

    template <typename It>
        requires requires { typename It::difference_type; }
    struct incrementable_traits<It>
    {
        using difference_type = typename It::difference_type;
    };

    template <typename It>
        requires(!requires { typename It::difference_type; }) && requires(const It& a, const It& b) {
            { b - a } -> integral;
        }
    struct incrementable_traits<It>
    {
        using difference_type = make_signed_t<decltype(declval<It>() - declval<It>())>;
    };

    template <typename T>
    using iter_difference_t = typename incrementable_traits<T>::difference_type;

    template <typename It>
    struct indirectly_readable_traits {};

    template <typename It>
        requires is_object_v<It>
    struct indirectly_readable_traits<It*>
    {
        using value_type = remove_cv_t<It>;
    };

    template <typename It>
        requires is_array_v<It>
    struct indirectly_readable_traits<It>
    {
        using value_type = remove_cv_t<remove_extent_t<It>>;
    };

    template <typename It>
    struct indirectly_readable_traits<const It> : indirectly_readable_traits<It>
    {
    };

    template <typename It>
        requires requires { typename It::value_type; }
    struct indirectly_readable_traits<It>
    {
        using value_type = typename It::value_type;
    };

    template <typename It>
        requires requires { typename It::element_type; }
    struct indirectly_readable_traits<It>
    {
        using value_type = remove_cv_t<typename It::element_type>;
    };

    template <typename It>
        requires requires {
            typename It::value_type;
            typename It::element_type;
        } && same_as<remove_cv_t<typename It::value_type>, remove_cv_t<typename It::element_type>>
    struct indirectly_readable_traits<It>
    {
        using value_type = remove_cv_t<typename It::value_type>;
    };

    template <typename It>
        requires requires {
            typename It::value_type;
            typename It::element_type;
        }
    struct indirectly_readable_traits<It>
    {
    };

    template <typename T>
    concept indirectly_readable = requires(T t) { typename indirectly_readable_traits<T>::value_type; };

    template <indirectly_readable It>
    using iter_reference_t = decltype(*declval<It&>());

    template <indirectly_readable It>
    using iter_value_t = remove_cvref_t<iter_reference_t<It>>;

    template <typename O, typename T>
    concept indirectly_writable = requires(O&& o, T&& t) {
        *o = tempest::forward<T>(t);
        *tempest::forward<O>(o) = tempest::forward<T>(t);
        const_cast<iter_reference_t<O>&&>(*o) = tempest::forward<T>(t);
        const_cast<iter_reference_t<O>&&>(*tempest::forward<O>(o)) = tempest::forward<T>(t);
    };

    template <typename It>
    concept weakly_iteratable = movable<It> && requires(It it) {
        typename iter_difference_t<It>;
        requires signed_integral<iter_difference_t<It>>;
        { ++it } -> same_as<It&>;
        it++;
    };

    template <typename It>
    concept iterator = requires(It it) {
        { *it } -> detail::can_reference;
        { ++it } -> same_as<It&>;
        { *it++ } -> detail::can_reference;
    } && copyable<It>;

    template <typename It>
    concept input_or_output_iterator = requires(It it) {
        { *it } -> detail::can_reference;
    } && weakly_iteratable<It>;

    template <typename It>
    concept input_iterator = iterator<It> && equality_comparable<It> && requires(It it) {
        typename incrementable_traits<It>::difference_type;
        typename indirectly_readable_traits<It>::value_type;
        typename common_reference_t<iter_reference_t<It>&&, typename indirectly_readable_traits<It>::value_type&>;
        *it++;
        typename common_reference_t<decltype(*++it)&&, iter_reference_t<It>&>;

        requires signed_integral<typename incrementable_traits<It>::difference_type>;
    };

    template <typename It, typename T>
    concept output_iterator = input_or_output_iterator<It> && indirectly_writable<It, T> &&
                              requires(It it, T&& t) { *it++ = tempest::forward<T>(t); };

    template <typename It>
    concept forward_iterator =
        input_iterator<It> && constructible_from<It> && is_reference_v<iter_reference_t<It>> &&
        same_as<remove_cvref_t<iter_reference_t<It>>, typename indirectly_readable_traits<It>::value_type> &&
        requires(It it) {
            { it++ } -> convertible_to<const It&>;
            { *it++ } -> same_as<iter_reference_t<It>>;
        };

    template <typename It>
    concept bidirectional_iterator = forward_iterator<It> && requires(It it) {
        { --it } -> same_as<It&>;
        { it-- } -> convertible_to<const It&>;
        { *it-- } -> same_as<iter_reference_t<It>>;
    };

    template <typename It>
    concept random_access_iterator = bidirectional_iterator<It> && totally_ordered<It> &&
                                     requires(It it, typename incrementable_traits<It>::difference_type n) {
                                         { it += n } -> same_as<It&>;
                                         { it -= n } -> same_as<It&>;
                                         { it + n } -> same_as<It>;
                                         { n + it } -> same_as<It>;
                                         { it - n } -> same_as<It>;
                                         { it - it } -> same_as<decltype(n)>;
                                         { it[n] } -> same_as<iter_reference_t<It>>;
                                     };

    template <typename It>
    concept contiguous_iterator =
        random_access_iterator<It> && is_lvalue_reference_v<iter_reference_t<It>> &&
        same_as<iter_value_t<It>, remove_cvref_t<iter_reference_t<It>>> && requires(const It& it) {
            { &*it } -> same_as<add_pointer_t<iter_reference_t<It>>>;
        };

    template <typename S, typename I>
    concept sentinel_for = semiregular<S> && input_or_output_iterator<I> && detail::weakly_eq_cmp_with<S, I>;

    namespace detail
    {
        template <typename T>
        concept has_pointer_type_test = requires { typename T::pointer; };

        template <typename T>
        concept has_arrow_operator_test = requires { declval<T&>().operator->(); };

        template <typename T>
        concept has_reference_test = requires { typename T::reference; };

        template <typename T, bool, bool>
        struct pointer_type_impl;

        template <typename T>
        struct pointer_type_impl<T, true, false>
        {
            using type = typename T::pointer;
        };

        template <typename T>
        struct pointer_type_impl<T, false, true>
        {
            using type = decltype(declval<T&>().operator->());
        };

        template <typename T>
        struct pointer_type_impl<T, true, true>
        {
            using type = typename T::pointer;
        };

        template <typename T>
        struct pointer_type_impl<T, false, false>
        {
            using type = void;
        };

        template <typename T>
        using pointer_type = typename pointer_type_impl<T, has_pointer_type_test<T>, has_arrow_operator_test<T>>::type;

        template <typename T, bool>
        struct reference_type_impl;

        template <typename T>
        struct reference_type_impl<T, true>
        {
            using type = typename T::reference;
        };

        template <typename T>
        struct reference_type_impl<T, false>
        {
            using type = iter_reference_t<T>;
        };

        template <typename T>
        using reference_type = typename reference_type_impl<T, has_reference_test<T>>::type;
    } // namespace detail

    template <typename It>
    struct iterator_traits
    {
        using difference_type = typename incrementable_traits<It>::difference_type;
        using value_type = typename indirectly_readable_traits<It>::value_type;
        using pointer = typename detail::pointer_type<It>;
        using reference = typename detail::reference_type<It>;
    };

    template <input_iterator It, typename D>
    inline constexpr void advance(It& it, D n)
    {
        if constexpr (contiguous_iterator<It>)
        {
            it += n;
        }
        else
        {
            while (n > 0)
            {
                ++it;
                --n;
            }

            if constexpr (random_access_iterator<It>)
            {
                while (n < 0)
                {
                    --it;
                    ++n;
                }
            }
        }
    }

    template <input_iterator It>
    inline constexpr It next(It it, typename iterator_traits<It>::difference_type n = 1)
    {
        advance(it, n);
        return it;
    }

    template <bidirectional_iterator It>
    inline constexpr It prev(It it, typename iterator_traits<It>::difference_type n = 1)
    {
        advance(it, -n);
        return it;
    }

    template <input_iterator It>
    inline constexpr iter_difference_t<It> distance(It first, It last)
    {
        if constexpr (contiguous_iterator<It>)
        {
            return last - first;
        }
        else
        {
            iter_difference_t<It> n = 0;

            while (first != last)
            {
                ++first;
                ++n;
            }

            return n;
        }
    }
} // namespace tempest

#endif // tempest_core_iterator_hpp