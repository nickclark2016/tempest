#ifndef tempest_core_slot_map_hpp
#define tempest_core_slot_map_hpp

#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/vector.hpp>

namespace tempest
{
    template <typename T>
    struct slot_map_traits;

    template <>
    struct slot_map_traits<uint32_t>
    {
        static constexpr uint32_t empty = 0xFFFFFFFF;
        static constexpr uint32_t id_bits = 20;
        static constexpr uint32_t generation_bits = 12;

        static constexpr uint32_t id_mask = 0x000FFFFF;         // Low 20 bits
        static constexpr uint32_t generation_mask = 0xFFF00000; // High 12 bits

        using id_type = uint32_t;
        using generation_type = uint16_t;
    };

    template <>
    struct slot_map_traits<uint64_t>
    {
        static constexpr uint64_t empty = 0xFFFFFFFFFFFFFFFF;
        static constexpr uint64_t id_bits = 32;
        static constexpr uint64_t generation_bits = 32;

        static constexpr uint64_t id_mask = 0x00000000FFFFFFFF;         // Low 32 bits
        static constexpr uint64_t generation_mask = 0xFFFFFFFF00000000; // High 32 bits

        using id_type = uint32_t;
        using generation_type = uint32_t;
    };

    template <typename T>
    inline constexpr T create_slot_map_key(typename slot_map_traits<T>::id_type id,
                                           typename slot_map_traits<T>::generation_type generation) noexcept
    {
        return (static_cast<T>(generation) << slot_map_traits<T>::id_bits) | id;
    }

    template <typename T>
    inline constexpr typename slot_map_traits<T>::id_type get_slot_map_key_id(T key) noexcept
    {
        return key & slot_map_traits<T>::id_mask;
    }

    template <typename T>
    inline constexpr typename slot_map_traits<T>::generation_type get_slot_map_key_generation(T key) noexcept
    {
        return (key & slot_map_traits<T>::generation_mask) >> slot_map_traits<T>::id_bits;
    }

    template <typename T, typename Allocator>
    class slot_map;

    template <typename T, typename Allocator>
    class slot_map_iterator
    {
      public:
        using slot_map_type =
            conditional_t<is_const_v<T>, const slot_map<remove_const_t<T>, Allocator>, slot_map<T, Allocator>>;

        slot_map_iterator(slot_map_type* map, size_t index) noexcept;

        slot_map_iterator& operator++() noexcept;
        slot_map_iterator operator++(int) noexcept;

        T& operator*() noexcept;
        const T& operator*() const noexcept;
        T* operator->() noexcept;
        const T* operator->() const noexcept;

        bool operator!=(const slot_map_iterator<T, Allocator>& rhs) const noexcept
        {
            return _index != rhs._index;
        }

        bool operator==(const slot_map_iterator<T, Allocator>& rhs) const noexcept
        {
            return _index == rhs._index;
        }

      private:
        slot_map_type* _map;
        size_t _index;
    };

    template <typename T, typename Allocator = allocator<T>>
    class slot_map
    {
      public:
        using value_type = T;
        using allocator_type = Allocator;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = typename allocator_traits<Allocator>::pointer;
        using const_pointer = typename allocator_traits<Allocator>::const_pointer;
        using iterator = slot_map_iterator<T, Allocator>;
        using const_iterator = slot_map_iterator<const T, Allocator>;

        using key_type = conditional_t<sizeof(size_t) >= sizeof(uint64_t), uint64_t, uint32_t>;

        slot_map() noexcept = default;
        slot_map(const slot_map& other);
        slot_map(slot_map&& other) noexcept;
        ~slot_map();
        slot_map& operator=(const slot_map& other);
        slot_map& operator=(slot_map&& other) noexcept;

        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] size_type size() const noexcept;
        [[nodiscard]] size_type capacity() const noexcept;
        [[nodiscard]] size_type max_capacity() const noexcept;

        void clear();

        key_type insert(const T& value);
        key_type insert(T&& value);

