#ifndef tempest_core_functional_hpp
#define tempest_core_functional_hpp

namespace tempest
{
    template <typename T = void>
    struct equal_to;

    template <typename T = void>
    struct not_equal_to;

    template <typename T = void>
    struct greater;

    template <typename T = void>
    struct less;

    template <typename T = void>
    struct greater_equal;

    template <typename T = void>
    struct less_equal;

    template <typename T>
    struct equal_to
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs == rhs;
        }
    };

    template <typename T>
    struct not_equal_to
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs != rhs;
        }
    };

    template <typename T>
    struct greater
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs > rhs;
        }
    };

    template <typename T>
    struct less
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs < rhs;
        }
    };

    template <typename T>
    struct greater_equal
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs >= rhs;
        }
    };

    template <typename T>
    struct less_equal
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs <= rhs;
        }
    };
} // namespace tempest

#endif // tempest_core_functional_hpp