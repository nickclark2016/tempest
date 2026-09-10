#ifndef tempest_core_comparators_hpp
#define tempest_core_comparators_hpp

#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    template <typename T = void>
    struct equal_to
    {
        constexpr auto operator()(const T& lhs, const T& rhs) const -> bool
        {
            return lhs == rhs;
        }
    };

    template <>
    struct equal_to<void>
    {
        template <typename T, typename U>
        constexpr auto operator()(T&& lhs, U&& rhs) const
        {
            if constexpr (is_same_v<decay_t<T>, decay_t<U>>)
            {
                return equal_to<decay_t<T>>{}(tempest::forward<T>(lhs), tempest::forward<U>(rhs));
            }
            else
            {
                return tempest::forward<T>(lhs) == tempest::forward<U>(rhs);
            }
        }

        using is_transparent = void;
    };

    template <typename T = void>
    struct not_equal_to
    {
        constexpr auto operator()(const T& lhs, const T& rhs) const -> bool
        {
            return lhs != rhs;
        }
    };

    template <>
    struct not_equal_to<void>
    {
        template <typename T, typename U>
        constexpr auto operator()(T&& lhs, U&& rhs) const
        {
            if constexpr (is_same_v<decay_t<T>, decay_t<U>>)
            {
                return not_equal_to<decay_t<T>>{}(tempest::forward<T>(lhs), tempest::forward<U>(rhs));
            }
            else
            {
                return tempest::forward<T>(lhs) != tempest::forward<U>(rhs);
            }
        }

        using is_transparent = void;
    };

    template <typename T = void>
    struct greater
    {
        constexpr auto operator()(const T& lhs, const T& rhs) const -> bool
        {
            return lhs > rhs;
        }
    };

    template <>
    struct greater<void>
    {
        template <typename T, typename U>
        constexpr auto operator()(T&& lhs, U&& rhs) const
        {
            if constexpr (is_same_v<decay_t<T>, decay_t<U>>)
            {
                return greater<decay_t<T>>{}(tempest::forward<T>(lhs), tempest::forward<U>(rhs));
            }
            else
            {
                return tempest::forward<T>(lhs) > tempest::forward<U>(rhs);
            }
        }

        using is_transparent = void;
    };

    template <typename T = void>
    struct less
    {
        constexpr auto operator()(const T& lhs, const T& rhs) const -> bool
        {
            return lhs < rhs;
        }
    };

    template <>
    struct less<void>
    {
        template <typename T, typename U>
        constexpr auto operator()(T&& lhs, U&& rhs) const
        {
            if constexpr (is_same_v<decay_t<T>, decay_t<U>>)
            {
                return less<decay_t<T>>{}(tempest::forward<T>(lhs), tempest::forward<U>(rhs));
            }
            else
            {
                return tempest::forward<T>(lhs) < tempest::forward<U>(rhs);
            }
        }

        using is_transparent = void;
    };

    template <typename T = void>
    struct greater_equal
    {
        constexpr auto operator()(const T& lhs, const T& rhs) const -> bool
        {
            return lhs >= rhs;
        }
    };

    template <>
    struct greater_equal<void>
    {
        template <typename T, typename U>
        constexpr auto operator()(T&& lhs, U&& rhs) const
        {
            if constexpr (is_same_v<decay_t<T>, decay_t<U>>)
            {
                return greater_equal<decay_t<T>>{}(tempest::forward<T>(lhs), tempest::forward<U>(rhs));
            }
            else
            {
                return tempest::forward<T>(lhs) >= tempest::forward<U>(rhs);
            }
        }

        using is_transparent = void;
    };

    template <typename T = void>
    struct less_equal
    {
        constexpr auto operator()(const T& lhs, const T& rhs) const -> bool
        {
            return lhs <= rhs;
        }
    };

    template <>
    struct less_equal<void>
    {
        template <typename T, typename U>
        constexpr auto operator()(T&& lhs, U&& rhs) const
        {
            if constexpr (is_same_v<decay_t<T>, decay_t<U>>)
            {
                return less_equal<decay_t<T>>{}(tempest::forward<T>(lhs), tempest::forward<U>(rhs));
            }
            else
            {
                return tempest::forward<T>(lhs) <= tempest::forward<U>(rhs);
            }
        }

        using is_transparent = void;
    };
} // namespace tempest

#endif // tempest_core_comparators_hpp