        template <typename... Args>
        key_type emplace(Args&&... args);

        bool erase(key_type key);

        iterator begin() noexcept;
        const_iterator begin() const noexcept;
        const_iterator cbegin() const noexcept;

        iterator end() noexcept;
        const_iterator end() const noexcept;
        const_iterator cend() const noexcept;

        reference operator[](key_type key);
        const_reference operator[](key_type key) const;

        reference at(key_type key);
        const_reference at(key_type key) const;

        iterator find(key_type key) noexcept;
        const_iterator find(key_type key) const noexcept;

        size_t index_of(key_type key) const noexcept;

        void swap(slot_map& other) noexcept;

      private:
        struct key_block
        {
            static constexpr size_t element_count = 16 / sizeof(uint32_t);
            static constexpr size_t value_count = 128;
            static constexpr size_t skip_field_bits_per_element = sizeof(uint32_t) * 8;

            uint32_t skip_field[element_count];
            size_t key_table[value_count]; // one entry per element of skip field

            alignas(
                alignof(T)) unsigned char data[sizeof(T) * value_count]; // uninitialized byte array for data storage

            [[nodiscard]] T* typed_ptr() noexcept;
            [[nodiscard]] const T* typed_ptr() const noexcept;

            key_block() noexcept = default;
        };

        using alloc_traits = allocator_traits<Allocator>;
        using key_block_allocator = typename alloc_traits::template rebind_alloc<key_block>;
        using key_block_traits = allocator_traits<key_block_allocator>;

        vector<key_block, key_block_allocator> _elements;

        slot_map_traits<key_type>::id_type _first_free_element = 0;
        size_t _size = 0;

        [[nodiscard]] key_block& _get_block(key_type key) noexcept;
        [[nodiscard]] const key_block& _get_block(key_type key) const noexcept;

        void _add_to_free_list(key_type key) noexcept;
        [[nodiscard]] bool _has_free_elements() const noexcept;
        [[nodiscard]] key_type _get_next_free_element() noexcept;
        void _mark_as_free(key_type key) noexcept;
        void _mark_as_used(key_type key) noexcept;

        void _grow() noexcept;
        void _grow_to(size_t new_capacity) noexcept;
        void _initialize_block(key_block& block, size_t first_index) noexcept;

        void _insert_at(key_type key, const T& value);
        void _insert_at(key_type key, T&& value);

        size_t _search_for_free_element(size_t start_index) noexcept;

        void _release();

