#ifndef tempest_core_span_hpp
#define tempest_core_span_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/assert.hpp>
#include <tempest/int.hpp>
#include <tempest/iterator.hpp>
#include <tempest/memory.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    inline constexpr size_t dynamic_extent = static_cast<size_t>(-1);

    template <typename T, size_t Extent = dynamic_extent>
    class span
    {
      public:
        using element_type = T;
        using value_type = remove_cv_t<T>;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = tempest::reverse_iterator<iterator>;
        using const_reverse_iterator = tempest::reverse_iterator<const_iterator>;

        static constexpr size_t extent = Extent;

        constexpr span() noexcept;

        template <typename It>
        explicit(extent != dynamic_extent) constexpr span(It start, size_type count);

        template <typename It, typename End>
            requires(!is_convertible_v<End, size_t>)
        explicit(extent != dynamic_extent) constexpr span(It start, End end);

        template <size_t N>
        constexpr span(T (&arr)[N]) noexcept;

        template <typename U, size_t N>
        constexpr span(array<U, N>& arr) noexcept;

        template <typename U, size_t N>
        constexpr span(const array<U, N>& arr) noexcept;

        template <typename U, size_t N>
        explicit(extent != dynamic_extent && N == dynamic_extent) constexpr span(const span<U, N>& other) noexcept;

        template <typename R>
        constexpr span(R&& r);

        constexpr span(const span& other) noexcept = default;
        constexpr span(span&& other) noexcept = default;
        constexpr ~span() = default;

        constexpr auto operator=(const span& other) noexcept -> span& = default;
        constexpr auto operator=(span&& other) noexcept -> span& = default;

        [[nodiscard]] constexpr auto begin() const noexcept -> iterator;
        [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator;

        [[nodiscard]] constexpr auto end() const noexcept -> iterator;
        [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator;

        [[nodiscard]] constexpr auto rbegin() const noexcept -> reverse_iterator;
        [[nodiscard]] constexpr auto crbegin() const noexcept -> const_reverse_iterator;

        [[nodiscard]] constexpr auto rend() const noexcept -> reverse_iterator;
        [[nodiscard]] constexpr auto crend() const noexcept -> const_reverse_iterator;

        constexpr auto operator[](size_type idx) const noexcept -> reference;
        [[nodiscard]] constexpr auto at(size_type idx) const -> reference;

        [[nodiscard]] constexpr auto front() const noexcept -> reference;
        [[nodiscard]] constexpr auto back() const noexcept -> reference;

        [[nodiscard]] constexpr auto data() const noexcept -> pointer;

        [[nodiscard]] constexpr auto size() const noexcept -> size_type;
        [[nodiscard]] constexpr auto size_bytes() const noexcept -> size_type;

        [[nodiscard]] constexpr auto empty() const noexcept -> bool;

        [[nodiscard]] constexpr auto first(size_type count) const -> span<T, dynamic_extent>;

        template <size_t Count>
        constexpr auto first() const -> span<T, Count>;

        [[nodiscard]] constexpr auto last(size_type count) const -> span<T, dynamic_extent>;

        template <size_t Count>
        constexpr auto last() const -> span<T, Count>;

        template <size_t Offset, size_t Count = dynamic_extent>
        [[nodiscard]] constexpr auto subspan() const -> span<T, dynamic_extent>;

        [[nodiscard]] constexpr auto subspan(size_type offset, size_type count = dynamic_extent) const -> span<T, dynamic_extent>;

      private:
        T* _start{};
        T* _end{};
    };

    template <typename T, size_t N>
    span(T (&)[N]) -> span<T, N>;

    template <typename T, size_t N>
    span(array<T, N>&) -> span<T, N>;

    template <typename T, size_t N>
    span(const array<T, N>&) -> span<const T, N>;

    template <typename It, typename EndOrSize>
    span(It, EndOrSize) -> span<remove_reference_t<iter_reference_t<It>>>;

    template <typename T>
    inline auto as_bytes(span<const T> s) noexcept -> span<const byte>
    {
        return {reinterpret_cast<const byte*>(s.data()), s.size_bytes()};
    }

    template <typename T>
    inline auto as_writeable_bytes(span<T> s) noexcept -> span<byte>
    {
        return {reinterpret_cast<byte*>(s.data()), s.size_bytes()};
    }

    template <typename T, size_t Extent>
    constexpr span<T, Extent>::span() noexcept  
    {
        static_assert(Extent == 0 || Extent == dynamic_extent, "Extent must be 0 or dynamic.");
    }

    template <typename T, size_t Extent>
    template <typename It>
    constexpr span<T, Extent>::span(It start, size_type count) : _start{start}, _end{start + count}
    {
    }

    template <typename T, size_t Extent>
    template <typename It, typename End>
        requires(!is_convertible_v<End, size_t>)
    constexpr span<T, Extent>::span(It start, End end)
        : _start{tempest::to_address(start)}, _end{_start + (end - start)}
    {
    }

    template <typename T, size_t Extent>
    template <size_t N>
    constexpr span<T, Extent>::span(T (&arr)[N]) noexcept
        : _start{tempest::to_address(arr)}, _end{tempest::to_address(arr) + N}
    {
    }

    namespace detail
    {
        template <typename T>
        concept has_begin_pointer = requires(T t) {
            { t.begin() } -> pointer;
        };

        template <typename T>
        inline auto get_begin_ptr(T&& t) noexcept
        {
            if constexpr (has_begin_pointer<T>)
            {
                return t.begin();
            }
            else
            {
                return tempest::addressof(*begin(t));
            }
        }
    } // namespace detail

    template <typename T, size_t Extent>
    template <typename R>
    constexpr span<T, Extent>::span(R&& r)
        : _start{detail::get_begin_ptr(r)}, _end{_start + (tempest::distance(::tempest::begin(r), ::tempest::end(r)))}
    {
    }

    template <typename T, size_t Extent>
    template <typename U, size_t N>
    constexpr span<T, Extent>::span(array<U, N>& arr) noexcept : _start{arr.data()}, _end{arr.data() + N}
    {
    }

    template <typename T, size_t Extent>
    template <typename U, size_t N>
    constexpr span<T, Extent>::span(const array<U, N>& arr) noexcept : _start{arr.data()}, _end{arr.data() + N}
    {
    }

    template <typename T, size_t Extent>
    template <typename U, size_t N>
    constexpr span<T, Extent>::span(const span<U, N>& other) noexcept
        : _start{other.data()}, _end{other.data() + other.size()}
    {
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::begin() const noexcept -> span<T, Extent>::iterator
    {
        return _start;
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::cbegin() const noexcept -> span<T, Extent>::const_iterator
    {
        return _start;
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::end() const noexcept -> span<T, Extent>::iterator
    {
        return _end;
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::cend() const noexcept -> span<T, Extent>::const_iterator
    {
        return _end;
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::rbegin() const noexcept -> span<T, Extent>::reverse_iterator
    {
        return reverse_iterator{_end};
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::crbegin() const noexcept -> span<T, Extent>::const_reverse_iterator
    {
        return const_reverse_iterator{_end};
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::rend() const noexcept -> span<T, Extent>::reverse_iterator
    {
        return reverse_iterator{_start};
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::crend() const noexcept -> span<T, Extent>::const_reverse_iterator
    {
        return const_reverse_iterator{_start};
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::operator[](size_type idx) const noexcept -> span<T, Extent>::reference
    {
        return _start[idx];
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::at(size_type idx) const -> span<T, Extent>::reference
    {
        assert(idx < size());

        return _start[idx];
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::front() const noexcept -> span<T, Extent>::reference
    {
        return *_start;
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::back() const noexcept -> span<T, Extent>::reference
    {
        return *(_end - 1);
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::data() const noexcept -> span<T, Extent>::pointer
    {
        return _start;
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::size() const noexcept -> span<T, Extent>::size_type
    {
        return _end - _start;
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::size_bytes() const noexcept -> span<T, Extent>::size_type
    {
        return size() * sizeof(T);
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::empty() const noexcept -> bool
    {
        return _start == _end;
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::first(size_type count) const -> span<T, dynamic_extent>
    {
        return {_start, _start + count};
    }

    template <typename T, size_t Extent>
    template <size_t Count>
    constexpr auto span<T, Extent>::first() const -> span<T, Count>
    {
        static_assert(Count <= Extent, "Count must be less than or equal to the extent of the span.");

        return span{_start, _start + Count};
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::last(size_type count) const -> span<T, dynamic_extent>
    {
        return {_end - count, _end};
    }

    template <typename T, size_t Extent>
    template <size_t Count>
    constexpr auto span<T, Extent>::last() const -> span<T, Count>
    {
        static_assert(Count <= Extent, "Count must be less than or equal to the extent of the span.");

        return span{_end - Count, _end};
    }

    template <typename T, size_t Extent>
    template <size_t Offset, size_t Count>
    constexpr auto span<T, Extent>::subspan() const -> span<T, dynamic_extent>
    {
        static_assert(Offset + Count <= Extent, "Offset + Count must be less than or equal to the extent of the span.");

        if constexpr (Count == dynamic_extent)
        {
            return {_start + Offset, _end};
        }
        else
        {
            return {_start + Offset, _start + Offset + Count};
        }
    }

    template <typename T, size_t Extent>
    constexpr auto span<T, Extent>::subspan(size_type offset, size_type count) const -> span<T, dynamic_extent>
    {
        TEMPEST_ASSERT(offset + count <= size());

        if (count == dynamic_extent)
        {
            return {_start + offset, _end};
        }

        return {_start + offset, _start + offset + count};
    }
} // namespace tempest

#endif // tempest_core_span_hpp