#ifndef tempest_core_string_hpp
#define tempest_core_string_hpp

#include <tempest/algorithm.hpp>
#include <tempest/char_traits.hpp>
#include <tempest/iterator.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#include <cassert>
#include <compare>
#include <cstddef>
#include <iterator>

namespace tempest
{
    template <typename CharT, typename Traits = char_traits<CharT>, typename Allocator = ::tempest::allocator<CharT>>
    class basic_string
    {
        static constexpr bool is_noexecpt_movable =
            std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
            std::allocator_traits<Allocator>::is_always_equal::value;

        struct iter
        {
            using iterator_category = std::random_access_iterator_tag;
            using value_type = CharT;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            CharT* ptr;

            constexpr iter& operator++() noexcept;
            constexpr iter operator++(int) noexcept;
            constexpr iter& operator--() noexcept;
            constexpr iter operator--(int) noexcept;
            constexpr iter& operator+=(difference_type n) noexcept;
            constexpr iter& operator-=(difference_type n) noexcept;
            constexpr reference operator*() const noexcept;
            constexpr pointer operator->() const noexcept;
            constexpr reference operator[](difference_type n) const noexcept;
            constexpr iter operator+(difference_type n) const noexcept;
            constexpr iter operator-(difference_type n) const noexcept;
            constexpr difference_type operator-(const iter& other) const noexcept;

            constexpr bool operator==(const iter& other) const noexcept;
            constexpr auto operator<=>(const iter& other) const noexcept = default;

            constexpr operator pointer() const noexcept;
        };

        struct const_iter
        {
            using iterator_category = std::random_access_iterator_tag;
            using value_type = CharT;
            using difference_type = std::ptrdiff_t;
            using pointer = const value_type*;
            using reference = const value_type&;

            const CharT* ptr;

            const_iter(const CharT* ptr) : ptr{ptr}
            {
            }
            const_iter(const iter& other) : ptr{other.ptr}
            {
            }

            constexpr const_iter& operator++() noexcept;
            constexpr const_iter operator++(int) noexcept;
            constexpr const_iter& operator--() noexcept;
            constexpr const_iter operator--(int) noexcept;
            constexpr const_iter& operator+=(difference_type n) noexcept;
            constexpr const_iter& operator-=(difference_type n) noexcept;
            constexpr reference operator*() const noexcept;
            constexpr pointer operator->() const noexcept;
            constexpr reference operator[](difference_type n) const noexcept;
            constexpr const_iter operator+(difference_type n) const noexcept;
            constexpr const_iter operator-(difference_type n) const noexcept;
            constexpr difference_type operator-(const const_iter& other) const noexcept;

            constexpr bool operator==(const const_iter& other) const noexcept;
            constexpr auto operator<=>(const const_iter& other) const noexcept = default;

            constexpr operator pointer() const noexcept;
        };

      public:
        using traits_type = Traits;
        using size_type = size_t;
        using value_type = CharT;
        using allocator_type = Allocator;
        using difference_type = std::ptrdiff_t;
        using reference_type = value_type&;
        using const_reference_type = const value_type&;
        using pointer_type = value_type*;
        using const_pointer_type = const value_type*;

        using iterator = iter;
        using const_iterator = const_iter;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        static constexpr size_type npos = ~size_type(0);

        constexpr basic_string() noexcept(noexcept(Allocator()));
        explicit constexpr basic_string(const Allocator& alloc) noexcept;
        constexpr basic_string(size_type count, value_type ch, const Allocator& alloc = Allocator());
        constexpr basic_string(const basic_string& other, size_type pos, const Allocator& alloc = Allocator());
        constexpr basic_string(const value_type* s, size_type count, const Allocator& alloc = Allocator());
        constexpr basic_string(const value_type* s, const Allocator& alloc = Allocator());

        template <input_iterator InputIt>
        constexpr basic_string(InputIt first, InputIt last, const Allocator& alloc = Allocator());

        constexpr basic_string(const basic_string& other);
        constexpr basic_string(const basic_string& other, const Allocator& alloc);
        constexpr basic_string(basic_string&& other) noexcept;
        constexpr basic_string(basic_string&& other, const Allocator& alloc);

        template <typename StringLikeView>
            requires(is_convertible_v<const StringLikeView&, basic_string_view<CharT, Traits>> &&
                     !is_convertible_v<const StringLikeView&, const CharT*>)
        constexpr basic_string(const StringLikeView& view, const Allocator& alloc = Allocator());

        basic_string(nullptr_t) = delete;

        constexpr ~basic_string();

        constexpr basic_string& operator=(const basic_string& other);
        constexpr basic_string& operator=(basic_string&& other) noexcept(is_noexecpt_movable);

        constexpr basic_string& operator=(const value_type* s);
        constexpr basic_string& operator=(value_type ch);

        basic_string& operator=(nullptr_t) = delete;

        constexpr basic_string& assign(size_type count, value_type ch);
        constexpr basic_string& assign(const basic_string& str);
        constexpr basic_string& assign(const basic_string& str, size_type pos, size_type count = npos);
        constexpr basic_string& assign(basic_string&& str) noexcept(is_noexecpt_movable);
        constexpr basic_string& assign(const value_type* s, size_type count);
        constexpr basic_string& assign(const value_type* s);

        template <input_iterator InputIt>
        constexpr basic_string& assign(InputIt first, InputIt last);

        constexpr allocator_type get_allocator() const noexcept;

        constexpr value_type& at(size_type pos);
        constexpr const value_type& at(size_type pos) const;

        constexpr value_type& operator[](size_type pos);
        constexpr const value_type& operator[](size_type pos) const;

        constexpr value_type& front();
        constexpr const value_type& front() const;

        constexpr value_type& back();
        constexpr const value_type& back() const;

        constexpr value_type* data() noexcept;
        constexpr const value_type* data() const noexcept;

        constexpr const value_type* c_str() const noexcept;

        constexpr iterator begin() noexcept;
        constexpr const_iterator begin() const noexcept;
        constexpr const_iterator cbegin() const noexcept;

        constexpr iterator end() noexcept;
        constexpr const_iterator end() const noexcept;
        constexpr const_iterator cend() const noexcept;

        constexpr reverse_iterator rbegin() noexcept;
        constexpr const_reverse_iterator rbegin() const noexcept;
        constexpr const_reverse_iterator crbegin() const noexcept;

        constexpr reverse_iterator rend() noexcept;
        constexpr const_reverse_iterator rend() const noexcept;
        constexpr const_reverse_iterator crend() const noexcept;

        constexpr bool empty() const noexcept;
        constexpr size_type size() const noexcept;
        constexpr size_type length() const noexcept;
        constexpr size_type capacity() const noexcept;
        constexpr size_type max_size() const noexcept;

        constexpr void reserve(size_type new_cap);
        constexpr void shrink_to_fit();

        constexpr void clear() noexcept;

