#ifndef tempest_core_span_hpp
#define tempest_core_span_hpp

#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/int.hpp>
#include <tempest/type_traits.hpp>

#include <cassert>
#include <iterator>

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
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

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

        constexpr span& operator=(const span& other) noexcept = default;
        constexpr span& operator=(span&& other) noexcept = default;

        constexpr iterator begin() const noexcept;
        constexpr const_iterator cbegin() const noexcept;

        constexpr iterator end() const noexcept;
        constexpr const_iterator cend() const noexcept;

        constexpr reverse_iterator rbegin() const noexcept;
        constexpr const_reverse_iterator crbegin() const noexcept;

        constexpr reverse_iterator rend() const noexcept;
        constexpr const_reverse_iterator crend() const noexcept;

        constexpr reference operator[](size_type idx) const noexcept;
        constexpr reference at(size_type idx) const;

        constexpr reference front() const noexcept;
        constexpr reference back() const noexcept;

        constexpr pointer data() const noexcept;

        constexpr size_type size() const noexcept;
        constexpr size_type size_bytes() const noexcept;

        constexpr bool empty() const noexcept;

        constexpr span<T, dynamic_extent> first(size_type count) const;

        template <size_t Count>
        constexpr span<T, Count> first() const;

        constexpr span<T, dynamic_extent> last(size_type count) const;

        template <size_t Count>
        constexpr span<T, Count> last() const;

        template <size_t Offset, size_t Count = dynamic_extent>
        constexpr span<T, dynamic_extent> subspan() const;

        constexpr span<T, dynamic_extent> subspan(size_type offset, size_type count = dynamic_extent) const;

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
    inline span<const byte> as_bytes(span<const T> s) noexcept
    {
        return {reinterpret_cast<const byte*>(s.data()), s.size_bytes()};
    }

    template <typename T>
    inline span<byte> as_writeable_bytes(span<T> s) noexcept
    {
        return {reinterpret_cast<byte*>(s.data()), s.size_bytes()};
    }

    template <typename T, size_t Extent>
    inline constexpr span<T, Extent>::span() noexcept : _start{nullptr}, _end{nullptr}
    {
        static_assert(Extent == 0 || Extent == dynamic_extent, "Extent must be 0 or dynamic.");
    }

    template <typename T, size_t Extent>
    template <typename It>
    inline constexpr span<T, Extent>::span(It start, size_type count) : _start{start}, _end{start + count}
    {
    }

    template <typename T, size_t Extent>
    template <typename It, typename End>
        requires(!is_convertible_v<End, size_t>)
    inline constexpr span<T, Extent>::span(It start, End end)
        : _start{std::to_address(start)}, _end{_start + (end - start)}
    {
    }

    template <typename T, size_t Extent>
    template <size_t N>
    inline constexpr span<T, Extent>::span(T (&arr)[N]) noexcept
        : _start{std::to_address(arr)}, _end{std::to_address(arr) + N}
    {
    }

    template <typename T, size_t Extent>
    template <typename R>
    inline constexpr span<T, Extent>::span(R&& r)
        : _start{std::addressof(*::tempest::begin(r))},
          _end{_start + (std::distance(::tempest::begin(r), ::tempest::end(r)))}
    {
    }

    template <typename T, size_t Extent>
    template <typename U, size_t N>
    inline constexpr span<T, Extent>::span(array<U, N>& arr) noexcept : _start{arr.data()}, _end{arr.data() + N}
    {
    }

    template <typename T, size_t Extent>
    template <typename U, size_t N>
    inline constexpr span<T, Extent>::span(const array<U, N>& arr) noexcept
        : _start{arr.data()}, _end{arr.data() + N}
    {
    }

    template <typename T, size_t Extent>
    template <typename U, size_t N>
    inline constexpr span<T, Extent>::span(const span<U, N>& other) noexcept
        : _start{other.data()}, _end{other.data() + other.size()}
    {
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::iterator span<T, Extent>::begin() const noexcept
    {
        return _start;
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::const_iterator span<T, Extent>::cbegin() const noexcept
    {
        return _start;
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::iterator span<T, Extent>::end() const noexcept
    {
        return _end;
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::const_iterator span<T, Extent>::cend() const noexcept
    {
        return _end;
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::reverse_iterator span<T, Extent>::rbegin() const noexcept
    {
        return reverse_iterator{_end};
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::const_reverse_iterator span<T, Extent>::crbegin() const noexcept
    {
        return const_reverse_iterator{_end};
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::reverse_iterator span<T, Extent>::rend() const noexcept
    {
        return reverse_iterator{_start};
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::const_reverse_iterator span<T, Extent>::crend() const noexcept
    {
        return const_reverse_iterator{_start};
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::reference span<T, Extent>::operator[](size_type idx) const noexcept
    {
        return _start[idx];
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::reference span<T, Extent>::at(size_type idx) const
    {
        assert(idx < size());

        return _start[idx];
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::reference span<T, Extent>::front() const noexcept
    {
        return *_start;
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::reference span<T, Extent>::back() const noexcept
    {
        return *(_end - 1);
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::pointer span<T, Extent>::data() const noexcept
    {
        return _start;
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::size_type span<T, Extent>::size() const noexcept
    {
        return _end - _start;
    }

    template <typename T, size_t Extent>
    inline constexpr typename span<T, Extent>::size_type span<T, Extent>::size_bytes() const noexcept
    {
        return size() * sizeof(T);
    }

    template <typename T, size_t Extent>
    inline constexpr bool span<T, Extent>::empty() const noexcept
    {
        return _start == _end;
    }

    template <typename T, size_t Extent>
    inline constexpr span<T, dynamic_extent> span<T, Extent>::first(size_type count) const
    {
        return {_start, _start + count};
    }

    template <typename T, size_t Extent>
    template <size_t Count>
    inline constexpr span<T, Count> span<T, Extent>::first() const
    {
        static_assert(Count <= Extent, "Count must be less than or equal to the extent of the span.");

        return span{_start, _start + Count};
    }

    template <typename T, size_t Extent>
    inline constexpr span<T, dynamic_extent> span<T, Extent>::last(size_type count) const
    {
        return {_end - count, _end};
    }

    template <typename T, size_t Extent>
    template <size_t Count>
    inline constexpr span<T, Count> span<T, Extent>::last() const
    {
        static_assert(Count <= Extent, "Count must be less than or equal to the extent of the span.");

        return span{_end - Count, _end};
    }

    template <typename T, size_t Extent>
    template <size_t Offset, size_t Count>
    inline constexpr span<T, dynamic_extent> span<T, Extent>::subspan() const
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
    inline constexpr span<T, dynamic_extent> span<T, Extent>::subspan(size_type offset, size_type count) const
    {
        assert(offset + count <= size());

        if (count == dynamic_extent)
        {
            return {_start + offset, _end};
        }

        return {_start + offset, _start + offset + count};
    }
} // namespace tempest

#endif // tempest_core_span_hpp