        // Friend both the const and non-const iterator
        friend slot_map_iterator<T, Allocator>;
        friend slot_map_iterator<const T, Allocator>;
    };

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::slot_map(const slot_map& other)
    {
        _grow_to(other._elements.capacity());

        for (size_t block_idx = 0; block_idx < other._elements.size(); ++block_idx)
        {
            auto& other_block = other._elements[block_idx];
            auto& this_block = _elements[block_idx];

            for (size_t i = 0; i < key_block::element_count; ++i)
            {
                this_block.skip_field[i] = other_block.skip_field[i];
            }

            for (size_t i = 0; i < key_block::value_count; ++i)
            {
                const auto skip_list = other_block.skip_field[i / key_block::element_count];
                const auto skip_entry = skip_list & (1 << (i % key_block::skip_field_bits_per_element));

                this_block.key_table[i] = other_block.key_table[i];

                if (skip_entry)
                {
                    _insert_at(other_block.key_table[i], other_block.typed_ptr()[i]);
                }
            }
        }

        _first_free_element = other._first_free_element;
        _size = other._size;
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::slot_map(slot_map&& other) noexcept
        : _elements(tempest::move(other._elements)), _first_free_element(other._first_free_element), _size(other._size)
    {
        other._first_free_element = 0;
        other._size = 0;
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::~slot_map()
    {
        _release();
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>& slot_map<T, Allocator>::operator=(const slot_map<T, Allocator>& rhs)
    {
        if (this != &rhs)
        {
            _release();

            _grow_to(rhs._elements.capacity());

            for (size_t block_idx = 0; block_idx < rhs._elements.size(); ++block_idx)
            {
                auto& other_block = rhs._elements[block_idx];
                auto& this_block = _elements[block_idx];

                for (size_t i = 0; i < key_block::element_count; ++i)
                {
                    this_block.skip_field[i] = other_block.skip_field[i];
                }

                for (size_t i = 0; i < key_block::value_count; ++i)
                {
                    const auto skip_list = other_block.skip_field[i / key_block::element_count];
                    const auto skip_entry = skip_list & (1 << (i % key_block::skip_field_bits_per_element));

                    this_block.key_table[i] = other_block.key_table[i];

                    if (skip_entry)
                    {
                        _insert_at(other_block.key_table[i], other_block.typed_ptr()[i]);
                    }
                }
            }

            _first_free_element = rhs._first_free_element;
            _size = rhs._size;
        }

        return *this;
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>& slot_map<T, Allocator>::operator=(slot_map<T, Allocator>&& rhs) noexcept
    {
        if (this != &rhs)
        {
            _release();

            _elements = tempest::move(rhs._elements);
            _first_free_element = rhs._first_free_element;
            _size = rhs._size;

            rhs._first_free_element = 0;
            rhs._size = 0;
        }

        return *this;
    }

    template <typename T, typename Allocator>
    inline bool slot_map<T, Allocator>::empty() const noexcept
    {
        return _size == 0;
    }

    template <typename T, typename Allocator>
    inline typename slot_map<T, Allocator>::size_type slot_map<T, Allocator>::size() const noexcept
    {
        return _size;
    }

    template <typename T, typename Allocator>
    inline typename slot_map<T, Allocator>::size_type slot_map<T, Allocator>::capacity() const noexcept
    {
        return _elements.size() * key_block::value_count;
    }

    template <typename T, typename Allocator>
    inline typename slot_map<T, Allocator>::size_type slot_map<T, Allocator>::max_capacity() const noexcept
    {
        return slot_map_traits<key_type>::id_mask;
    }

    template <typename T, typename Allocator>
    typename slot_map<T, Allocator>::key_type slot_map<T, Allocator>::insert(const T& value)
    {
        key_type key = _get_next_free_element();
        _insert_at(key, value);

        return key;
    }

    template <typename T, typename Allocator>
    typename slot_map<T, Allocator>::key_type slot_map<T, Allocator>::insert(T&& value)
    {
        key_type key = _get_next_free_element();
        _insert_at(key, tempest::move(value));

        return key;
    }

    template <typename T, typename Allocator>
    template <typename... Args>
    typename slot_map<T, Allocator>::key_type slot_map<T, Allocator>::emplace(Args&&... args)
    {
        key_type key = _get_next_free_element();
        _insert_at(key, T(tempest::forward<Args>(args)...));

        return key;
    }

    template <typename T, typename Allocator>
    bool slot_map<T, Allocator>::erase(key_type key)
    {
        const auto key_index = get_slot_map_key_id(key);
        auto& block = _get_block(key);
        auto block_index = key_index % key_block::value_count;

        if (block.key_table[block_index] != key)
        {
            return false;
        }

        _mark_as_free(key);
        destroy_at(&block.typed_ptr()[block_index]);

        // Increase the generation
        const auto generation = get_slot_map_key_generation(key);
        const auto new_key = create_slot_map_key<key_type>(key_index, generation + 1);
        block.key_table[block_index] = new_key;

        _add_to_free_list(new_key);

        --_size;

        return true;
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::iterator slot_map<T, Allocator>::begin() noexcept
    {
        return slot_map_iterator<T, Allocator>(this, _search_for_free_element(0));
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::const_iterator slot_map<T, Allocator>::begin() const noexcept
    {
        return slot_map_iterator<const T, Allocator>(this, _search_for_free_element(0));
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::const_iterator slot_map<T, Allocator>::cbegin() const noexcept
    {
        return slot_map_iterator<const T, Allocator>(this, _search_for_free_element(0));
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::iterator slot_map<T, Allocator>::end() noexcept
    {
        return slot_map_iterator<T, Allocator>(this, capacity());
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::const_iterator slot_map<T, Allocator>::end() const noexcept
    {
        return slot_map_iterator<const T, Allocator>(this, capacity());
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::const_iterator slot_map<T, Allocator>::cend() const noexcept
    {
        return slot_map_iterator<const T, Allocator>(this, capacity());
    }

    template <typename T, typename Allocator>
    inline slot_map<T, Allocator>::reference slot_map<T, Allocator>::operator[](key_type key)
    {
        return at(key);
    }

    template <typename T, typename Allocator>
    inline slot_map<T, Allocator>::const_reference slot_map<T, Allocator>::operator[](key_type key) const
    {
        return at(key);
    }

    template <typename T, typename Allocator>
    inline slot_map<T, Allocator>::reference slot_map<T, Allocator>::at(key_type key)
    {
        return *find(key);
    }

    template <typename T, typename Allocator>
    inline slot_map<T, Allocator>::const_reference slot_map<T, Allocator>::at(key_type key) const
    {
        return *find(key);
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::iterator slot_map<T, Allocator>::find(key_type key) noexcept
    {
        const auto key_index = get_slot_map_key_id(key);

        if (key_index >= _elements.size() * key_block::value_count)
        {
            return end();
        }

        auto& block = _get_block(key);
        auto block_index = key_index % key_block::value_count;

        if (block.key_table[block_index] == key)
        {
            return slot_map_iterator<T, Allocator>(this, key_index);
        }

        return end();
    }

    template <typename T, typename Allocator>
    slot_map<T, Allocator>::const_iterator slot_map<T, Allocator>::find(key_type key) const noexcept
    {
        const auto key_index = get_slot_map_key_id(key);
        auto& block = _get_block(key);
        auto block_index = key_index % key_block::value_count;

        if (block.key_table[block_index] == key)
        {
            return slot_map_iterator<const T, Allocator>(this, key_index);
        }

        return end();
    }

    template <typename T, typename Allocator>
    inline size_t slot_map<T, Allocator>::index_of(key_type key) const noexcept
    {
        const auto key_index = get_slot_map_key_id(key);
        return key_index;
    }

    template <typename T, typename Allocator>
    inline void slot_map<T, Allocator>::swap(slot_map& other) noexcept
    {
        using tempest::swap;

        swap(_elements, other._elements);
        swap(_first_free_element, other._first_free_element);
        swap(_size, other._size);
    }

    template <typename T, typename Allocator>
    T* slot_map<T, Allocator>::key_block::typed_ptr() noexcept
    {
        return reinterpret_cast<T*>(data);
    }

    template <typename T, typename Allocator>
    const T* slot_map<T, Allocator>::key_block::typed_ptr() const noexcept
    {
        return reinterpret_cast<const T*>(data);
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::clear()
    {
        _first_free_element = 0;

        for (key_block& block : _elements)
        {
            // TODO: Investigate optimizing out the integer division
            for (size_t i = 0; i < key_block::value_count; ++i)
            {
                if (block.skip_field[i / key_block::element_count] &
                    (1 << (i % key_block::skip_field_bits_per_element)))
                {
                    destroy_at(&block.typed_ptr()[i]);
                }
            }

            for (auto& sf : block.skip_field)
            {
                sf = 0;
            }
        }

        size_t idx = 0;
        for (key_block& block : _elements)
        {
            _initialize_block(block, idx);
            idx += key_block::value_count;
        }
    }

    template <typename T, typename Allocator>
    typename slot_map<T, Allocator>::key_block& slot_map<T, Allocator>::_get_block(key_type key) noexcept
    {
        const auto key_index = get_slot_map_key_id(key);

        const size_t block_index = key_index / key_block::value_count;
        return _elements[block_index];
    }

    template <typename T, typename Allocator>
    const typename slot_map<T, Allocator>::key_block& slot_map<T, Allocator>::_get_block(key_type key) const noexcept
    {
        const auto key_index = get_slot_map_key_id(key);

        const size_t block_index = key_index / key_block::value_count;
        return _elements[block_index];
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::_add_to_free_list(key_type key) noexcept
    {
        const auto key_index = get_slot_map_key_id(key);

        key_block& block = _get_block(key);
        const auto block_index = key_index % key_block::value_count;

        // Create a key where the index is the current free head and the generation is the current generation
        const auto current_generation = get_slot_map_key_generation(key);
        const auto next_index =
            static_cast<slot_map_traits<key_type>::id_type>(_first_free_element & slot_map_traits<key_type>::id_mask);
        const auto new_key = create_slot_map_key<key_type>(next_index, current_generation);

        block.key_table[block_index] = new_key;
        _first_free_element = key_index;
    }

    template <typename T, typename Allocator>
    bool slot_map<T, Allocator>::_has_free_elements() const noexcept
    {
        return _first_free_element < static_cast<slot_map_traits<key_type>::id_type>(capacity());
    }

    template <typename T, typename Allocator>
    typename slot_map<T, Allocator>::key_type slot_map<T, Allocator>::_get_next_free_element() noexcept
    {
        // Get the current free element
        if (_size >= capacity())
        {
            _grow();
        }

        const auto free_element_index = _first_free_element;

        auto& block = _get_block(free_element_index);
        const key_type free_list_entry = block.key_table[free_element_index % key_block::value_count];

        // Extract the generation from the free list entry
        const auto generation = get_slot_map_key_generation(free_list_entry);
        const auto next_index =
            static_cast<slot_map_traits<key_type>::id_type>(free_list_entry & slot_map_traits<key_type>::id_mask);

        const auto free_key =
            create_slot_map_key<key_type>(free_element_index, generation); // Generation is incremented on erase

        // Mark the key as in use
        _mark_as_used(free_key);
        // Update the key in the table
        block.key_table[free_element_index % key_block::value_count] = free_key;

        // Update the free list head
        _first_free_element = next_index;

        return free_key;
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::_mark_as_free(key_type key) noexcept
    {
        const auto key_index = get_slot_map_key_id(key);
        auto& block = _get_block(key);
        auto block_index = key_index % key_block::value_count;

        auto& skip_field_entry = block.skip_field[block_index / key_block::skip_field_bits_per_element];
        skip_field_entry &= ~(1 << (block_index % key_block::skip_field_bits_per_element));
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::_mark_as_used(key_type key) noexcept
    {
        const auto key_index = get_slot_map_key_id(key);
        auto& block = _get_block(key);
        auto block_index = key_index % key_block::value_count;

        auto& skip_field_entry = block.skip_field[block_index / key_block::skip_field_bits_per_element];
        skip_field_entry |= 1 << (block_index % key_block::skip_field_bits_per_element);
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::_initialize_block(key_block& block, size_t first_index) noexcept
    {
        for (size_t i = 0; i < key_block::element_count; ++i)
        {
            block.skip_field[i] = 0;
        }

        for (size_t i = 0; i < key_block::value_count; ++i)
        {
            const auto next = static_cast<slot_map_traits<key_type>::id_type>(first_index + i);
            const auto key = create_slot_map_key<key_type>(next, 0);
            block.key_table[i] = key;
            _add_to_free_list(key);
        }
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::_grow() noexcept
    {
        const size_t new_capacity = std::max<size_t>(_elements.size() * 2, 1);
        _grow_to(new_capacity);
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::_grow_to(size_t new_capacity) noexcept
    {
        if (new_capacity > _elements.capacity())
        {
            _elements.reserve(new_capacity);
        }

        for (size_t i = _elements.size(); i < new_capacity; ++i)
        {
            key_block& block = _elements.emplace_back();
            _initialize_block(block, i * key_block::value_count);
        }
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::_insert_at(key_type key, const T& value)
    {
        const auto key_index = get_slot_map_key_id(key);
        auto& block = _get_block(key);
        auto block_index = key_index % key_block::value_count;

        (void)construct_at(&block.typed_ptr()[block_index], value);
        _mark_as_used(key);

        ++_size;
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::_insert_at(key_type key, T&& value)
    {
        const auto key_index = get_slot_map_key_id(key);
        auto& block = _get_block(key);
        auto block_index = key_index % key_block::value_count;

        (void)construct_at(&block.typed_ptr()[block_index], tempest::move(value));

        ++_size;
    }

    template <typename T, typename Allocator>
    void slot_map<T, Allocator>::_release()
    {
        if (empty())
        {
            return;
        }

        for (auto& block : _elements)
        {
            for (size_t i = 0; i < key_block::value_count; ++i)
            {
                if (block.skip_field[i / key_block::element_count] &
                    (1 << (i % key_block::skip_field_bits_per_element)))
                {
                    destroy_at(&block.typed_ptr()[i]);
                }
            }
        }

        _size = 0;
    }

    template <typename T, typename Allocator>
    inline size_t slot_map<T, Allocator>::_search_for_free_element(size_t start_index) noexcept
    {
        size_t start_block_index = start_index / key_block::value_count;
        size_t start_block_offset = start_index % key_block::value_count;

        size_t element_index = start_index;

        for (size_t i = start_block_index; i < _elements.size(); ++i)
        {
            key_block& block = _elements[i];

            // TODO: AVX2 implementation?
            // AVX2 implementation would be faster, especially for densely populated maps
            // This is sufficient for now
            // https://lemire.me/blog/2018/03/08/iterating-over-set-bits-quickly-simd-edition/
            for (size_t j = start_block_offset; j < key_block::value_count; ++j)
            {
                if (block.skip_field[j / key_block::skip_field_bits_per_element] &
                    (1 << (j % key_block::skip_field_bits_per_element)))
                {
                    return element_index;
                }

                ++element_index;
            }

            start_block_offset = 0;
        }

        return element_index;
    }

    template <typename T, typename Allocator>
    inline slot_map_iterator<T, Allocator>::slot_map_iterator(slot_map_type* map, size_t index) noexcept
        : _map{map}, _index{index}
    {
    }

    template <typename T, typename Allocator>
    inline slot_map_iterator<T, Allocator>& slot_map_iterator<T, Allocator>::operator++() noexcept
    {
        _index = _map->_search_for_free_element(_index + 1);

        return *this;
    }

    template <typename T, typename Allocator>
    inline slot_map_iterator<T, Allocator> slot_map_iterator<T, Allocator>::operator++(int) noexcept
    {
        slot_map_iterator<T, Allocator> copy = *this;
        ++(*this);

        return copy;
    }

    template <typename T, typename Allocator>
    inline T& slot_map_iterator<T, Allocator>::operator*() noexcept
    {
        auto block_index = _index / slot_map<T, Allocator>::key_block::value_count;
        auto block_offset = _index % slot_map<T, Allocator>::key_block::value_count;

        return _map->_elements[block_index].typed_ptr()[block_offset];
    }

    template <typename T, typename Allocator>
    inline const T& slot_map_iterator<T, Allocator>::operator*() const noexcept
    {
        auto block_index = _index / slot_map<T, Allocator>::key_block::value_count;
        auto block_offset = _index % slot_map<T, Allocator>::key_block::value_count;

        return _map->_elements[block_index].typed_ptr()[block_offset];
    }

    template <typename T, typename Allocator>
    inline T* slot_map_iterator<T, Allocator>::operator->() noexcept
    {
        return &**this;
    }

    template <typename T, typename Allocator>
    inline const T* slot_map_iterator<T, Allocator>::operator->() const noexcept
    {
        return &**this;
    }

    template <typename T, typename Allocator>
    inline void swap(slot_map<T, Allocator>& lhs, slot_map<T, Allocator>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
} // namespace tempest

#endif // tempest_core_slot_map_hpp