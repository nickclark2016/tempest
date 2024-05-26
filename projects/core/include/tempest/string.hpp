#ifndef tempest_core_string_hpp
#define tempest_core_string_hpp

#include <tempest/algorithm.hpp>
#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

#include <cassert>
#include <compare>
#include <cstddef>
#include <iterator>

namespace tempest::core
{
    template <typename T>
    concept character_type =
        std::is_same_v<std::remove_cvref_t<T>, char> || std::is_same_v<std::remove_cvref_t<T>, wchar_t> ||
        std::is_same_v<std::remove_cvref_t<T>, char8_t> || std::is_same_v<std::remove_cvref_t<T>, char16_t> ||
        std::is_same_v<std::remove_cvref_t<T>, char32_t>;

    inline void* memmove(void* dst, const void* src, std::size_t count)
    {
        auto* dst_ptr = reinterpret_cast<std::byte*>(dst);
        const auto* src_ptr = reinterpret_cast<const std::byte*>(src);

        if (dst_ptr < src_ptr)
        {
            while (count--)
            {
                *dst_ptr++ = *src_ptr++;
            }
        }
        else
        {
            dst_ptr += (count - 1);
            src_ptr += (count - 1);

            while (count--)
            {
                *dst_ptr-- = *src_ptr--;
            }
        }

        return dst;
    }

    template <typename CharT>
    class char_traits;

    template <>
    class char_traits<char>
    {
      public:
        using char_type = char;
        using int_type = int;
        using off_type = long long;
        using pos_type = long long;

        static constexpr void assign(char_type& c1, const char_type& c2) noexcept;
        static constexpr char_type* assign(char_type* s, std::size_t n, char_type a);

        static constexpr bool eq(char_type a, char_type b) noexcept;
        static constexpr bool lt(char_type a, char_type b) noexcept;

        static constexpr char_type* move(char_type* dest, const char_type* src, std::size_t count);
        static constexpr char_type* copy(char_type* dest, const char_type* src, std::size_t count);

        static constexpr int compare(const char_type* s1, const char_type* s2, std::size_t count);
        static constexpr std::size_t length(const char_type* s);

        static constexpr const char_type* find(const char_type* ptr, std::size_t count, const char_type& ch);

        static constexpr char_type to_char_type(int_type c) noexcept;
        static constexpr int_type to_int_type(char_type c) noexcept;

        static constexpr bool eq_int_type(int_type c1, int_type c2) noexcept;

        static constexpr int_type eof() noexcept;

        static constexpr int_type not_eof(int_type c) noexcept;
    };

    inline constexpr void char_traits<char>::assign(char_type& c1, const char_type& c2) noexcept
    {
        c1 = c2;
    }