        constexpr basic_string& insert(const_iterator pos, size_type count, value_type ch);
        constexpr basic_string& insert(const_iterator pos, const value_type* s);
        constexpr basic_string& insert(const_iterator pos, const value_type* s, size_type count);
        constexpr basic_string& insert(const_iterator pos, const basic_string& str);
        constexpr basic_string& insert(const_iterator pos, const basic_string& str, size_type s_index,
                                       size_type count = npos);
        constexpr basic_string& insert(const_iterator pos, value_type ch);

        template <input_iterator InputIt>
        constexpr basic_string& insert(const_iterator pos, InputIt first, InputIt last);

        constexpr basic_string& erase(const_iterator pos);
        constexpr basic_string& erase(const_iterator first, const_iterator last);

        constexpr void push_back(value_type ch);
        constexpr void pop_back();

        constexpr basic_string& append(size_type count, value_type ch);
        constexpr basic_string& append(const basic_string& str);
        constexpr basic_string& append(const basic_string& str, size_type pos, size_type count = npos);
        constexpr basic_string& append(const value_type* s, size_type count);
        constexpr basic_string& append(const value_type* s);

        template <input_iterator InputIt>
        constexpr basic_string& append(InputIt first, InputIt last);

        constexpr basic_string& operator+=(const basic_string& str);
        constexpr basic_string& operator+=(value_type ch);
        constexpr basic_string& operator+=(const value_type* s);

        constexpr basic_string& replace(const_iterator first, const_iterator last, const basic_string& str);
        constexpr basic_string& replace(const_iterator first, const_iterator last, const value_type* s,
                                        size_type s_count);
        constexpr basic_string& replace(const_iterator first, const_iterator last, const value_type* s);
        constexpr basic_string& replace(const_iterator first, const_iterator last, size_type s_count, value_type ch);

        template <input_iterator InputIt>
        constexpr basic_string& replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2);

        constexpr void resize(size_type count);
        constexpr void resize(size_type count, value_type ch);

        constexpr void swap(basic_string& other) noexcept(is_noexecpt_movable);

        constexpr operator basic_string_view<CharT, Traits>() const noexcept;

      private:
        struct large_string
        {
            value_type* data;
            size_type size;
            size_type capacity;
        };

        static constexpr size_type small_string_capacity = sizeof(large_string) / sizeof(value_type);

        struct small_string
        {
            value_type data[small_string_capacity];
        };

        union storage {
            small_string small;
            large_string large;
        } _storage;

        allocator_type _alloc;

        constexpr bool _is_small() const noexcept;
        constexpr void _release_large() noexcept;
        constexpr void _emplace_remaining_small_capacity(size_t size) noexcept;
        constexpr size_t _small_string_capacity() const noexcept;
        constexpr size_t _large_string_capacity() const noexcept;
        constexpr size_t _small_string_size() const noexcept;
        constexpr size_t _large_string_size() const noexcept;
        constexpr void _set_capacity(size_t cap) noexcept;
        constexpr size_t _aligned_large_string_allocation(size_t requested) const noexcept;

