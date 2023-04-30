#ifndef tempest_core_algorithm_hpp
#define tempest_core_algorithm_hpp

#include <iterator>
#include <memory>
#include <type_traits>

namespace tempest::core
{
    template <typename InputIt, typename OutputIt>
    inline constexpr OutputIt copy_construct(InputIt first, InputIt last, OutputIt d_first)
    {
        using src_traits = std::iterator_traits<InputIt>;
        using dst_traits = std::iterator_traits<OutputIt>;

        for (; first != last; (void)++first, (void)++d_first)
        {
            typename dst_traits::pointer ptr = std::addressof(*d_first);
            std::construct_at(ptr, *first);
        }

        return d_first;
    }

    template <typename InputIt, typename OutputIt>
    inline constexpr OutputIt move_construct(InputIt first, InputIt last, OutputIt d_first)
    {
        using src_traits = std::iterator_traits<InputIt>;
        using dst_traits = std::iterator_traits<OutputIt>;

        for (; first != last; (void)++first, (void)++d_first)
        {
            typename dst_traits::pointer ptr = std::addressof(*d_first);
            std::construct_at(ptr, std::move(*first));
        }

        return d_first;
    }

    template <typename InputIt, typename OutputIt>
    inline constexpr OutputIt optimal_construct(InputIt first, InputIt last, OutputIt d_first)
    {
        if constexpr (std::is_move_constructible_v<typename std::iterator_traits<OutputIt>::value_type>)
        {
            return move_construct(first, last, d_first);
        }
        else
        {
            return copy_construct(first, last, d_first);
        }
    }
} // namespace tempest::core

#endif // tempest_core_algorithm_hpp