    inline constexpr char_traits<char>::char_type* char_traits<char>::assign(char_type* s, std::size_t n, char_type a)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            s[i] = a;
        }
        return s;
    }

    inline constexpr bool char_traits<char>::eq(char_type a, char_type b) noexcept
    {
        return a == b;
    }

    inline constexpr bool char_traits<char>::lt(char_type a, char_type b) noexcept
    {
        return a < b;
    }

    inline constexpr char_traits<char>::char_type* char_traits<char>::move(char_type* dest, const char_type* src,
                                                                           std::size_t count)
    {
        // For constant expressions, we need to fall back to a less efficient implementation to correctly handle
        // overlapping regions of memory.
        if (std::is_constant_evaluated())
        {
            bool should_loop_forward = true;

            for (const auto* s = src; s != dest; ++s)
            {
                if (s == dest)
                {
                    should_loop_forward = false;
                    break;
                }
            }

            if (should_loop_forward)
            {
                for (std::size_t i = 0; i < count; ++i)
                {
                    dest[i] = src[i];
                }
                return dest;
            }
            else
            {
                for (std::size_t i = count; i != 0; --i)
                {
                    dest[i - 1] = src[i - 1];
                }
                return dest;
            }
        }

        (void)memmove(dest, src, count);
        return dest;
    }

    inline constexpr char_traits<char>::char_type* char_traits<char>::copy(char_type* dest, const char_type* src,
                                                                           std::size_t count)
    {
        // Assume: No overlapping regions
        for (std::size_t i = 0; i < count; ++i)
        {
            dest[i] = src[i];
        }

        return dest;
    }

    inline constexpr int char_traits<char>::compare(const char_type* s1, const char_type* s2, std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            auto diff = s1[i] - s2[i];
            if (diff != 0)
            {
                return diff;
            }
        }

        return 0;
    }

    inline constexpr std::size_t char_traits<char>::length(const char_type* s)
    {
        std::size_t len = 0;
        while (s[len] != char_type())
        {
            ++len;
        }
        return len;
    }

    inline constexpr const char_traits<char>::char_type* char_traits<char>::find(const char_type* ptr,
                                                                                 std::size_t count, const char_type& ch)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            if (ptr[i] == ch)
            {
                return ptr + i;
            }
        }

        return nullptr;
    }

    inline constexpr char_traits<char>::char_type char_traits<char>::to_char_type(int_type c) noexcept
    {
        return static_cast<char_type>(c);
    }

    inline constexpr char_traits<char>::int_type char_traits<char>::to_int_type(char_type c) noexcept
    {
        return static_cast<int_type>(c);
    }

    inline constexpr bool char_traits<char>::eq_int_type(int_type c1, int_type c2) noexcept
    {
        return c1 == c2;
    }

    inline constexpr char_traits<char>::int_type char_traits<char>::eof() noexcept
    {
        return -1; // EOF is -1
    }

    inline constexpr char_traits<char>::int_type char_traits<char>::not_eof(int_type c) noexcept
    {
        return c != eof() ? c : !eof();
    }

    template <typename CharT, typename Traits = char_traits<CharT>, typename Allocator = allocator<CharT>>
    class basic_string
    {
        // TODO: Handle string_view construction and assignment

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
        using size_type = std::size_t;
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

        static constexpr size_type npos = -1;

        constexpr basic_string() noexcept(noexcept(Allocator()));
        explicit constexpr basic_string(const Allocator& alloc) noexcept;
        constexpr basic_string(size_type count, value_type ch, const Allocator& alloc = Allocator());
        constexpr basic_string(const basic_string& other, size_type pos, const Allocator& alloc = Allocator());
        constexpr basic_string(const value_type* s, size_type count, const Allocator& alloc = Allocator());
        constexpr basic_string(const value_type* s, const Allocator& alloc = Allocator());

        template <std::input_iterator InputIt>
        constexpr basic_string(InputIt first, InputIt last, const Allocator& alloc = Allocator());

        constexpr basic_string(const basic_string& other);
        constexpr basic_string(const basic_string& other, const Allocator& alloc);
        constexpr basic_string(basic_string&& other) noexcept;
        constexpr basic_string(basic_string&& other, const Allocator& alloc);

        basic_string(std::nullptr_t) = delete;

        constexpr ~basic_string();

        constexpr basic_string& operator=(const basic_string& other);
        constexpr basic_string& operator=(basic_string&& other) noexcept(is_noexecpt_movable);

        constexpr basic_string& operator=(const value_type* s);
        constexpr basic_string& operator=(value_type ch);

        basic_string& operator=(std::nullptr_t) = delete;

        constexpr basic_string& assign(size_type count, value_type ch);
        constexpr basic_string& assign(const basic_string& str);
        constexpr basic_string& assign(const basic_string& str, size_type pos, size_type count = npos);
        constexpr basic_string& assign(basic_string&& str) noexcept(is_noexecpt_movable);
        constexpr basic_string& assign(const value_type* s, size_type count);
        constexpr basic_string& assign(const value_type* s);

        template <std::input_iterator InputIt>
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

        template <std::input_iterator InputIt>
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

        template <std::input_iterator InputIt>
        constexpr basic_string& append(InputIt first, InputIt last);

        constexpr basic_string& operator+=(const basic_string& str);
        constexpr basic_string& operator+=(value_type ch);
        constexpr basic_string& operator+=(const value_type* s);

        constexpr basic_string& replace(const_iterator first, const_iterator last, const basic_string& str);
        constexpr basic_string& replace(const_iterator first, const_iterator last, const value_type* s,
                                        size_type s_count);
        constexpr basic_string& replace(const_iterator first, const_iterator last, const value_type* s);
        constexpr basic_string& replace(const_iterator first, const_iterator last, size_type s_count, value_type ch);

        template <std::input_iterator InputIt>
        constexpr basic_string& replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2);

        constexpr void resize(size_type count);
        constexpr void resize(size_type count, value_type ch);

        constexpr void swap(basic_string& other) noexcept(is_noexecpt_movable);

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
        constexpr void _emplace_remaining_small_capacity(std::size_t size) noexcept;
        constexpr std::size_t _small_string_capacity() const noexcept;
        constexpr std::size_t _large_string_capacity() const noexcept;
        constexpr std::size_t _small_string_size() const noexcept;
        constexpr std::size_t _large_string_size() const noexcept;
        constexpr void _set_capacity(std::size_t cap) noexcept;
        constexpr std::size_t _aligned_large_string_allocation(std::size_t requested) const noexcept;

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
    template <std::input_iterator InputIt>
    inline constexpr basic_string<CharT, Traits, Allocator>::basic_string(InputIt first, InputIt last,
                                                                          const Allocator& alloc)
        : _alloc{alloc}
    {
        assign(first, last);
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
        : basic_string{std::move(other), other._alloc}
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
            _alloc = std::move(other._alloc);
        }

        assign(std::move(other));
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
    template <std::input_iterator InputIt>
    inline constexpr basic_string<CharT, Traits, Allocator>& basic_string<CharT, Traits, Allocator>::assign(
        InputIt first, InputIt last)
    {
        auto count = std::distance(first, last);
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
    template <std::input_iterator InputIt>
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
    template <std::input_iterator InputIt>
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
        auto erase_count = last - first;
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
        auto erase_count = last - first;
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
    template <std::input_iterator InputIt>
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
        std::size_t size) noexcept
    {
        _storage.small.data[size] = value_type();

        // Shift the remaining capacity 3 bits left
        auto remaining_capacity = small_string_capacity - size - 1;
        _storage.small.data[small_string_capacity - 1] = remaining_capacity;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr std::size_t basic_string<CharT, Traits, Allocator>::_small_string_capacity() const noexcept
    {
        return small_string_capacity - 1;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr std::size_t basic_string<CharT, Traits, Allocator>::_large_string_capacity() const noexcept
    {
        auto cap = _storage.large.capacity;
        // Mask off the top 3 bits of size_t
        constexpr std::size_t mask_bits = 0b111;
        constexpr std::size_t mask = mask_bits << (sizeof(std::size_t) * 8 - 4);

        return cap & ~mask;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr std::size_t basic_string<CharT, Traits, Allocator>::_small_string_size() const noexcept
    {
        auto last_small_value =
            static_cast<std::make_unsigned_t<CharT>>(_storage.small.data[small_string_capacity - 1]) & ~0xE0;
        // shift the size 3 bits right
        return small_string_capacity - 1 - last_small_value;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr std::size_t basic_string<CharT, Traits, Allocator>::_large_string_size() const noexcept
    {
        return _storage.large.size;
    }

    template <typename CharT, typename Traits, typename Allocator>
    inline constexpr void basic_string<CharT, Traits, Allocator>::_set_capacity(std::size_t cap) noexcept
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
    inline constexpr std::size_t basic_string<CharT, Traits, Allocator>::_aligned_large_string_allocation(
        std::size_t requested) const noexcept
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

    namespace detail
    {
        template <typename CharT, typename Traits>
        typename Traits::int_type bad_character_heuristic(const CharT* str, std::size_t size,
                                                          core::span<typename Traits::int_type> table)
        {
            for (auto& entry : table)
            {
                entry = static_cast<typename Traits::int_type>(-1);
            }

            auto min_el = std::min_element(str, str + size);
            auto min_val = *min_el;

            for (std::size_t i = 0; i < size; ++i)
            {
                table[Traits::to_int_type(str[i]) - Traits::to_int_type(min_val)] = i;
            }

            return min_val;
        }

        template <typename CharT, typename Traits>
        typename Traits::int_type reverse_bad_character_heuristic(const CharT* str, std::size_t size,
                                                                  core::span<typename Traits::int_type> table)
        {
            for (auto& entry : table)
            {
                entry = static_cast<typename Traits::int_type>(-1);
            }

            auto min_el = std::min_element(str, str + size);
            auto min_val = *min_el;

            for (std::size_t i = size; i > 0; --i)
            {
                auto it = i - 1;
                table[Traits::to_int_type(str[it]) - Traits::to_int_type(min_val)] = it;
            }

            return min_val;
        }

        template <typename CharT, typename Traits>
        const CharT* boyer_moore_helper(const CharT* str, std::size_t str_len, const CharT* pattern,
                                        std::size_t pattern_len, core::span<typename Traits::int_type> bad_char_table)
        {
            auto min_value = bad_character_heuristic<CharT, Traits>(pattern, pattern_len, bad_char_table);

            for (int s = 0; s <= (str_len - pattern_len);)
            {
                int p = pattern_len - 1;

                while (p >= 0 && pattern[p] == str[s + p])
                {
                    --p;
                }

                if (p < 0)
                {
                    return str + s;
                }

                s += std::max(1, p - bad_char_table[Traits::to_int_type(str[s + p]) - min_value]);
            }

            return str + str_len;
        }

        template <typename CharT, typename Traits>
        const CharT* reverse_boyer_more_helper(const CharT* str, std::size_t str_len, const CharT* pattern,
                                               std::size_t pattern_len,
                                               core::span<typename Traits::int_type> bad_char_table)
        {
            auto min_value = reverse_bad_character_heuristic<CharT, Traits>(pattern, pattern_len, bad_char_table);

            for (int s = str_len - pattern_len; s >= 0;)
            {
                int p = 0;

                while (p < pattern_len && pattern[p] == str[s + p])
                {
                    ++p;
                }

                if (p == pattern_len)
                {
                    return str + s;
                }

                s -= std::max(1, p - bad_char_table[Traits::to_int_type(str[s + p]) - min_value]);
            }

            return str + str_len;
        }

        template <typename CharT, typename Traits>
        const CharT* boyer_moore(const CharT* str, std::size_t str_len, const CharT* pattern, std::size_t pattern_len)
        {
            if constexpr (sizeof(CharT) == 1)
            {
                std::array<typename Traits::int_type, 256> bad_char_table;
                return boyer_moore_helper<CharT, Traits>(str, str_len, pattern, pattern_len, bad_char_table);
            }
            else
            {
                // Too large for stack, allocate on heap
                // Get the minimum and maximum values of the pattern
                auto [min_el, max_el] = std::minmax_element(pattern, pattern + pattern_len);

                core::vector<typename Traits::int_type> bad_char_table(*max_el - *min_el);
                return boyer_moore_helper<CharT, Traits>(str, str_len, pattern, pattern_len, bad_char_table);
            }
        }

        template <typename CharT, typename Traits>
        const CharT* reverse_boyer_moore(const CharT* str, std::size_t str_len, const CharT* pattern,
                                         std::size_t pattern_len)
        {
            if constexpr (sizeof(CharT) == 1)
            {
                std::array<typename Traits::int_type, 256> bad_char_table;
                return reverse_boyer_more_helper<CharT, Traits>(str, str_len, pattern, pattern_len, bad_char_table);
            }
            else
            {
                // Too large for stack, allocate on heap
                // Get the minimum and maximum values of the pattern
                auto [min_el, max_el] = std::minmax_element(pattern, pattern + pattern_len);

                core::vector<typename Traits::int_type> bad_char_table(*max_el - *min_el);
                return reverse_boyer_more_helper<CharT, Traits>(str, str_len, pattern, pattern_len, bad_char_table);
            }
        }
    } // namespace detail

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

    template <typename It, character_type CharT>
    constexpr It search(It first, It last, CharT ch)
    {
        while (first != last && *first != ch)
        {
            ++first;
        }

        return first;
    }

    template <typename It, character_type CharT>
    constexpr It search(It first, It last, const CharT* s, std::size_t count)
    {
        using char_t = typename std::iterator_traits<It>::value_type;
        using traits_t = char_traits<char_t>;

        auto first_char_ptr = detail::boyer_moore<char_t, traits_t>(&(*first), last - first, s, count);
        const auto distance_to_first = first_char_ptr - &(*first);
        return std::next(first, distance_to_first);
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr It search(It first, It last, It2 p_first, It2 p_last)
    {
        using char_t = typename std::iterator_traits<It>::value_type;
        using traits_t = char_traits<char_t>;

        auto first_char_ptr =
            detail::boyer_moore<char_t, traits_t>(&(*first), last - first, &(*p_first), p_last - p_first);
        const auto distance_to_first = first_char_ptr - &(*first);
        return std::next(first, distance_to_first);
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr It search_first_of(It first, It last, It2 p_first, It2 p_last)
    {
        // Fast path for 8 bit characters
        if constexpr (sizeof(typename std::iterator_traits<It>::value_type) == 1)
        {
            // Create bit field for lookup
            std::uint64_t lut[4] = {0, 0, 0, 0}; // 256 bits, 0 initialized

            for (auto it = p_first; it != p_last; ++it)
            {
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                lut[idx] |= 1ULL << bit;
            }

            for (; first != last; ++first)
            {
                auto ch = *first;
                auto idx = ch / 64;
                auto bit = ch % 64;
                if (lut[idx] & (1ULL << bit))
                {
                    return first;
                }
            }

            return last;
        }

        // Fallback for non 8 bit characters
        for (; first != last; ++first)
        {
            for (auto it = p_first; it != p_last; ++it)
            {
                if (*first == *it)
                {
                    return first;
                }
            }
        }

        return last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_first_of(It first, It last, const CharT* s, std::size_t count)
    {
        return search_first_of(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_first_of(It first, It last, const CharT* s)
    {
        using traits = char_traits<CharT>;
        return search_first_of(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_first_of(It first, It last, CharT ch)
    {
        for (; first != last; ++first)
        {
            if (*first == ch)
            {
                return first;
            }
        }

        return last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It reverse_search(It first, It last, CharT ch)
    {
        for (auto it = last; it != first;)
        {
            --it;
            if (*it == ch)
            {
                return it;
            }
        }

        return last;
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr It reverse_search(It first, It last, It2 p_first, It2 p_last)
    {
        using char_t = typename std::iterator_traits<It>::value_type;
        using traits_t = char_traits<char_t>;

        auto last_char_ptr =
            detail::reverse_boyer_moore<char_t, traits_t>(&(*first), last - first, &(*p_first), p_last - p_first);
        const auto distance_to_last = last_char_ptr - &(*first);
        return std::next(first, distance_to_last);
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It reverse_search(It first, It last, const CharT* s, std::size_t count)
    {
        using char_t = typename std::iterator_traits<It>::value_type;
        using traits_t = char_traits<char_t>;

        auto last_char_ptr = detail::reverse_boyer_moore<char_t, traits_t>(&(*first), last - first, s, count);
        const auto distance_to_last = last_char_ptr - &(*first);
        return std::next(first, distance_to_last);
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It reverse_search(It first, It last, const CharT* s)
    {
        using traits = char_traits<CharT>;
        return reverse_search(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_last_of(It first, It last, CharT ch)
    {
        return reverse_search(first, last, ch);
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr It search_last_of(It first, It last, It2 p_first, It2 p_last)
    {
        // Fast path for 8 bit characters
        if constexpr (sizeof(typename std::iterator_traits<It>::value_type) == 1)
        {
            // Create bit field for lookup
            std::uint64_t lut[4] = {0, 0, 0, 0}; // 256 bits, 0 initialized

            for (auto it = p_first; it != p_last; ++it)
            {
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                lut[idx] |= 1ULL << bit;
            }

            for (auto it = last; it != first;)
            {
                --it;
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                if (lut[idx] & (1ULL << bit))
                {
                    return it;
                }
            }

            return last;
        }

        // Fallback for non 8 bit characters
        for (auto it = last; it != first;)
        {
            --it;
            for (auto it2 = p_first; it2 != p_last; ++it2)
            {
                if (*it == *it2)
                {
                    return it;
                }
            }
        }

        return last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_last_of(It first, It last, const CharT* s, std::size_t count)
    {
        return search_last_of(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_last_of(It first, It last, const CharT* s)
    {
        using traits = char_traits<CharT>;
        return search_last_of(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_first_not_of(It first, It last, CharT ch)
    {
        for (; first != last; ++first)
        {
            if (*first != ch)
            {
                return first;
            }
        }

        return last;
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr It search_first_not_of(It first, It last, It2 p_first, It2 p_last)
    {
        // Fast path for 8 bit characters
        if constexpr (sizeof(typename std::iterator_traits<It>::value_type) == 1)
        {
            // Create bit field for lookup
            std::uint64_t lut[4] = {0, 0, 0, 0}; // 256 bits, 0 initialized

            for (auto it = p_first; it != p_last; ++it)
            {
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                lut[idx] |= 1ULL << bit;
            }

            for (; first != last; ++first)
            {
                auto ch = *first;
                auto idx = ch / 64;
                auto bit = ch % 64;
                if (!(lut[idx] & (1ULL << bit)))
                {
                    return first;
                }
            }

            return last;
        }

        // Fallback for non 8 bit characters
        for (; first != last; ++first)
        {
            bool found = false;
            for (auto it = p_first; it != p_last; ++it)
            {
                if (*first == *it)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                return first;
            }
        }

        return last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_first_not_of(It first, It last, const CharT* s)
    {
        using traits = char_traits<CharT>;
        return search_first_not_of(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_first_not_of(It first, It last, const CharT* s, std::size_t count)
    {
        return search_first_not_of(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_last_not_of(It first, It last, CharT ch)
    {
        for (auto it = last; it != first;)
        {
            --it;
            if (*it != ch)
            {
                return it;
            }
        }

        return last;
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr It search_last_not_of(It first, It last, It2 p_first, It2 p_last)
    {
        // Fast path for 8 bit characters
        if constexpr (sizeof(typename std::iterator_traits<It>::value_type) == 1)
        {
            // Create bit field for lookup
            std::uint64_t lut[4] = {0, 0, 0, 0}; // 256 bits, 0 initialized

            for (auto it = p_first; it != p_last; ++it)
            {
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                lut[idx] |= 1ULL << bit;
            }

            for (auto it = last; it != first;)
            {
                --it;
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                if (!(lut[idx] & (1ULL << bit)))
                {
                    return it;
                }
            }

            return last;
        }

        // Fallback for non 8 bit characters
        for (auto it = last; it != first;)
        {
            --it;
            bool found = false;
            for (auto it2 = p_first; it2 != p_last; ++it2)
            {
                if (*it == *it2)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                return it;
            }
        }

        return last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_last_not_of(It first, It last, const CharT* s, std::size_t count)
    {
        return search_last_not_of(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr It search_last_not_of(It first, It last, const CharT* s)
    {
        using traits = char_traits<CharT>;
        return search_last_not_of(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr bool starts_with(It first, It last, CharT ch)
    {
        return first != last && *first == ch;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr bool starts_with(It first, It last, const CharT* s)
    {
        using traits = char_traits<CharT>;
        auto len = traits::length(s);

        return starts_with(first, last, s, len);
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr bool starts_with(It first, It last, It2 p_first, It2 p_last)
    {
        while (first != last && p_first != p_last)
        {
            if (*first != *p_first)
            {
                return false;
            }

            ++first;
            ++p_first;
        }

        return p_first == p_last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr bool starts_with(It first, It last, const CharT* s, std::size_t count)
    {
        return starts_with(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr bool ends_with(It first, It last, CharT ch)
    {
        if (first == last)
        {
            return false;
        }

        auto it = last;
        --it;
        return *it == ch;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr bool ends_with(It first, It last, const CharT* s)
    {
        using traits = char_traits<CharT>;
        auto len = traits::length(s);

        return ends_with(first, last, s, len);
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr bool ends_with(It first, It last, It2 p_first, It2 p_last)
    {
        if (std::distance(first, last) < std::distance(p_first, p_last))
        {
            return false;
        }

        auto it = last;
        auto p_it = p_last;

        while (p_it != p_first)
        {
            --it;
            --p_it;

            if (*it != *p_it)
            {
                return false;
            }
        }

        return true;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr bool ends_with(It first, It last, const CharT* s, std::size_t count)
    {
        return ends_with(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr bool contains(It first, It last, CharT ch)
    {
        return search(first, last, ch) != last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr bool contains(It first, It last, const CharT* s)
    {
        using traits = char_traits<CharT>;
        auto len = traits::length(s);

        return search(first, last, s, len) != last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr bool contains(It first, It last, const CharT* s, std::size_t count)
    {
        return search(first, last, s, s + count) != last;
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr bool contains(It first, It last, It2 p_first, It2 p_last)
    {
        return search(first, last, p_first, p_last) != last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr int compare(It first1, It last1, const CharT* s)
    {
        using traits = char_traits<CharT>;
        auto len = traits::length(s);

        return compare(first1, last1, s, s + len);
    }

    template <typename It, character_type CharT>
        requires character_type<typename std::iterator_traits<It>::value_type>
    constexpr int compare(It first1, It last1, const CharT* s, std::size_t count)
    {
        return compare(first1, last1, s, s + count);
    }

    template <typename It, typename It2>
        requires character_type<typename std::iterator_traits<It>::value_type> &&
                 character_type<typename std::iterator_traits<It2>::value_type>
    constexpr int compare(It first1, It last1, It2 first2, It2 last2)
    {
        while (first1 != last1 && first2 != last2)
        {
            if (*first1 < *first2)
            {
                return -1;
            }

            if (*first1 > *first2)
            {
                return 1;
            }

            ++first1;
            ++first2;
        }

        if (first1 == last1 && first2 == last2)
        {
            return 0;
        }

        return first1 == last1 ? -1 : 1;
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search(const basic_string<CharT, Traits, Allocator>& str, CharT ch)
    {
        return search(str.begin(), str.end(), ch);
    }

    template <typename CharT, typename Traits, typename Allocator>
    constexpr auto search(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, std::size_t count)
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
    constexpr auto reverse_search(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, std::size_t count)
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
    constexpr auto search_first_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, std::size_t count)
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
    constexpr auto search_last_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, std::size_t count)
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
    constexpr auto search_first_not_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s,
                                       std::size_t count)
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
    constexpr auto search_last_not_of(const basic_string<CharT, Traits, Allocator>& str, const CharT* s,
                                      std::size_t count)
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
    constexpr bool starts_with(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, std::size_t count)
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
    constexpr bool ends_with(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, std::size_t count)
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
    constexpr bool contains(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, std::size_t count)
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
    constexpr int compare(const basic_string<CharT, Traits, Allocator>& str, const CharT* s, std::size_t count)
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
} // namespace tempest::core

#endif // tempest_core_string_hpp