        using alloc_traits = std::allocator_traits<allocator_type>;
    };

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::iter& basic_string<CharT, Traits,
                                                                                Allocator>::iter::operator++() noexcept
    {
        ++ptr;
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::iter basic_string<
        CharT, Traits, Allocator>::iter::operator++(int) noexcept
    {
        auto copy = *this;
        ++ptr;
        return copy;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::iter& basic_string<CharT, Traits,
                                                                                Allocator>::iter::operator--() noexcept
    {
        --ptr;
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::iter basic_string<
        CharT, Traits, Allocator>::iter::operator--(int) noexcept
    {
        auto copy = *this;
        --ptr;
        return copy;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::iter& basic_string<
        CharT, Traits, Allocator>::iter::operator+=(difference_type n) noexcept
    {
        ptr += n;
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::iter& basic_string<
        CharT, Traits, Allocator>::iter::operator-=(difference_type n) noexcept
    {
        ptr -= n;
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::iter::reference basic_string<
        CharT, Traits, Allocator>::iter::operator*() const noexcept
    {
        return *ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::iter::pointer basic_string<
        CharT, Traits, Allocator>::iter::operator->() const noexcept
    {
        return ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::iter::reference basic_string<
        CharT, Traits, Allocator>::iter::operator[](difference_type n) const noexcept
    {
        return ptr[n];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::iter basic_string<
        CharT, Traits, Allocator>::iter::operator+(difference_type n) const noexcept
    {
        return iter{ptr + n};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::iter basic_string<
        CharT, Traits, Allocator>::iter::operator-(difference_type n) const noexcept
    {
        return iter{ptr - n};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::difference_type basic_string<
        CharT, Traits, Allocator>::iter::operator-(const iter& other) const noexcept
    {
        return ptr - other.ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr bool basic_string<CharT, Traits, Allocator>::iter::operator==(const iter& other) const noexcept
    {
        return ptr == other.ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::iter::operator pointer() const noexcept
    {
        return ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::const_iter& basic_string<
        CharT, Traits, Allocator>::const_iter::operator++() noexcept
    {
        ++ptr;
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::const_iter basic_string<
        CharT, Traits, Allocator>::const_iter::operator++(int) noexcept
    {
        auto copy = *this;
        ++ptr;
        return copy;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::const_iter& basic_string<
        CharT, Traits, Allocator>::const_iter::operator--() noexcept
    {
        --ptr;
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::const_iter basic_string<
        CharT, Traits, Allocator>::const_iter::operator--(int) noexcept
    {
        auto copy = *this;
        --ptr;
        return copy;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::const_iter& basic_string<
        CharT, Traits, Allocator>::const_iter::operator+=(difference_type n) noexcept
    {
        ptr += n;
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::const_iter& basic_string<
        CharT, Traits, Allocator>::const_iter::operator-=(difference_type n) noexcept
    {
        ptr -= n;
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_iter::reference basic_string<
        CharT, Traits, Allocator>::const_iter::operator*() const noexcept
    {
        return *ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_iter::pointer basic_string<
        CharT, Traits, Allocator>::const_iter::operator->() const noexcept
    {
        return ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_iter::reference basic_string<
        CharT, Traits, Allocator>::const_iter::operator[](difference_type n) const noexcept
    {
        return ptr[n];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::const_iter basic_string<
        CharT, Traits, Allocator>::const_iter::operator+(difference_type n) const noexcept
    {
        return const_iter{ptr + n};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::const_iter basic_string<
        CharT, Traits, Allocator>::const_iter::operator-(difference_type n) const noexcept
    {
        return const_iter{ptr - n};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::difference_type basic_string<
        CharT, Traits, Allocator>::const_iter::operator-(const const_iter& other) const noexcept
    {
        return ptr - other.ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr bool basic_string<CharT, Traits, Allocator>::const_iter::operator==(
        const const_iter& other) const noexcept
    {
        return ptr == other.ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::const_iter::operator pointer() const noexcept
    {
        return ptr;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string() noexcept(noexcept(Allocator()))
    {
        _emplace_remaining_small_capacity(0);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(const Allocator& alloc) noexcept
        : _alloc{alloc}
    {
        _emplace_remaining_small_capacity(0);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(size_type count, value_type ch,
                                                                          const Allocator& alloc)
        : _alloc{alloc}
    {
        _emplace_remaining_small_capacity(0);
        assign(count, ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(const basic_string& other, size_type pos,
                                                                          const Allocator& alloc)
        : _alloc{alloc}
    {
        _emplace_remaining_small_capacity(0);
        assign(other, pos);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(const value_type* s, size_type count,
                                                                          const Allocator& alloc)
        : _alloc{alloc}
    {
        _emplace_remaining_small_capacity(0);
        assign(s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(const value_type* s, const Allocator& alloc)
        : _alloc{alloc}
    {
        _emplace_remaining_small_capacity(0);
        assign(s);
    }

    template <typename CharT, typename Traits, typename Allocator>
    template <input_iterator InputIt>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(InputIt first, InputIt last,
                                                                          const Allocator& alloc)
        : _alloc{alloc}
    {
        _emplace_remaining_small_capacity(0);
        assign(first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    template <typename StringLikeView>
        requires(is_convertible_v<const StringLikeView&, basic_string_view<CharT, Traits>> &&
                 !is_convertible_v<const StringLikeView&, const CharT*>)
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(const StringLikeView& view,
                                                                          const Allocator& alloc)
        : _alloc{alloc}
    {
        _emplace_remaining_small_capacity(0);
        assign(view.data(), view.size());
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(const basic_string& other)
        : basic_string{other, alloc_traits::select_on_container_copy_construction(other._alloc)}
    {
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(const basic_string& other,
                                                                          const Allocator& alloc)
        : _alloc{alloc}
    {
        _emplace_remaining_small_capacity(0);
        assign(other);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(basic_string&& other) noexcept
        : basic_string{move(other), other._alloc}
    {
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(basic_string&& other, const Allocator& alloc)
        : _alloc{alloc}
    {
        if (other._is_small())
        {
            Traits::move(_storage.small.data, other._storage.small.data, small_string_capacity);
        }
        else
        {
            _storage.large = other._storage.large;
            other._storage.large.data = nullptr;
            other._storage.large.size = 0;
            other._set_capacity(0);
        }
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>::~basic_string()
    {
        if (!_is_small())
        {
            destroy_n(_storage.large.data, _storage.large.size);
            _alloc.deallocate(_storage.large.data, _storage.large.capacity);
        }
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::operator=(
        const basic_string& other)
    {
        if (&other == this)
        {
            return *this;
        }

        if (alloc_traits::propagate_on_container_copy_assignment::value)
        {
            _alloc = other._alloc;
        }

        assign(other);
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::operator=(
        basic_string&& other) noexcept(is_noexecpt_movable)
    {
        if (&other == this)
        {
            return *this;
        }

        if (alloc_traits::propagate_on_container_move_assignment::value)
        {
            _alloc = move(other._alloc);
        }

        assign(move(other));
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::operator=(
        const value_type* s)
    {
        assign(s);
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::operator=(
        value_type ch)
    {
        assign(1, ch);
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::assign(
        size_type count, value_type ch)
    {
        clear();

        if (count < small_string_capacity)
        {
            _release_large();

            Traits::assign(_storage.small.data, count, ch);
            _storage.small.data[count] = value_type();
            _emplace_remaining_small_capacity(count);
        }
        else
        {
            if (count > capacity())
            {
                _release_large();
                auto alloc_size = _aligned_large_string_allocation(count);
                _storage.large.data = _alloc.allocate(alloc_size + 1);
                _set_capacity(alloc_size);
            }

            Traits::assign(_storage.large.data, count, ch);
            _storage.large.data[count] = value_type();
            _storage.large.size = count;
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::assign(
        const basic_string& str)
    {
        return assign(str, 0, npos);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::assign(
        const basic_string& str, size_type pos, size_type count)
    {
        assert(pos <= str.size());
        count = std::min(count, str.size() - pos);

        if (count < small_string_capacity)
        {
            _release_large();

            Traits::copy(_storage.small.data, str.data() + pos, count);
            _storage.small.data[count] = value_type();
            _emplace_remaining_small_capacity(count);
        }
        else
        {
            if (count > capacity())
            {
                _release_large();
                auto alloc_size = _aligned_large_string_allocation(count);
                _storage.large.data = _alloc.allocate(alloc_size + 1);
                _set_capacity(alloc_size);
            }

            Traits::copy(_storage.large.data, str.data() + pos, count);
            _storage.large.data[count] = value_type();
            _storage.large.size = count;
        }

        _alloc = str._alloc;

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::assign(
        basic_string&& str) noexcept(is_noexecpt_movable)
    {
        if (str._is_small())
        {
            _release_large();

            Traits::move(_storage.small.data, str._storage.small.data, small_string_capacity);
        }
        else
        {
            _storage.large = str._storage.large;
            str._storage.large.data = nullptr;
            str._storage.large.size = 0;
        }

        str._set_capacity(0);

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::assign(
        const value_type* s, size_type count)
    {
        if (count < small_string_capacity)
        {
            _release_large();

            Traits::copy(_storage.small.data, s, count);
            _storage.small.data[count] = value_type();
            _emplace_remaining_small_capacity(count);
        }
        else
        {
            if (count > capacity())
            {
                _release_large();
                auto alloc_size = _aligned_large_string_allocation(count);
                _storage.large.data = _alloc.allocate(alloc_size + 1);
                _set_capacity(alloc_size);
            }

            Traits::copy(_storage.large.data, s, count);
            _storage.large.data[count] = value_type();
            _storage.large.size = count;
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::assign(
        const value_type* s)
    {
        return assign(s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    template <input_iterator InputIt>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::assign(
        InputIt first, InputIt last)
    {
        size_t count = std::distance(first, last);
        if (count < small_string_capacity)
        {
            _release_large();

            auto it = first;
            for (size_type i = 0; i < count; ++i)
            {
                _storage.small.data[i] = *it;
                ++it;
            }

            _storage.small.data[count] = value_type();
            _emplace_remaining_small_capacity(count);
        }
        else
        {
            if (count > capacity())
            {
                _release_large();
                auto alloc_size = _aligned_large_string_allocation(count);
                _storage.large.data = _alloc.allocate(alloc_size + 1);
                _set_capacity(alloc_size);
            }

            auto it = first;
            for (size_type i = 0; i < count; ++i)
            {
                _storage.large.data[i] = *it;
                ++it;
            }

            _storage.large.data[count] = value_type();
            _storage.large.size = count;
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::allocator_type basic_string<
        CharT, Traits, Allocator>::get_allocator() const noexcept
    {
        return _alloc;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::value_type& basic_string<
        CharT, Traits, Allocator>::at(size_type pos)
    {
        assert(pos < size() && "Position out of bounds");

        return data()[pos];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr const typename basic_string<CharT, Traits, Allocator>::value_type& basic_string<
        CharT, Traits, Allocator>::at(size_type pos) const
    {
        assert(pos < size() && "Position out of bounds");

        return data()[pos];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::value_type& basic_string<
        CharT, Traits, Allocator>::operator[](size_type pos)
    {
        return data()[pos];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr const typename basic_string<CharT, Traits, Allocator>::value_type& basic_string<
        CharT, Traits, Allocator>::operator[](size_type pos) const
    {
        return data()[pos];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::value_type& basic_string<CharT, Traits,
                                                                                               Allocator>::front()
    {
        return data()[0];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr const typename basic_string<CharT, Traits, Allocator>::value_type& basic_string<
        CharT, Traits, Allocator>::front() const
    {
        return data()[0];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::value_type& basic_string<CharT, Traits,
                                                                                               Allocator>::back()
    {
        return data()[size() - 1];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr const typename basic_string<CharT, Traits, Allocator>::value_type& basic_string<
        CharT, Traits, Allocator>::back() const
    {
        return data()[size() - 1];
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::value_type* basic_string<
        CharT, Traits, Allocator>::data() noexcept
    {
        return _is_small() ? _storage.small.data : _storage.large.data;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr const typename basic_string<CharT, Traits, Allocator>::value_type* basic_string<
        CharT, Traits, Allocator>::data() const noexcept
    {
        return _is_small() ? _storage.small.data : _storage.large.data;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr const typename basic_string<CharT, Traits, Allocator>::value_type* basic_string<
        CharT, Traits, Allocator>::c_str() const noexcept
    {
        return data();
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::iterator basic_string<CharT, Traits,
                                                                                            Allocator>::begin() noexcept
    {
        return iterator{data()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_iterator basic_string<
        CharT, Traits, Allocator>::begin() const noexcept
    {
        return const_iterator{data()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_iterator basic_string<
        CharT, Traits, Allocator>::cbegin() const noexcept
    {
        return const_iterator{data()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::iterator basic_string<CharT, Traits,
                                                                                            Allocator>::end() noexcept
    {
        return iterator{data() + size()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_iterator basic_string<
        CharT, Traits, Allocator>::end() const noexcept
    {
        return const_iterator{data() + size()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_iterator basic_string<
        CharT, Traits, Allocator>::cend() const noexcept
    {
        return const_iterator{data() + size()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::reverse_iterator basic_string<
        CharT, Traits, Allocator>::rbegin() noexcept
    {
        return reverse_iterator{end()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_reverse_iterator basic_string<
        CharT, Traits, Allocator>::rbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_reverse_iterator basic_string<
        CharT, Traits, Allocator>::crbegin() const noexcept
    {
        return const_reverse_iterator{cend()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::reverse_iterator basic_string<
        CharT, Traits, Allocator>::rend() noexcept
    {
        return reverse_iterator{begin()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_reverse_iterator basic_string<
        CharT, Traits, Allocator>::rend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::const_reverse_iterator basic_string<
        CharT, Traits, Allocator>::crend() const noexcept
    {
        return const_reverse_iterator{cbegin()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr bool basic_string<CharT, Traits, Allocator>::empty() const noexcept
    {
        return size() == 0;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::size_type basic_string<
        CharT, Traits, Allocator>::size() const noexcept
    {
        return _is_small() ? _small_string_size() : _large_string_size();
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::size_type basic_string<
        CharT, Traits, Allocator>::length() const noexcept
    {
        return size();
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::size_type basic_string<
        CharT, Traits, Allocator>::capacity() const noexcept
    {
        return _is_small() ? _small_string_capacity() : _large_string_capacity();
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr typename basic_string<CharT, Traits, Allocator>::size_type basic_string<
        CharT, Traits, Allocator>::max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max();
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::reserve(size_type new_cap)
    {
        if (new_cap <= capacity())
        {
            return;
        }

        // We can only reserve more space if we are not requesting a small string
        if (new_cap >= small_string_capacity)
        {
            // Round new cap to the nearest multiple of 8
            new_cap = _aligned_large_string_allocation(new_cap);

            auto current_size = size();
            auto new_data = _alloc.allocate(new_cap + 1);
            Traits::move(new_data, data(), size());
            new_data[size()] = value_type();
            _release_large();
            _storage.large.data = new_data;
            _storage.large.size = current_size;

            _set_capacity(new_cap);
        }
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::shrink_to_fit()
    {
        // Shrink to fit fails if the string is already small
        if (size() < small_string_capacity)
        {
            return;
        }

        auto aligned_size = _aligned_large_string_allocation(size());
        auto new_data = _alloc.allocate(aligned_size + 1);
        Traits::move(new_data, data(), size());
        new_data[size()] = value_type();
        _release_large();
        _storage.large.data = new_data;
        _storage.large.size = size();
        _set_capacity(aligned_size);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::clear() noexcept
    {
        if (!_is_small())
        {
            destroy_n(_storage.large.data, _storage.large.size);
            _storage.large.size = 0;
        }
        else
        {
            _emplace_remaining_small_capacity(0);
        }
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::insert(
        const_iterator pos, size_type count, value_type ch)
    {
        if (count == 0)
        {
            return *this;
        }

        auto current_size = size();
        auto insert_pos = pos - begin();
        auto new_size = current_size + count;

        if (new_size < small_string_capacity)
        {
            auto insert_ptr = data() + insert_pos;
            auto end_ptr = data() + current_size;
            Traits::move(insert_ptr + count, insert_ptr, end_ptr - insert_ptr);
            Traits::assign(insert_ptr, count, ch);
            _storage.small.data[new_size] = value_type();
            _emplace_remaining_small_capacity(new_size);
        }
        else
        {
            if (new_size > capacity())
            {
                auto new_cap = std::max(new_size, 2 * capacity());
                auto new_data = _alloc.allocate(new_cap + 1);

                Traits::move(new_data, data(), insert_pos);
                Traits::assign(new_data + insert_pos, count, ch);
                Traits::move(new_data + insert_pos + count, data() + insert_pos, current_size - insert_pos);
                _release_large();

                _storage.large.data = new_data;
                _storage.large.size = new_size;
                _set_capacity(new_cap);
            }
            else
            {
                auto insert_ptr = data() + insert_pos;
                auto end_ptr = data() + current_size;
                Traits::move(insert_ptr + count, insert_ptr, end_ptr - insert_ptr);
                Traits::assign(insert_ptr, count, ch);
                _storage.large.data[new_size] = value_type();
                _storage.large.size = new_size;
            }
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::insert(
        const_iterator pos, const value_type* s)
    {
        return insert(pos, s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::insert(
        const_iterator pos, const value_type* s, size_type count)
    {
        if (count == 0)
        {
            return *this;
        }

        auto current_size = size();
        auto insert_pos = pos - begin();
        auto new_size = current_size + count;

        if (new_size < small_string_capacity)
        {
            auto insert_ptr = data() + insert_pos;
            auto end_ptr = data() + current_size;
            Traits::move(insert_ptr + count, insert_ptr, end_ptr - insert_ptr);
            Traits::copy(insert_ptr, s, count);
            _storage.small.data[new_size] = value_type();
            _emplace_remaining_small_capacity(new_size);
        }
        else
        {
            if (new_size > capacity())
            {
                auto new_cap = std::max(new_size, 2 * capacity());
                auto new_data = _alloc.allocate(new_cap + 1);

                Traits::move(new_data, data(), insert_pos);
                Traits::copy(new_data + insert_pos, s, count);
                Traits::move(new_data + insert_pos + count, data() + insert_pos, current_size - insert_pos);
                _release_large();

                _storage.large.data = new_data;
                _storage.large.size = new_size;
                _set_capacity(new_cap);
            }
            else
            {
                auto insert_ptr = data() + insert_pos;
                auto end_ptr = data() + current_size;
                Traits::move(insert_ptr + count, insert_ptr, end_ptr - insert_ptr);
                Traits::copy(insert_ptr, s, count);
                _storage.large.data[new_size] = value_type();
                _storage.large.size = new_size;
            }
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::insert(
        const_iterator pos, const basic_string& str)
    {
        return insert(pos, str.data(), str.size());
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::insert(
        const_iterator pos, const basic_string& str, size_type s_index, size_type count)
    {
        assert(s_index <= str.size());
        count = std::min(count, str.size() - s_index);

        return insert(pos, str.data() + s_index, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::insert(
        const_iterator pos, value_type ch)
    {
        return insert(pos, 1, ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    template <input_iterator InputIt>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::insert(
        const_iterator pos, InputIt first, InputIt last)
    {
        auto count = std::distance(first, last);
        if (count == 0)
        {
            return *this;
        }

        auto current_size = size();
        auto insert_pos = pos - begin();
        auto new_size = current_size + count;

        if (new_size < small_string_capacity)
        {
            auto insert_ptr = data() + insert_pos;
            auto end_ptr = data() + current_size;
            Traits::move(insert_ptr + count, insert_ptr, end_ptr - insert_ptr);

            auto it = first;
            for (size_type i = 0; i < count; ++i)
            {
                insert_ptr[i] = *it;
                ++it;
            }

            _storage.small.data[new_size] = value_type();
            _emplace_remaining_small_capacity(new_size);
        }
        else
        {
            if (new_size > capacity())
            {
                auto new_cap = std::max(new_size, 2 * capacity());
                auto new_data = _alloc.allocate(new_cap + 1);

                Traits::move(new_data, data(), insert_pos);

                auto it = first;
                for (size_type i = 0; i < count; ++i)
                {
                    new_data[insert_pos + i] = *it;
                    ++it;
                }

                Traits::move(new_data + insert_pos + count, data() + insert_pos, current_size - insert_pos);
                _release_large();

                _storage.large.data = new_data;
                _storage.large.size = new_size;
                _set_capacity(new_cap);
            }
            else
            {
                auto insert_ptr = data() + insert_pos;
                auto end_ptr = data() + current_size;
                Traits::move(insert_ptr + count, insert_ptr, end_ptr - insert_ptr);

                auto it = first;
                for (size_type i = 0; i < count; ++i)
                {
                    insert_ptr[i] = *it;
                    ++it;
                }

                _storage.large.data[new_size] = value_type();
                _storage.large.size = new_size;
            }
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::erase(
        const_iterator first)
    {
        return erase(first, first + 1);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::erase(
        const_iterator first, const_iterator last)
    {
        auto current_size = size();
        auto erase_pos = first - begin();
        auto erase_count = last - first;

        if (erase_count == 0)
        {
            return *this;
        }

        auto new_size = current_size - erase_count;

        // Do not change downgrade to small string optimization, keep capacity
        auto erase_ptr = data() + erase_pos;
        auto end_ptr = data() + current_size;
        Traits::move(erase_ptr, erase_ptr + erase_count, end_ptr - erase_ptr);
        data()[new_size] = value_type();

        if (_is_small())
        {
            _emplace_remaining_small_capacity(new_size);
        }
        else
        {
            _storage.large.size = new_size;
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::push_back(value_type ch)
    {
        // Check if we are a small string and have enough capacity
        if (auto ss_size = _small_string_size(); _is_small() && ss_size < small_string_capacity - 1)
        {
            _storage.small.data[ss_size] = ch;
            _storage.small.data[ss_size + 1] = value_type();
            _emplace_remaining_small_capacity(ss_size + 1);

            return;
        }

        // Check if we are at capacity and need to resize
        if (size() == capacity())
        {
            // Geometric growth
            auto new_cap = 2 * capacity();
            reserve(new_cap);
        }

        // We are a large string and have sufficient capacity to append to back of the string
        _storage.large.data[size()] = ch;
        _storage.large.data[size() + 1] = value_type();
        _storage.large.size += 1;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::pop_back()
    {
        if (empty())
        {
            return;
        }

        if (_is_small())
        {
            auto ss_size = _small_string_size();
            _storage.small.data[ss_size - 1] = value_type();
            _storage.small.data[small_string_capacity - 1] = static_cast<value_type>(ss_size - 1);
        }
        else
        {
            _storage.large.data[size() - 1] = value_type();
            _storage.large.size -= 1;
        }
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::append(
        size_type count, value_type ch)
    {
        if (auto ss_size = _small_string_size(); _is_small() && ss_size + count < small_string_capacity - 1)
        {
            Traits::assign(_storage.small.data + ss_size, count, ch);
            _storage.small.data[ss_size + count] = value_type();
            _emplace_remaining_small_capacity(ss_size + count);

            return *this;
        }

        auto current_size = size();
        auto new_size = current_size + count;

        if (new_size > capacity())
        {
            auto new_cap = std::max(new_size, 2 * capacity());
            reserve(new_cap);
        }

        Traits::assign(_storage.large.data + current_size, count, ch);
        _storage.large.data[new_size] = value_type();
        _storage.large.size = new_size;

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::append(
        const basic_string& str)
    {
        return append(str.data(), str.size());
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::append(
        const basic_string& str, size_type pos, size_type count)
    {
        assert(pos <= str.size());
        count = std::min(count, str.size() - pos);

        return append(str.data() + pos, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::append(
        const value_type* s)
    {
        return append(s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::append(
        const value_type* s, size_type count)
    {
        if (auto ss_size = _small_string_size(); _is_small() && ss_size + count < small_string_capacity - 1)
        {
            Traits::copy(_storage.small.data + ss_size, s, count);
            _storage.small.data[ss_size + count] = value_type();
            _emplace_remaining_small_capacity(ss_size + count);

            return *this;
        }

        auto current_size = size();
        auto new_size = current_size + count;

        if (new_size > capacity())
        {
            auto new_cap = std::max(new_size, 2 * capacity());
            reserve(new_cap);
        }

        Traits::copy(_storage.large.data + current_size, s, count);
        _storage.large.data[new_size] = value_type();
        _storage.large.size = new_size;

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    template <input_iterator InputIt>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::append(
        InputIt first, InputIt last)
    {
        auto count = std::distance(first, last);
        if (auto ss_size = _small_string_size(); _is_small() && ss_size + count < small_string_capacity - 1)
        {
            auto it = first;
            for (size_type i = 0; i < count; ++i)
            {
                _storage.small.data[ss_size + i] = *it;
                ++it;
            }

            _storage.small.data[ss_size + count] = value_type();
            _emplace_remaining_small_capacity(ss_size + count);

            return *this;
        }

        auto current_size = size();
        auto new_size = current_size + count;

        if (new_size > capacity())
        {
            auto new_cap = std::max(new_size, 2 * capacity());
            reserve(new_cap);
        }

        auto it = first;
        for (size_type i = 0; i < count; ++i)
        {
            _storage.large.data[current_size + i] = *it;
            ++it;
        }

        _storage.large.data[new_size] = value_type();
        _storage.large.size = new_size;

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::operator+=(
        value_type ch)
    {
        push_back(ch);
        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::operator+=(
        const basic_string& str)
    {
        return append(str);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::operator+=(
        const value_type* s)
    {
        return append(s);
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::replace(
        const_iterator first, const_iterator last, const basic_string<CharT, Traits, Allocator>& str)
    {
        return replace(first, last, str.data(), str.size());
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::replace(
        const_iterator first, const_iterator last, const value_type* s)
    {
        return replace(first, last, s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::replace(
        const_iterator first, const_iterator last, const value_type* s, size_type count)
    {
        auto current_size = size();
        auto erase_pos = first - begin();
        size_type erase_count = last - first;
        auto insert_count = count;

        if (erase_count > insert_count)
        {
            auto new_size = current_size - erase_count + insert_count;

            auto erase_ptr = data() + erase_pos;
            auto end_ptr = data() + current_size;

            // Number of elements to move
            auto move_count = end_ptr - erase_ptr - erase_count;
            Traits::move(erase_ptr + insert_count, erase_ptr + erase_count, move_count);

            Traits::copy(erase_ptr, s, count);
            data()[new_size] = value_type();

            if (_is_small())
            {
                _emplace_remaining_small_capacity(new_size);
            }
            else
            {
                _storage.large.size = new_size;
            }
        }
        else if (erase_count < insert_count)
        {
            auto new_size = current_size - erase_count + insert_count;

            if (new_size > capacity())
            {
                auto new_cap = std::max(new_size, 2 * capacity());
                reserve(new_cap);
            }

            auto erase_ptr = data() + erase_pos;
            auto end_ptr = data() + current_size;

            // Number of elements to move
            auto move_count = end_ptr - erase_ptr - erase_count;
            Traits::move(erase_ptr + insert_count, erase_ptr + erase_count, move_count);

            Traits::copy(erase_ptr, s, count);
            _storage.large.data[new_size] = value_type();
            _storage.large.size = new_size;
        }
        else
        {
            Traits::copy(data() + erase_pos, s, count);
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::replace(
        const_iterator first, const_iterator last, size_type count, value_type ch)
    {
        auto current_size = size();
        auto erase_pos = first - begin();
        size_type erase_count = last - first;
        auto insert_count = count;

        if (erase_count > insert_count)
        {
            auto new_size = current_size - erase_count + insert_count;

            auto erase_ptr = data() + erase_pos;
            auto end_ptr = data() + current_size;

            // Number of elements to move
            auto move_count = end_ptr - erase_ptr - erase_count;
            Traits::move(erase_ptr + insert_count, erase_ptr + erase_count, move_count);

            Traits::assign(erase_ptr, insert_count, ch);
            data()[new_size] = value_type();

            if (_is_small())
            {
                _emplace_remaining_small_capacity(new_size);
            }
            else
            {
                _storage.large.size = new_size;
            }
        }
        else if (erase_count < insert_count)
        {
            auto new_size = current_size - erase_count + insert_count;

            if (new_size > capacity())
            {
                auto new_cap = std::max(new_size, 2 * capacity());
                reserve(new_cap);
            }

            auto erase_ptr = data() + erase_pos;
            auto end_ptr = data() + current_size;

            // Number of elements to move
            auto move_count = end_ptr - erase_ptr - erase_count;
            Traits::move(erase_ptr + insert_count, erase_ptr + erase_count, move_count);

            Traits::assign(erase_ptr, insert_count, ch);
            data()[new_size] = value_type();

            if (_is_small())
            {
                _emplace_remaining_small_capacity(new_size);
            }
            else
            {
                _storage.large.size = new_size;
            }
        }
        else
        {
            Traits::assign(data() + erase_pos, insert_count, ch);
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    template <input_iterator InputIt>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::replace(
        const_iterator first, const_iterator last, InputIt first2, InputIt last2)
    {
        auto current_size = size();
        auto erase_pos = first - begin();
        auto erase_count = last - first;
        auto insert_count = std::distance(first2, last2);

        if (erase_count > insert_count)
        {
            auto new_size = current_size - erase_count + insert_count;

            auto erase_ptr = data() + erase_pos;
            auto end_ptr = data() + current_size;

            // Number of elements to move
            auto move_count = end_ptr - erase_ptr - erase_count;
            Traits::move(erase_ptr + insert_count, erase_ptr + erase_count, move_count);

            auto it = first2;
            for (size_type i = 0; i < insert_count; ++i)
            {
                erase_ptr[i] = *it;
                ++it;
            }

            data()[new_size] = value_type();

            if (_is_small())
            {
                _emplace_remaining_small_capacity(new_size);
            }
            else
            {
                _storage.large.size = new_size;
            }
        }
        else if (erase_count < insert_count)
        {
            auto new_size = current_size - erase_count + insert_count;

            if (new_size > capacity())
            {
                auto new_cap = std::max(new_size, 2 * capacity());
                reserve(new_cap);
            }

            auto erase_ptr = data() + erase_pos;
            auto end_ptr = data() + current_size;

            // Number of elements to move
            auto move_count = end_ptr - erase_ptr - erase_count;
            Traits::move(erase_ptr + insert_count, erase_ptr + erase_count, move_count);

            auto it = first2;
            for (size_type i = 0; i < insert_count; ++i)
            {
                erase_ptr[i] = *it;
                ++it;
            }

            _storage.large.data[new_size] = value_type();
            _storage.large.size = new_size;
        }
        else
        {
            auto erase_ptr = data() + erase_pos;
            auto it = first2;
            for (size_type i = 0; i < insert_count; ++i)
            {
                erase_ptr[i] = *it;
                ++it;
            }
        }

        return *this;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::resize(size_type count)
    {
        resize(count, value_type());
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::resize(size_type count, value_type ch)
    {
        if (count < size())
        {
            erase(begin() + count, end());
        }
        else if (count > size())
        {
            append(count - size(), ch);
        }
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr tempest::basic_string<CharT, Traits, Allocator>::operator basic_string_view<CharT, Traits>()
        const noexcept
    {
        return basic_string_view<CharT, Traits>{data(), size()};
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::swap(basic_string& other) noexcept(
        is_noexecpt_movable)
    {
        if (this == &other)
        {
            return;
        }

        if (_is_small() && other._is_small())
        {
            for (size_type i = 0; i < small_string_capacity; ++i)
            {
                std::swap(_storage.small.data[i], other._storage.small.data[i]);
            }
        }
        else if (!_is_small() && !other._is_small())
        {
            std::swap(_storage.large.data, other._storage.large.data);
            std::swap(_storage.large.size, other._storage.large.size);
            std::swap(_storage.large.capacity, other._storage.large.capacity);
        }
        else
        {
            auto& small = _is_small() ? *this : other;
            auto& large = _is_small() ? other : *this;

            large_string large_str = large._storage.large;

            for (size_type i = 0; i < small_string_capacity; ++i)
            {
                large._storage.small.data[i] = small._storage.small.data[i];
            }

            small._storage.large = large_str;
        }
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr bool basic_string<CharT, Traits, Allocator>::_is_small() const noexcept
    {
        auto last_small_value = _storage.small.data[small_string_capacity - 1];
        // Get the mask of the first 3 bits
        auto flags = static_cast<std::make_unsigned_t<CharT>>(last_small_value) & 0xE0;
        return flags == 0;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::_release_large() noexcept
    {
        if (!_is_small())
        {
            destroy_n(_storage.large.data, _storage.large.size);
            _alloc.deallocate(_storage.large.data, _storage.large.capacity);
        }
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::_emplace_remaining_small_capacity(
        size_t size) noexcept
    {
        _storage.small.data[size] = value_type();

        // Shift the remaining capacity 3 bits left
        auto remaining_capacity = small_string_capacity - size - 1;
        _storage.small.data[small_string_capacity - 1] =
            Traits::to_char_type(static_cast<Traits::int_type>(remaining_capacity));
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr size_t basic_string<CharT, Traits, Allocator>::_small_string_capacity() const noexcept
    {
        return small_string_capacity - 1;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr size_t basic_string<CharT, Traits, Allocator>::_large_string_capacity() const noexcept
    {
        auto cap = _storage.large.capacity;
        // Mask off the top 3 bits of size_t
        constexpr size_t mask_bits = 0b111;
        constexpr size_t mask = mask_bits << (sizeof(size_t) * 8 - 4);

        return cap & ~mask;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr size_t basic_string<CharT, Traits, Allocator>::_small_string_size() const noexcept
    {
        auto last_small_value =
            static_cast<std::make_unsigned_t<CharT>>(_storage.small.data[small_string_capacity - 1]) & ~0xE0;
        // shift the size 3 bits right
        return small_string_capacity - 1 - last_small_value;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr size_t basic_string<CharT, Traits, Allocator>::_large_string_size() const noexcept
    {
        return _storage.large.size;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::_set_capacity(size_t cap) noexcept
    {
        if (cap < small_string_capacity)
        {
            _storage.small.data[small_string_capacity - 1] = static_cast<value_type>(cap);
        }
        else
        {
            // Clear the most significant 3 bits, storing a 1
            auto cleared_top_3_bits = cap & 0x1FFFFFFF;
            // Set the most significant 3 bits to 1
            auto cap_with_flags = set_bit(cleared_top_3_bits, sizeof(cap) * 8 - 3);
            _storage.large.capacity = cap_with_flags;
        }
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr size_t basic_string<CharT, Traits, Allocator>::_aligned_large_string_allocation(
        size_t requested) const noexcept
    {
        // Align to next 8 bytes
        return (requested + 7) & ~7;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr auto operator==(const basic_string<CharT, Traits, Allocator>& lhs,
                                     const basic_string<CharT, Traits, Allocator>& rhs) noexcept -> bool
    {
        return lhs.size() == rhs.size() && Traits::compare(lhs.data(), rhs.data(), lhs.size()) == 0;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr auto operator==(const CharT* lhs, const basic_string<CharT, Traits, Allocator>& rhs) -> bool
    {
        return Traits::compare(lhs, rhs.data(), Traits::length(lhs)) == 0;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr auto operator==(const basic_string<CharT, Traits, Allocator>& lhs, const CharT* rhs) -> bool
    {
        return Traits::compare(lhs.data(), rhs, Traits::length(rhs)) == 0;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr auto operator<=>(const basic_string<CharT, Traits, Allocator>& lhs,
                                      const basic_string<CharT, Traits, Allocator>& rhs) noexcept
        -> std::strong_ordering
    {
        auto cmp = Traits::compare(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size()));
        if (cmp == 0 && lhs.size() != rhs.size())
        {
            return lhs.size() <=> rhs.size();
        }
        return cmp <=> 0;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr auto operator<=>(const CharT* lhs,
                                      const basic_string<CharT, Traits, Allocator>& rhs) -> std::strong_ordering
    {
        auto lhs_size = Traits::length(lhs);
        auto cmp = Traits::compare(lhs, rhs.data(), std::min(lhs_size, rhs.size()));
        if (cmp == 0 && lhs_size != rhs.size())
        {
            return lhs_size <=> rhs.size();
        }
        return cmp <=> 0;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr auto operator<=>(const basic_string<CharT, Traits, Allocator>& lhs,
                                      const CharT* rhs) -> std::strong_ordering
    {
        auto rhs_size = Traits::length(rhs);
        auto cmp = Traits::compare(lhs.data(), rhs, std::min(lhs.size(), rhs_size));
        if (cmp == 0 && lhs.size() != rhs_size)
        {
            return lhs.size() <=> rhs_size;
        }
        return cmp <=> 0;
    }

    using string = basic_string<char>;

    template <typename CharT, typename Traits, typename Allocator>
    constexpr typename basic_string<CharT, Traits, Allocator>::size_type copy(
        const basic_string<CharT, Traits, Allocator>& src,
        typename basic_string<CharT, Traits, Allocator>::value_type* dest,
        typename basic_string<CharT, Traits, Allocator>::size_type count,
        typename basic_string<CharT, Traits, Allocator>::size_type pos = 0)
    {
        assert(pos <= src.size() && "Position out of bounds");

        auto len = std::min(count, src.size() - pos);
        Traits::copy(dest, src.data() + pos, len);
        return len;
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return search(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return search(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return search(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search(const basic_string<CharT, Traits, Allocator>& str, const basic_string<CharT, Traits>& s)
    {
        return search(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr auto search(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return search(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto reverse_search(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return reverse_search(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto reverse_search(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return reverse_search(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto reverse_search(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return reverse_search(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto reverse_search(const basic_string<CharT, Traits, Allocator>& str,
                                  const basic_string<CharT, Traits>& s)
    {
        return reverse_search(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr auto reverse_search(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return reverse_search(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_of(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return search_first_of(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return search_first_of(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return search_first_of(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_of(const basic_string<CharT, Traits, Allocator>& str,
                                   const basic_string<CharT, Traits>& s)
    {
        return search_first_of(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr auto search_first_of(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return search_first_of(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_of(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return search_last_of(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return search_last_of(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return search_last_of(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_of(const basic_string<CharT, Traits, Allocator>& str,
                                  const basic_string<CharT, Traits>& s)
    {
        return search_last_of(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr auto search_last_of(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return search_last_of(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_not_of(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return search_first_not_of(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_not_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return search_first_not_of(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_not_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return search_first_not_of(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_not_of(const basic_string<CharT, Traits, Allocator>& str,
                                       const basic_string<CharT, Traits>& s)
    {
        return search_first_not_of(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr auto search_first_not_of(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return search_first_not_of(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_not_of(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return search_last_not_of(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_not_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return search_last_not_of(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_not_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return search_last_not_of(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_not_of(const basic_string<CharT, Traits, Allocator>& str,
                                      const basic_string<CharT, Traits>& s)
    {
        return search_last_not_of(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr auto search_last_not_of(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return search_last_not_of(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool starts_with(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return starts_with(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool starts_with(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return starts_with(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool starts_with(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return starts_with(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool starts_with(const basic_string<CharT, Traits, Allocator>& str, const basic_string<CharT, Traits>& s)
    {
        return starts_with(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr bool starts_with(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return starts_with(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool ends_with(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return ends_with(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool ends_with(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return ends_with(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool ends_with(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return ends_with(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool ends_with(const basic_string<CharT, Traits, Allocator>& str, const basic_string<CharT, Traits>& s)
    {
        return ends_with(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr bool ends_with(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return ends_with(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool contains(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return contains(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool contains(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return contains(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool contains(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return contains(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool contains(const basic_string<CharT, Traits, Allocator>& str, const basic_string<CharT, Traits>& s)
    {
        return contains(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr bool contains(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return contains(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr int compare(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, size_t count)
    {
        return compare(str.begin(), str.end(), s, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr int compare(const basic_string<CharT, Traits, Allocator>& str, const CharT* s)
    {
        return compare(str.begin(), str.end(), s, Traits::length(s));
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr int compare(const basic_string<CharT, Traits, Allocator>& str, const basic_string<CharT, Traits>& s)
    {
        return compare(str.begin(), str.end(), s.begin(), s.end());
    }

    template <typename CharT, typename Traits, typename Allocator, typename It>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 std::is_same_v<CharT, typename std::iterator_traits<It>::value_type>
    constexpr int compare(const basic_string<CharT, Traits, Allocator>& str, It first, It last)
    {
        return compare(str.begin(), str.end(), first, last);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search(basic_string_view<CharT, Traits> sv,
                          const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return search(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search(const basic_string<CharT, Traits, Allocator>& str,
                          basic_string_view<CharT, Traits> sv) noexcept
    {
        return search(str.begin(), str.end(), sv.begin(), sv.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto reverse_search(basic_string_view<CharT, Traits> sv,
                                  const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return reverse_search(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto reverse_search(const basic_string<CharT, Traits, Allocator>& str,
                                  basic_string_view<CharT, Traits> sv) noexcept
    {
        return reverse_search(str.begin(), str.end(), sv.begin(), sv.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_of(basic_string_view<CharT, Traits> sv,
                                   const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return search_first_of(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_of(const basic_string<CharT, Traits, Allocator>& str,
                                   basic_string_view<CharT, Traits> sv) noexcept
    {
        return search_first_of(str.begin(), str.end(), sv.begin(), sv.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_not_of(basic_string_view<CharT, Traits> sv,
                                       const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return search_first_not_of(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_first_not_of(const basic_string<CharT, Traits, Allocator>& str,
                                       basic_string_view<CharT, Traits> sv) noexcept
    {
        return search_first_not_of(str.begin(), str.end(), sv.begin(), sv.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_of(basic_string_view<CharT, Traits> sv,
                                  const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return search_last_of(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_of(const basic_string<CharT, Traits, Allocator>& str,
                                  basic_string_view<CharT, Traits> sv) noexcept
    {
        return search_last_of(str.begin(), str.end(), sv.begin(), sv.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_not_of(basic_string_view<CharT, Traits> sv,
                                      const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return search_last_not_of(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search_last_not_of(const basic_string<CharT, Traits, Allocator>& str,
                                      basic_string_view<CharT, Traits> sv) noexcept
    {
        return search_last_not_of(str.begin(), str.end(), sv.begin(), sv.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool starts_with(basic_string_view<CharT, Traits> sv,
                               const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return starts_with(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool starts_with(const basic_string<CharT, Traits, Allocator>& str,
                               basic_string_view<CharT, Traits> sv) noexcept
    {
        return starts_with(str.begin(), str.end(), sv.begin(), sv.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool ends_with(basic_string_view<CharT, Traits> sv,
                             const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return ends_with(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool ends_with(const basic_string<CharT, Traits, Allocator>& str,
                             basic_string_view<CharT, Traits> sv) noexcept
    {
        return ends_with(str.begin(), str.end(), sv.begin(), sv.end());
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto substr(const basic_string<CharT, Traits, Allocator>& str, std::size_t pos,
                          std::size_t count) noexcept
    {
        return basic_string_view<CharT, Traits>(str.begin() + pos, count);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr const CharT* data(const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return str.data();
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr typename basic_string<CharT, Traits, Allocator>::size_type size(
        const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return str.size();
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr bool empty(const basic_string<CharT, Traits, Allocator>& str) noexcept
    {
        return str.empty();
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto begin(basic_string<CharT, Traits, Allocator>& str) noexcept -> decltype(str.begin())
    {
        return str.begin();
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto begin(const basic_string<CharT, Traits, Allocator>& str) noexcept -> decltype(str.begin())
    {
        return str.begin();
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto cbegin(const basic_string<CharT, Traits, Allocator>& str) noexcept -> decltype(str.cbegin())
    {
        return str.cbegin();
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto end(basic_string<CharT, Traits, Allocator>& str) noexcept -> decltype(str.end())
    {
        return str.end();
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto end(const basic_string<CharT, Traits, Allocator>& str) noexcept -> decltype(str.end())
    {
        return str.end();
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto cend(const basic_string<CharT, Traits, Allocator>& str) noexcept -> decltype(str.cend())
    {
        return str.cend();
    }

    template <typename CharT, typename Traits, typename Allocator>
    struct hash<basic_string<CharT, Traits, Allocator>>
    {
        std::size_t operator()(const basic_string<CharT, Traits, Allocator>& str) const noexcept
        {
            return hash<basic_string_view<CharT, Traits>>{}(str);
        }
    };
} // namespace tempest

#endif // tempest_core_string_hpp
