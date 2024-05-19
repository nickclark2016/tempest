#ifndef tempest_ecs_sparse_hpp
#define tempest_ecs_sparse_hpp

#include "traits.hpp"

#include <tempest/algorithm.hpp>
#include <tempest/span.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace tempest::ecs
{
    namespace detail
    {
        template <typename T>
        struct basic_sparse_set_iterator
        {
            using value_type = T;
            using pointer = const T*;
            using const_pointer = const T*;
            using reference = const T&;
            using const_reference = const T&;
            using difference_type = std::ptrdiff_t;
            using iterator_cateogry = std::random_access_iterator_tag;

            constexpr basic_sparse_set_iterator() noexcept;
            constexpr basic_sparse_set_iterator(core::span<const T> data, difference_type idx) noexcept;
            constexpr basic_sparse_set_iterator& operator++() noexcept;
            constexpr basic_sparse_set_iterator operator++(int) noexcept;
            constexpr basic_sparse_set_iterator& operator--() noexcept;
            constexpr basic_sparse_set_iterator& operator--(int) noexcept;
            constexpr basic_sparse_set_iterator& operator+=(difference_type diff) noexcept;
            constexpr basic_sparse_set_iterator& operator-=(difference_type diff) noexcept;
            [[nodiscard]] constexpr basic_sparse_set_iterator operator+(difference_type diff) const noexcept;
            [[nodiscard]] constexpr basic_sparse_set_iterator operator-(difference_type diff) const noexcept;
            [[nodiscard]] constexpr const_reference operator[](difference_type diff) const noexcept;
            [[nodiscard]] constexpr const_pointer operator->() const noexcept;
            [[nodiscard]] constexpr const_reference operator*() const noexcept;
            [[nodiscard]] constexpr const_pointer data() const noexcept;
            [[nodiscard]] constexpr difference_type get_index() const noexcept;

            core::span<const T> packed;
            difference_type offset;
        };

        template <typename K, typename V>
        struct basic_sparse_map_iterator_ptr
        {
            std::pair<K, V> val;

            [[nodiscard]] constexpr std::pair<K, V>* operator->() noexcept;
            [[nodiscard]] constexpr const std::pair<K, V>* operator->() const noexcept;
        };

        template <typename K, typename V>
        struct basic_sparse_map_iterator
        {
            using value_type = std::pair<K, V>;
            using reference =
                std::pair<std::add_lvalue_reference_t<std::add_const_t<K>>, std::add_lvalue_reference_t<V>>;
            using const_reference = std::pair<std::add_lvalue_reference_t<std::add_const_t<K>>,
                                              std::add_lvalue_reference_t<std::add_const_t<V>>>;
            using pointer = basic_sparse_map_iterator_ptr<const K, V>;
            using const_pointer = basic_sparse_map_iterator_ptr<const K, const V>;
            using difference_type = std::ptrdiff_t;
            using iterator_cateogry = std::random_access_iterator_tag;

            constexpr basic_sparse_map_iterator() noexcept;
            constexpr basic_sparse_map_iterator(core::span<const K> keys, core::span<V> values,
                                                difference_type idx) noexcept;
            constexpr basic_sparse_map_iterator& operator++() noexcept;
            constexpr basic_sparse_map_iterator operator++(int) noexcept;
            constexpr basic_sparse_map_iterator& operator--() noexcept;
            constexpr basic_sparse_map_iterator operator--(int) noexcept;
            constexpr basic_sparse_map_iterator& operator+=(difference_type diff) noexcept;
            constexpr basic_sparse_map_iterator& operator-=(difference_type diff) noexcept;
            [[nodiscard]] constexpr basic_sparse_map_iterator operator+(difference_type diff) const noexcept;
            [[nodiscard]] constexpr basic_sparse_map_iterator operator-(difference_type diff) const noexcept;
            [[nodiscard]] constexpr reference operator[](difference_type diff) noexcept;
            [[nodiscard]] constexpr const_reference operator[](difference_type diff) const noexcept;
            [[nodiscard]] constexpr pointer operator->() noexcept;
            [[nodiscard]] constexpr const_pointer operator->() const noexcept;
            [[nodiscard]] constexpr reference operator*() noexcept;
            [[nodiscard]] constexpr const_reference operator*() const noexcept;
            [[nodiscard]] constexpr difference_type get_index() const noexcept;

            core::span<const K> keys;
            core::span<V> values;
            difference_type offset;
        };

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>::basic_sparse_set_iterator() noexcept : packed{}, offset{}
        {
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>::basic_sparse_set_iterator(core::span<const T> data,
                                                                                 difference_type idx) noexcept
            : packed{data}, offset{idx}
        {
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>& basic_sparse_set_iterator<T>::operator++() noexcept
        {
            --offset;
            return *this;
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T> basic_sparse_set_iterator<T>::operator++(int) noexcept
        {
            auto self = *this;
            --offset;
            return self;
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>& basic_sparse_set_iterator<T>::operator--() noexcept
        {
            ++offset;
            return *this;
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>& basic_sparse_set_iterator<T>::operator--(int) noexcept
        {
            auto self = *this;
            ++offset;
            return *this;
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>& basic_sparse_set_iterator<T>::operator+=(
            difference_type diff) noexcept
        {
            offset -= diff;
            return *this;
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>& basic_sparse_set_iterator<T>::operator-=(
            difference_type diff) noexcept
        {
            offset += diff;
            return *this;
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T> basic_sparse_set_iterator<T>::operator+(
            difference_type diff) const noexcept
        {
            auto copy = *this;
            copy += diff;
            return copy;
        }
        template <typename T>
        inline constexpr basic_sparse_set_iterator<T> basic_sparse_set_iterator<T>::operator-(
            difference_type diff) const noexcept
        {
            auto copy = *this;
            copy -= diff;
            return copy;
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>::const_reference basic_sparse_set_iterator<T>::operator[](
            difference_type diff) const noexcept
        {
            return packed[get_index() - diff];
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>::const_pointer basic_sparse_set_iterator<T>::operator->()
            const noexcept
        {
            return packed.data() + get_index();
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>::const_reference basic_sparse_set_iterator<T>::operator*()
            const noexcept
        {
            return packed[get_index()];
        }

        template <typename T>
        inline constexpr basic_sparse_set_iterator<T>::const_pointer basic_sparse_set_iterator<T>::data() const noexcept
        {
            return packed.data();
        }

        template <typename T>
        [[nodiscard]] inline constexpr basic_sparse_set_iterator<T>::difference_type basic_sparse_set_iterator<
            T>::get_index() const noexcept
        {
            return offset - 1;
        }

        template <typename T>
        [[nodiscard]] constexpr std::ptrdiff_t operator-(const basic_sparse_set_iterator<T>& lhs,
                                                         const basic_sparse_set_iterator<T>& rhs) noexcept
        {
            return lhs.get_index() - rhs.get_index();
        }

        template <typename T>
        [[nodiscard]] constexpr bool operator==(const basic_sparse_set_iterator<T>& lhs,
                                                const basic_sparse_set_iterator<T>& rhs) noexcept
        {
            return lhs.get_index() == rhs.get_index();
        }

        template <typename T>
        [[nodiscard]] constexpr bool operator<=>(const basic_sparse_set_iterator<T>& lhs,
                                                 const basic_sparse_set_iterator<T>& rhs) noexcept
        {
            return rhs.get_index() <=> lhs.get_index();
        }

        template <typename K, typename V>
        inline constexpr std::pair<K, V>* basic_sparse_map_iterator_ptr<K, V>::operator->() noexcept
        {
            return &val;
        }

        template <typename K, typename V>
        inline constexpr const std::pair<K, V>* basic_sparse_map_iterator_ptr<K, V>::operator->() const noexcept
        {
            return &val;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::basic_sparse_map_iterator() noexcept
            : keys{}, values{}, offset{}
        {
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::basic_sparse_map_iterator(core::span<const K> keys,
                                                                                    core::span<V> values,
                                                                                    difference_type idx) noexcept
            : keys{keys}, values{values}, offset{idx}
        {
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>& basic_sparse_map_iterator<K, V>::operator++() noexcept
        {
            --offset;
            return *this;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V> basic_sparse_map_iterator<K, V>::operator++(int) noexcept
        {
            auto self = *this;
            --offset;
            return self;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>& basic_sparse_map_iterator<K, V>::operator--() noexcept
        {
            ++offset;
            return *this;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V> basic_sparse_map_iterator<K, V>::operator--(int) noexcept
        {
            auto self = *this;
            ++offset;
            return *this;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>& basic_sparse_map_iterator<K, V>::operator+=(
            difference_type diff) noexcept
        {
            offset -= diff;
            return *this;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>& basic_sparse_map_iterator<K, V>::operator-=(
            difference_type diff) noexcept
        {
            offset += diff;
            return *this;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V> basic_sparse_map_iterator<K, V>::operator+(
            difference_type diff) const noexcept
        {
            auto self = *this;
            self += diff;
            return self;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V> basic_sparse_map_iterator<K, V>::operator-(
            difference_type diff) const noexcept
        {
            auto self = *this;
            self -= diff;
            return self;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::reference basic_sparse_map_iterator<K, V>::operator[](
            difference_type diff) noexcept
        {
            return std::make_tuple(keys[get_index()], values[get_index()]);
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::const_reference basic_sparse_map_iterator<K, V>::operator[](
            difference_type diff) const noexcept
        {
            return std::make_tuple(keys[get_index()], values[get_index()]);
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::pointer basic_sparse_map_iterator<K, V>::operator->() noexcept
        {
            return pointer{
                .val = std::make_pair(keys[get_index()], values[get_index()]),
            };
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::const_pointer basic_sparse_map_iterator<K, V>::operator->()
            const noexcept
        {
            return const_pointer{
                .val = std::make_pair(keys[get_index()], values[get_index()]),
            };
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::reference basic_sparse_map_iterator<K,
                                                                                              V>::operator*() noexcept
        {
            return std::make_pair(std::cref(keys[get_index()]), std::ref(values[get_index()]));
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::const_reference basic_sparse_map_iterator<K, V>::operator*()
            const noexcept
        {
            return std::make_pair(std::cref(keys[get_index()]), std::cref(values[get_index()]));
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::difference_type basic_sparse_map_iterator<K, V>::get_index()
            const noexcept
        {
            return offset - 1;
        }

        template <typename K, typename V>
        inline constexpr basic_sparse_map_iterator<K, V>::difference_type operator-(
            const basic_sparse_map_iterator<K, V>& lhs, const basic_sparse_map_iterator<K, V>& rhs) noexcept
        {
            return lhs.get_index() - rhs.get_index();
        }

        template <typename K, typename V>
        inline constexpr bool operator==(const basic_sparse_map_iterator<K, V>& lhs,
                                         const basic_sparse_map_iterator<K, V>& rhs) noexcept
        {
            return lhs.get_index() == rhs.get_index();
        }

        template <typename K, typename V1, typename V2>
        inline constexpr bool operator==(const basic_sparse_map_iterator<K, V1>& lhs,
                                         const basic_sparse_map_iterator<K, V2>& rhs) noexcept
        {
            static_assert(std::is_same_v<std::remove_cvref_t<V1>, std::remove_cvref_t<V2>>,
                          "V1 and V2 must have the same type with topmost CV-qualifiers and reference removed");
            return lhs.get_index() == rhs.get_index();
        }

        template <typename K, typename V>
        inline constexpr auto operator<=>(const basic_sparse_map_iterator<K, V>& lhs,
                                          const basic_sparse_map_iterator<K, V>& rhs) noexcept
        {
            return rhs.get_index() <=> lhs.get_index();
        }
    } // namespace detail

    template <typename T, typename Allocator = std::allocator<T>>
    class basic_sparse_set
    {
      public:
        using traits_type = entity_traits<T>;
        using entity_type = typename traits_type::value_type;
        using version_type = typename traits_type::version_type;

        using iterator = detail::basic_sparse_set_iterator<T>;
        using const_iterator = iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        using size_type = std::size_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;

        constexpr basic_sparse_set() = default;
        constexpr basic_sparse_set(const basic_sparse_set& rhs) noexcept;
        constexpr basic_sparse_set(basic_sparse_set&& rhs) noexcept;
        constexpr ~basic_sparse_set();

        constexpr basic_sparse_set& operator=(const basic_sparse_set& rhs);
        constexpr basic_sparse_set& operator=(basic_sparse_set&& rhs) noexcept;

        constexpr std::size_t size() const noexcept;
        constexpr std::size_t capacity() const noexcept;
        constexpr bool empty() const noexcept;
        constexpr bool contains(T value) const noexcept;
        constexpr const_iterator find(T value) const noexcept;
        constexpr pointer data() const noexcept;
        constexpr entity_type at(size_type idx) const noexcept;

        constexpr entity_type operator[](size_type idx) const noexcept;

        constexpr iterator begin() const noexcept;
        constexpr const_iterator cbegin() const noexcept;
        constexpr iterator end() const noexcept;
        constexpr const_iterator cend() const noexcept;
        constexpr reverse_iterator rbegin() const noexcept;
        constexpr const_reverse_iterator crbegin() const noexcept;
        constexpr reverse_iterator rend() const noexcept;
        constexpr const_reverse_iterator crend() const noexcept;

        constexpr iterator insert(T value);
        constexpr void erase(iterator it);
        constexpr void clear();

      private:
        T** _sparse{nullptr};
        T* _packed{nullptr};

        std::size_t _sparse_page_count{0};
        std::size_t _packed_count{0};
        std::size_t _packed_capacity{0};

        using alloc_traits = std::allocator_traits<Allocator>;
        using packed_alloc_type = Allocator;
        using packed_alloc_traits = alloc_traits;
        using sparse_alloc_type = typename alloc_traits::template rebind_alloc<typename alloc_traits::pointer>;
        using sparse_alloc_traits = std::allocator_traits<sparse_alloc_type>;

        [[no_unique_address]] packed_alloc_type _packed_alloc;
        [[no_unique_address]] sparse_alloc_type _sparse_alloc;

        std::size_t _free_list_head{traits_type::entity_mask};

        constexpr void _release_resources();
        constexpr void _release_sparse_resources();
        constexpr void _release_packed_resources();
        constexpr void _request_storage_resize(size_type sz);
        constexpr auto& _assure(T value);
        constexpr auto _to_iterator(T value) const noexcept;
        constexpr size_type _index(T value) const noexcept;
        constexpr auto& _sparse_reference(T value) const noexcept;
        constexpr auto _sparse_pointer(T value) const noexcept;
    };

    template <typename T>
    class basic_sparse_map_interface
    {
      public:
        constexpr basic_sparse_map_interface() noexcept = default;
        constexpr basic_sparse_map_interface(const basic_sparse_map_interface&) = default;
        constexpr basic_sparse_map_interface(basic_sparse_map_interface&&) noexcept = default;
        constexpr virtual ~basic_sparse_map_interface() = default;

        constexpr basic_sparse_map_interface& operator=(const basic_sparse_map_interface&) = default;
        constexpr basic_sparse_map_interface& operator=(basic_sparse_map_interface&&) noexcept = default;

        [[nodiscard]] virtual constexpr std::size_t size() const noexcept = 0;
        [[nodiscard]] virtual constexpr std::size_t capacity() const noexcept = 0;
        [[nodiscard]] virtual constexpr bool contains(T t) const noexcept = 0;

        virtual void reserve(std::size_t new_capacity) = 0;

        virtual constexpr void erase(T t) noexcept = 0;
    };

    template <typename K, typename V, typename Allocator = std::allocator<K>>
    class basic_sparse_map : public basic_sparse_map_interface<K>
    {
      public:
        using traits_type = entity_traits<K>;
        using entity_type = typename traits_type::value_type;
        using version_type = typename traits_type::version_type;

        using key_type = K;
        using mapped_type = V;
        using value_type = std::pair<std::add_const_t<K>, V>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = std::pair<typename std::add_const_t<typename std::add_lvalue_reference_t<K>>,
                                    typename std::add_lvalue_reference_t<V>>;
        using const_reference = std::pair<typename std::add_const_t<typename std::add_lvalue_reference_t<K>>,
                                          typename std::add_const_t<typename std::add_lvalue_reference_t<V>>>;
        using pointer =
            detail::basic_sparse_map_iterator_ptr<typename reference::first_type, typename reference::second_type>;
        using const_pointer = detail::basic_sparse_map_iterator_ptr<typename const_reference::first_type,
                                                                    typename const_reference::second_type>;
        using iterator = detail::basic_sparse_map_iterator<K, V>;
        using const_iterator = detail::basic_sparse_map_iterator<K, const V>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr basic_sparse_map() noexcept;
        constexpr basic_sparse_map(const basic_sparse_map& rhs);
        constexpr basic_sparse_map(basic_sparse_map&& rhs) noexcept;
        constexpr ~basic_sparse_map() override;

        constexpr basic_sparse_map& operator=(const basic_sparse_map& rhs);
        constexpr basic_sparse_map& operator=(basic_sparse_map&& rhs) noexcept;

        [[nodiscard]] constexpr size_type size() const noexcept override;
        [[nodiscard]] constexpr size_type capacity() const noexcept override;
        [[nodiscard]] constexpr bool empty() const noexcept;
        [[nodiscard]] constexpr bool contains(K k) const noexcept override;
        [[nodiscard]] constexpr bool contains(K k, const V& v) const noexcept;
        [[nodiscard]] constexpr const_iterator find(K k) const noexcept;
        [[nodiscard]] constexpr size_type index_of(K k) const noexcept;

        [[nodiscard]] constexpr mapped_type& operator[](K k) noexcept;
        [[nodiscard]] constexpr const mapped_type& operator[](K k) const noexcept;

        [[nodiscard]] constexpr iterator begin() noexcept;
        [[nodiscard]] constexpr const_iterator begin() const noexcept;
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept;
        [[nodiscard]] constexpr iterator end() noexcept;
        [[nodiscard]] constexpr const_iterator end() const noexcept;
        [[nodiscard]] constexpr const_iterator cend() const noexcept;

        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept;
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept;
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept;
        [[nodiscard]] constexpr reverse_iterator rend() noexcept;
        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept;
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept;

        constexpr void erase(K k) noexcept override;
        constexpr iterator insert(K k, const V& v);
        constexpr iterator insert(K k, V&& v);
        constexpr iterator insert_or_replace(K k, const V& v);
        constexpr void clear();

        template <typename... Ts>
        constexpr iterator emplace(K k, Ts&&... ts);

        template <typename... Ts>
        constexpr iterator emplace_or_replace(K k, Ts&&... ts);

        void reserve(std::size_t new_capacity) override;

        K* keys() const noexcept;
        V* values() const noexcept;

      private:
        constexpr void _release_resources();
        constexpr void _release_sparse_resources();
        constexpr void _release_packed_resources();
        constexpr void _request_storage_resize(size_type sz);
        constexpr auto& _assure(K value);
        constexpr auto _to_iterator(K value) const noexcept;
        constexpr size_type _index(K value) const noexcept;
        constexpr auto& _sparse_reference(K value) const noexcept;
        constexpr auto _sparse_pointer(K value) const noexcept;

        using alloc_traits = std::allocator_traits<Allocator>;
        using packed_alloc_type = Allocator;
        using packed_alloc_traits = alloc_traits;
        using sparse_alloc_type = typename alloc_traits::template rebind_alloc<typename alloc_traits::pointer>;
        using sparse_alloc_traits = std::allocator_traits<sparse_alloc_type>;
        using key_allocator_type = Allocator;
        using value_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<V>;
        using value_alloc_traits = std::allocator_traits<value_allocator_type>;

        [[no_unique_address]] packed_alloc_type _packed_alloc;
        [[no_unique_address]] sparse_alloc_type _sparse_alloc;
        [[no_unique_address]] value_allocator_type _value_alloc;

        K** _sparse{nullptr};
        K* _packed{nullptr};
        V* _values{nullptr};

        std::size_t _sparse_page_count{0};
        std::size_t _packed_count{0};
        std::size_t _packed_capacity{0};

        std::size_t _free_list_head{traits_type::entity_mask};
    };

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::basic_sparse_set(const basic_sparse_set& rhs) noexcept
    {
        _request_storage_resize(rhs._packed_capacity);
        std::uninitialized_copy_n(rhs._packed, rhs._packed_count, _packed);
        for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
        {
            std::copy_n(rhs._sparse[page_idx], traits_type::page_size, _sparse[page_idx]);
        }
        _free_list_head = rhs._free_list_head;
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::basic_sparse_set(basic_sparse_set&& rhs) noexcept
        : _sparse{std::exchange(rhs._sparse, nullptr)}, _packed{std::exchange(rhs._packed, nullptr)},
          _sparse_page_count{std::exchange(rhs._sparse_page_count, 0)},
          _packed_count{std::exchange(rhs._packed_count, 0)}, _packed_capacity{std::exchange(rhs._packed_capacity, 0)},
          _packed_alloc{std::move(rhs._packed_alloc)}, _sparse_alloc{std::move(rhs._sparse_alloc)},
          _free_list_head{std::exchange(rhs._free_list_head, traits_type::entity_mask)}
    {
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::~basic_sparse_set()
    {
        _release_resources();
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>& basic_sparse_set<T, Allocator>::operator=(
        const basic_sparse_set& rhs)
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release_resources();

        _request_storage_resize(rhs._packed_capacity);
        std::uninitialized_copy_n(rhs._packed, rhs._packed_count, _packed);
        for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
        {
            std::copy_n(rhs._sparse[page_idx], traits_type::page_size, _sparse[page_idx]);
        }

        return *this;
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>& basic_sparse_set<T, Allocator>::operator=(
        basic_sparse_set&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release_resources();

        std::swap(_sparse, rhs._sparse);
        std::swap(_packed, rhs._packed);
        std::swap(_sparse_page_count, rhs._sparse_page_count);
        std::swap(_packed_count, rhs._packed_count);
        std::swap(_packed_capacity, rhs._packed_capacity);
        std::swap(_packed_alloc, rhs._packed_alloc);
        std::swap(_sparse_alloc, rhs._sparse_alloc);

        return *this;
    }

    template <typename T, typename Allocator>
    inline constexpr std::size_t basic_sparse_set<T, Allocator>::size() const noexcept
    {
        return _packed_count;
    }

    template <typename T, typename Allocator>
    inline constexpr std::size_t basic_sparse_set<T, Allocator>::capacity() const noexcept
    {
        return _packed_capacity;
    }

    template <typename T, typename Allocator>
    inline constexpr bool basic_sparse_set<T, Allocator>::empty() const noexcept
    {
        return _packed_count == 0;
    }

    template <typename T, typename Allocator>
    inline constexpr bool basic_sparse_set<T, Allocator>::contains(T value) const noexcept
    {
        const auto element = _sparse_pointer(value);
        constexpr auto max_cap = traits_type::entity_mask;
        constexpr auto mask = traits_type::as_integral(null) & ~max_cap;
        return element && (((mask & traits_type::as_integral(value)) ^ traits_type::as_integral(*element)) < max_cap);
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::const_iterator basic_sparse_set<T, Allocator>::find(
        T value) const noexcept
    {
        return contains(value) ? _to_iterator(value) : cend();
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::pointer basic_sparse_set<T, Allocator>::data() const noexcept
    {
        return _packed;
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::entity_type basic_sparse_set<T, Allocator>::at(
        size_type idx) const noexcept
    {
        return idx < _packed_count ? _packed[idx] : null;
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::entity_type basic_sparse_set<T, Allocator>::operator[](
        size_type idx) const noexcept
    {
        return _packed[idx];
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::iterator basic_sparse_set<T, Allocator>::begin() const noexcept
    {
        const auto position = static_cast<typename iterator::difference_type>(_packed_count);
        return iterator{core::span<const T>(_packed, _packed_count), position};
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::const_iterator basic_sparse_set<T, Allocator>::cbegin()
        const noexcept
    {
        return begin();
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::iterator basic_sparse_set<T, Allocator>::end() const noexcept
    {
        return iterator{core::span<const T>(_packed, _packed_count), {}};
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::const_iterator basic_sparse_set<T, Allocator>::cend()
        const noexcept
    {
        return end();
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::reverse_iterator basic_sparse_set<T, Allocator>::rbegin()
        const noexcept
    {
        return std::make_reverse_iterator(end());
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::const_reverse_iterator basic_sparse_set<T, Allocator>::crbegin()
        const noexcept
    {
        return rbegin();
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::reverse_iterator basic_sparse_set<T, Allocator>::rend()
        const noexcept
    {
        return std::make_reverse_iterator(begin());
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::const_reverse_iterator basic_sparse_set<T, Allocator>::crend()
        const noexcept
    {
        return rend();
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::iterator basic_sparse_set<T, Allocator>::insert(T value)
    {
        auto& element = _assure(value);
        auto position = size();

        _packed[_packed_count++] = value;
        element = traits_type::combine_entities(static_cast<typename traits_type::entity_type>(_packed_count - 1),
                                                traits_type::as_integral(value));

        return --(end() - position);
    }

    template <typename T, typename Allocator>
    inline constexpr void basic_sparse_set<T, Allocator>::erase(iterator it)
    {
        auto& self = _sparse_reference(*it);
        const auto e = traits_type::as_entity(self);
        _sparse_reference(_packed[_packed_count - 1]) =
            traits_type::combine_entities(e, traits_type::as_integral(_packed[_packed_count - 1]));

        auto requested_erase = static_cast<size_type>(e);
        if (requested_erase != _packed_count - 1)
        {
            _packed[static_cast<size_type>(e)] = std::move(_packed[_packed_count - 1]);
        }

        std::destroy_at(_packed + _packed_count - 1);

        self = null;
        --_packed_count;
    }

    template <typename T, typename Allocator>
    inline constexpr void basic_sparse_set<T, Allocator>::clear()
    {
        for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
        {
            std::fill_n(_sparse[page_idx], traits_type::page_size, null);
        }
        std::destroy_n(_packed, _packed_count);
        _packed_count = 0;
    }

    template <typename T, typename Allocator>
    inline constexpr void basic_sparse_set<T, Allocator>::_release_resources()
    {
        _release_sparse_resources();
        _release_packed_resources();

        _packed_count = 0;
        _packed_capacity = 0;
        _sparse_page_count = 0;
    }

    template <typename T, typename Allocator>
    inline constexpr void basic_sparse_set<T, Allocator>::_release_sparse_resources()
    {
        if (_sparse != nullptr) [[likely]]
        {
            for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
            {
                std::destroy_n(_sparse[page_idx], traits_type::page_size);
                packed_alloc_traits::deallocate(_packed_alloc, _sparse[page_idx], traits_type::page_size);
            }
            std::destroy_n(_sparse, _sparse_page_count);
            sparse_alloc_traits::deallocate(_sparse_alloc, _sparse, _sparse_page_count);
            _sparse = nullptr;
        }
    }

    template <typename T, typename Allocator>
    inline constexpr void basic_sparse_set<T, Allocator>::_release_packed_resources()
    {
        if (_packed) [[likely]]
        {
            std::destroy_n(_packed, _packed_count);
            packed_alloc_traits::deallocate(_packed_alloc, _packed, _packed_capacity);
            _packed = nullptr;
        }
    }

    template <typename T, typename Allocator>
    inline constexpr void basic_sparse_set<T, Allocator>::_request_storage_resize(size_type sz)
    {
        if (sz == 0) [[unlikely]]
        {
            return;
        }

        const auto aligned_request = std::bit_ceil(sz);
        const auto new_page_count = 1 + ((aligned_request - 1) / traits_type::page_size);

        // handle page count changes
        if (new_page_count > _sparse_page_count)
        {
            T** sparse_pages = sparse_alloc_traits::allocate(_sparse_alloc, new_page_count);
            for (std::size_t page_idx = 0; page_idx < new_page_count; ++page_idx)
            {
                T* page = packed_alloc_traits::allocate(_packed_alloc, traits_type::page_size);
                std::construct_at(sparse_pages + page_idx, page);
            }

            // copy existing pages
            for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
            {
                T* page = sparse_pages[page_idx];
                std::uninitialized_copy_n(_sparse[page_idx], traits_type::page_size, page);
            }

            // fill new pages
            for (std::size_t page_idx = _sparse_page_count; page_idx < new_page_count; ++page_idx)
            {
                constexpr entity_type null_init = null;
                std::uninitialized_fill_n(sparse_pages[page_idx], traits_type::page_size, null_init);
            }

            // release old pages
            _release_sparse_resources();

            // update sparse members to point at new values
            _sparse = sparse_pages;
            _sparse_page_count = new_page_count;
        }

        // allocate new buffer for packed values if needed
        if (_packed_count >= _packed_capacity)
        {
            const auto aligned_packed = std::bit_ceil(_packed_capacity + 1);

            T* packed_values = packed_alloc_traits::allocate(_packed_alloc, aligned_packed);
            std::uninitialized_copy_n(_packed, _packed_count, packed_values);

            // release old buffer
            _release_packed_resources();

            // update packed members to point at new values
            _packed = packed_values;
            _packed_capacity = aligned_packed;
        }
    }

    template <typename T, typename Allocator>
    inline constexpr auto& basic_sparse_set<T, Allocator>::_assure(T value)
    {
        const auto position = static_cast<size_type>(traits_type::as_entity(value));
        const auto page = position / traits_type::page_size;

        _request_storage_resize(position + 1);

        return _sparse[page][core::fast_mod(position, traits_type::page_size)];
    }

    template <typename T, typename Allocator>
    inline constexpr auto basic_sparse_set<T, Allocator>::_to_iterator(T value) const noexcept
    {
        return --(end() - _index(value));
    }

    template <typename T, typename Allocator>
    inline constexpr basic_sparse_set<T, Allocator>::size_type basic_sparse_set<T, Allocator>::_index(
        T value) const noexcept
    {
        return static_cast<size_type>(traits_type::as_entity(_sparse_reference(value)));
    }

    template <typename T, typename Allocator>
    inline constexpr auto& basic_sparse_set<T, Allocator>::_sparse_reference(T value) const noexcept
    {
        const auto position = static_cast<size_type>(traits_type::as_entity(value));
        return _sparse[position / traits_type::page_size][core::fast_mod(position, traits_type::page_size)];
    }

    template <typename T, typename Allocator>
    inline constexpr auto basic_sparse_set<T, Allocator>::_sparse_pointer(T value) const noexcept
    {
        const auto position = static_cast<size_type>(traits_type::as_entity(value));
        const auto page = position / traits_type::page_size;
        return (page < _sparse_page_count && _sparse[page])
                   ? (_sparse[page] + core::fast_mod(position, traits_type::page_size))
                   : nullptr;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::basic_sparse_map() noexcept : basic_sparse_map_interface<K>{}
    {
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::basic_sparse_map(const basic_sparse_map& rhs)
        : basic_sparse_map_interface<K>{}
    {
        _request_storage_resize(rhs._packed_capacity);

        std::uninitialized_copy_n(rhs._packed, rhs._packed_count, _packed);
        std::uninitialized_copy_n(rhs._values, rhs._packed_count, _values);

        for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
        {
            std::copy_n(rhs._sparse[page_idx], traits_type::page_size, _sparse[page_idx]);
        }

        _free_list_head = rhs._free_list_head;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::basic_sparse_map(basic_sparse_map&& rhs) noexcept
        : basic_sparse_map_interface<K>{}, _sparse{std::exchange(rhs._sparse, nullptr)},
          _packed{std::exchange(rhs._packed, nullptr)}, _values{std::exchange(rhs._values, nullptr)},
          _sparse_page_count{std::exchange(rhs._sparse_page_count, 0)},
          _packed_count{std::exchange(rhs._packed_count, 0)}, _packed_capacity{std::exchange(rhs._packed_capacity, 0)},
          _packed_alloc{std::move(rhs._packed_alloc)}, _sparse_alloc{std::move(rhs._sparse_alloc)},
          _value_alloc{std::move(rhs._value_alloc)},
          _free_list_head{std::exchange(rhs._free_list_head, traits_type::entity_mask)}
    {
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::~basic_sparse_map()
    {
        _release_resources();
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>& basic_sparse_map<K, V, Allocator>::operator=(
        const basic_sparse_map& rhs)
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release_resources();

        _request_storage_resize(rhs._packed_capacity);

        std::uninitialized_copy_n(rhs._packed, rhs._packed_count, _packed);
        std::uninitialized_copy_n(rhs._values, rhs._packed_count, _values);

        for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
        {
            std::copy_n(rhs._sparse[page_idx], traits_type::page_size, _sparse[page_idx]);
        }

        return *this;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>& basic_sparse_map<K, V, Allocator>::operator=(
        basic_sparse_map&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release_resources();

        std::swap(_sparse, rhs._sparse);
        std::swap(_packed, rhs._packed);
        std::swap(_values, rhs._values);
        std::swap(_sparse_page_count, rhs._sparse_page_count);
        std::swap(_packed_count, rhs._packed_count);
        std::swap(_packed_capacity, rhs._packed_capacity);
        std::swap(_packed_alloc, rhs._packed_alloc);
        std::swap(_sparse_alloc, rhs._sparse_alloc);
        std::swap(_value_alloc, rhs._value_alloc);
        std::swap(_free_list_head, rhs._free_list_head);

        return *this;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::size_type basic_sparse_map<K, V, Allocator>::size()
        const noexcept
    {
        return _packed_count;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::size_type basic_sparse_map<K, V, Allocator>::capacity()
        const noexcept
    {
        return _packed_capacity;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr bool basic_sparse_map<K, V, Allocator>::empty() const noexcept
    {
        return size() == 0;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr bool basic_sparse_map<K, V, Allocator>::contains(K k) const noexcept
    {
        const auto element = _sparse_pointer(k);
        constexpr auto max_cap = traits_type::entity_mask;
        constexpr auto mask = traits_type::as_integral(null) & ~max_cap;
        return element && (((mask & traits_type::as_integral(k)) ^ traits_type::as_integral(*element)) < max_cap);
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr bool basic_sparse_map<K, V, Allocator>::contains(K k, const V& v) const noexcept
    {
        const auto element = _sparse_pointer(k);
        constexpr auto max_cap = traits_type::entity_mask;
        constexpr auto mask = traits_type::as_integral(null) & ~max_cap;
        if (element && (((mask & traits_type::as_integral(k)) ^ traits_type::as_integral(*element)) < max_cap))
        {
            return _values[traits_type::as_entity(*element)] == v;
        }
        return false;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::const_iterator basic_sparse_map<K, V, Allocator>::find(
        K k) const noexcept
    {
        return contains(k) ? _to_iterator(k) : cend();
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::size_type basic_sparse_map<K, V, Allocator>::index_of(
        K k) const noexcept
    {
        // Get the index of the key in the packed array
        const auto element = _sparse_pointer(k);
        constexpr auto max_cap = traits_type::entity_mask;
        constexpr auto mask = traits_type::as_integral(null) & ~max_cap;
        return static_cast<size_type>(traits_type::as_entity(*element));
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::mapped_type& basic_sparse_map<K, V, Allocator>::operator[](
        K k) noexcept
    {
        return _values[_index(k)];
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr const basic_sparse_map<K, V, Allocator>::mapped_type& basic_sparse_map<
        K, V, Allocator>::operator[](K k) const noexcept
    {
        return _values[_index(k)];
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::iterator basic_sparse_map<K, V, Allocator>::begin() noexcept
    {
        const auto position = static_cast<typename iterator::difference_type>(_packed_count);
        return iterator{core::span<const K>(_packed, _packed_count), core::span<V>(_values, _packed_count), position};
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::const_iterator basic_sparse_map<K, V, Allocator>::begin()
        const noexcept
    {
        const auto position = static_cast<typename iterator::difference_type>(_packed_count);
        return const_iterator{core::span<const K>(_packed, _packed_count), core::span<const V>(_values, _packed_count),
                              position};
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::const_iterator basic_sparse_map<K, V, Allocator>::cbegin()
        const noexcept
    {
        return begin();
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::iterator basic_sparse_map<K, V, Allocator>::end() noexcept
    {
        return iterator{core::span<const K>(_packed, _packed_count), core::span<V>(_values, _packed_count), {}};
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::const_iterator basic_sparse_map<K, V, Allocator>::end()
        const noexcept
    {
        return const_iterator{
            core::span<const K>(_packed, _packed_count), core::span<const V>(_values, _packed_count), {}};
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::const_iterator basic_sparse_map<K, V, Allocator>::cend()
        const noexcept
    {
        return end();
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::reverse_iterator basic_sparse_map<K, V,
                                                                                          Allocator>::rbegin() noexcept
    {
        return std::make_reverse_iterator(end());
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::const_reverse_iterator basic_sparse_map<
        K, V, Allocator>::rbegin() const noexcept
    {
        return crbegin();
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::const_reverse_iterator basic_sparse_map<
        K, V, Allocator>::crbegin() const noexcept
    {
        return std::make_reverse_iterator(cend());
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::reverse_iterator basic_sparse_map<K, V,
                                                                                          Allocator>::rend() noexcept
    {
        return std::make_reverse_iterator(begin());
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::const_reverse_iterator basic_sparse_map<K, V, Allocator>::rend()
        const noexcept
    {
        return crend();
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::const_reverse_iterator basic_sparse_map<
        K, V, Allocator>::crend() const noexcept
    {
        return std::make_reverse_iterator(cbegin());
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr void basic_sparse_map<K, V, Allocator>::erase(K k) noexcept
    {
        auto it = find(k);
        if (it == end())
        {
            return;
        }
        auto& self = _sparse_reference(it->first);
        const auto e = traits_type::as_entity(self);
        _sparse_reference(_packed[_packed_count - 1]) =
            traits_type::combine_entities(e, traits_type::as_integral(_packed[_packed_count - 1]));

        auto to_remove = static_cast<size_type>(e);
        if (to_remove != _packed_count - 1)
        {
            _packed[to_remove] = std::move(_packed[_packed_count - 1]);
            _values[to_remove] = std::move(_values[_packed_count - 1]);
        }

        std::destroy_at(_packed + _packed_count - 1);
        std::destroy_at(_values + _packed_count - 1);

        self = null;
        --_packed_count;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::iterator basic_sparse_map<K, V, Allocator>::insert(K k,
                                                                                                           const V& v)
    {
        return emplace(k, v);
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::iterator basic_sparse_map<K, V, Allocator>::insert(K k, V&& v)
    {
        return emplace(k, std::move(v));
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::iterator basic_sparse_map<K, V, Allocator>::insert_or_replace(
        K k, const V& v)
    {
        return emplace_or_replace(k, v);
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr void basic_sparse_map<K, V, Allocator>::clear()
    {
        for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
        {
            std::fill_n(_sparse[page_idx], traits_type::page_size, null);
        }
        std::destroy_n(_packed, _packed_count);
        std::destroy_n(_values, _packed_count);
        _packed_count = 0;
    }

    template <typename K, typename V, typename Allocator>
    template <typename... Ts>
    inline constexpr basic_sparse_map<K, V, Allocator>::iterator basic_sparse_map<K, V, Allocator>::emplace(K k,
                                                                                                            Ts&&... ts)
    {
        auto& element = _assure(k);
        auto position = size();

        _packed[_packed_count] = k;
        std::construct_at(_values + _packed_count, std::forward<Ts>(ts)...);
        _packed_count++;
        element = traits_type::combine_entities(static_cast<typename traits_type::entity_type>(_packed_count - 1),
                                                traits_type::as_integral(k));

        return --(end() - position);
    }

    template <typename K, typename V, typename Allocator>
    template <typename... Ts>
    inline constexpr basic_sparse_map<K, V, Allocator>::iterator basic_sparse_map<K, V, Allocator>::emplace_or_replace(
        K k, Ts&&... ts)
    {
        auto& element = _assure(k);
        auto position = size();

        if (traits_type::as_entity(element) < _packed_count)
        {
            _values[traits_type::as_entity(element)] = V(std::forward<Ts>(ts)...);
            return --(end() - position);
        }

        _packed[_packed_count] = k;
        std::construct_at(_values + _packed_count, std::forward<Ts>(ts)...);
        _packed_count++;
        element = traits_type::combine_entities(static_cast<typename traits_type::entity_type>(_packed_count - 1),
                                                traits_type::as_integral(k));

        return --(end() - position);
    }

    template <typename K, typename V, typename Allocator>
    inline void basic_sparse_map<K, V, Allocator>::reserve(std::size_t new_capacity)
    {
        if (new_capacity <= capacity()) [[unlikely]]
        {
            return;
        }

        _request_storage_resize(new_capacity);
    }

    template <typename K, typename V, typename Allocator>
    inline K* basic_sparse_map<K, V, Allocator>::keys() const noexcept
    {
        return _packed;
    }

    template <typename K, typename V, typename Allocator>
    inline V* basic_sparse_map<K, V, Allocator>::values() const noexcept
    {
        return _values;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr void basic_sparse_map<K, V, Allocator>::_release_resources()
    {
        _release_sparse_resources();
        _release_packed_resources();

        _packed_count = 0;
        _packed_capacity = 0;
        _sparse_page_count = 0;
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr void basic_sparse_map<K, V, Allocator>::_release_sparse_resources()
    {
        if (_sparse != nullptr) [[likely]]
        {
            for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
            {
                std::destroy_n(_sparse[page_idx], traits_type::page_size);
                packed_alloc_traits::deallocate(_packed_alloc, _sparse[page_idx], traits_type::page_size);
            }
            std::destroy_n(_sparse, _sparse_page_count);
            sparse_alloc_traits::deallocate(_sparse_alloc, _sparse, _sparse_page_count);
            _sparse = nullptr;
        }
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr void basic_sparse_map<K, V, Allocator>::_release_packed_resources()
    {
        if (_packed) [[likely]]
        {
            std::destroy_n(_packed, _packed_count);
            packed_alloc_traits::deallocate(_packed_alloc, _packed, _packed_capacity);
            _packed = nullptr;
        }

        if (_values) [[likely]]
        {
            std::destroy_n(_values, _packed_count);
            value_alloc_traits::deallocate(_value_alloc, _values, _packed_capacity);
            _values = nullptr;
        }
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr void basic_sparse_map<K, V, Allocator>::_request_storage_resize(size_type sz)
    {
        if (sz == 0) [[unlikely]]
        {
            return;
        }

        const auto aligned_request = std::bit_ceil(sz);
        const auto new_page_count = 1 + ((aligned_request - 1) / traits_type::page_size);

        // handle page count changes
        if (new_page_count > _sparse_page_count)
        {
            K** sparse_pages = sparse_alloc_traits::allocate(_sparse_alloc, new_page_count);
            for (std::size_t page_idx = 0; page_idx < new_page_count; ++page_idx)
            {
                K* page = packed_alloc_traits::allocate(_packed_alloc, traits_type::page_size);
                std::construct_at(sparse_pages + page_idx, page);
            }

            // copy existing pages
            for (std::size_t page_idx = 0; page_idx < _sparse_page_count; ++page_idx)
            {
                K* page = sparse_pages[page_idx];
                std::uninitialized_copy_n(_sparse[page_idx], traits_type::page_size, page);
            }

            // fill new pages
            for (std::size_t page_idx = _sparse_page_count; page_idx < new_page_count; ++page_idx)
            {
                constexpr entity_type null_init = null;
                std::uninitialized_fill_n(sparse_pages[page_idx], traits_type::page_size, null_init);
            }

            // release old pages
            _release_sparse_resources();

            // update sparse members to point at new values
            _sparse = sparse_pages;
            _sparse_page_count = new_page_count;
        }

        // allocate new buffer for packed values if needed
        if (_packed_count >= _packed_capacity)
        {
            const auto aligned_packed = std::bit_ceil(_packed_capacity + 1);

            K* packed_values = packed_alloc_traits::allocate(_packed_alloc, aligned_packed);
            std::uninitialized_copy_n(_packed, _packed_count, packed_values);

            V* value_values = value_alloc_traits::allocate(_value_alloc, aligned_packed);
            std::uninitialized_copy_n(_values, _packed_count, value_values);

            // release old buffer
            _release_packed_resources();

            // update packed members to point at new values
            _packed = packed_values;
            _values = value_values;
            _packed_capacity = aligned_packed;
        }
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr auto& basic_sparse_map<K, V, Allocator>::_assure(K value)
    {
        const auto position = static_cast<size_type>(traits_type::as_entity(value));
        const auto page = position / traits_type::page_size;

        _request_storage_resize(position + 1);

        return _sparse[page][core::fast_mod(position, traits_type::page_size)];
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr auto basic_sparse_map<K, V, Allocator>::_to_iterator(K value) const noexcept
    {
        return --(end() - _index(value));
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr basic_sparse_map<K, V, Allocator>::size_type basic_sparse_map<K, V, Allocator>::_index(
        K value) const noexcept
    {
        return static_cast<size_type>(traits_type::as_entity(_sparse_reference(value)));
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr auto& basic_sparse_map<K, V, Allocator>::_sparse_reference(K value) const noexcept
    {
        const auto position = static_cast<size_type>(traits_type::as_entity(value));
        return _sparse[position / traits_type::page_size][core::fast_mod(position, traits_type::page_size)];
    }

    template <typename K, typename V, typename Allocator>
    inline constexpr auto basic_sparse_map<K, V, Allocator>::_sparse_pointer(K value) const noexcept
    {
        const auto position = static_cast<size_type>(traits_type::as_entity(value));
        const auto page = position / traits_type::page_size;
        return (page < _sparse_page_count && _sparse[page])
                   ? (_sparse[page] + core::fast_mod(position, traits_type::page_size))
                   : nullptr;
    }

    using sparse_set = basic_sparse_set<entity>;

    template <typename V, typename Allocator = std::allocator<entity>>
    using sparse_map = basic_sparse_map<entity, V, Allocator>;
} // namespace tempest::ecs

#endif // tempest_ecs_sparse_hpp