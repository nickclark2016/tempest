#ifndef tempest_core_flat_map_hpp
#define tempest_core_flat_map_hpp

#include <compare>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <utility>
#include <vector>

namespace tempest::core
{
    namespace detail
    {
        template <typename K, typename V>
        struct flat_map_iterator_ptr_adapter
        {
            std::pair<const K&, V&> pair;

            friend auto operator<=>(const flat_map_iterator_ptr_adapter& lhs,
                                    const flat_map_iterator_ptr_adapter& rhs) noexcept = default;

            std::pair<const K&, V&> operator*() noexcept;
            std::pair<const K&, const V&> operator*() const noexcept;
            std::pair<const K&, V&>* operator->() noexcept;
            const std::pair<const K&, V&>* operator->() const noexcept;
        };

        template <typename K, typename V>
        inline std::pair<const K&, V&> flat_map_iterator_ptr_adapter<K, V>::operator*() noexcept
        {
            return pair;
        }

        template <typename K, typename V>
        inline std::pair<const K&, const V&> flat_map_iterator_ptr_adapter<K, V>::operator*() const noexcept
        {
            return {pair.first, pair.second};
        }

        template <typename K, typename V>
        inline std::pair<const K&, V&>* flat_map_iterator_ptr_adapter<K, V>::operator->() noexcept
        {
            return &pair;
        }

        template <typename K, typename V>
        inline const std::pair<const K&, V&>* flat_map_iterator_ptr_adapter<K, V>::operator->() const noexcept
        {
            return &pair;
        }

        template <typename K, typename V>
        struct flat_map_iterator
        {
            std::pair<const K*, V*> pair;

            friend auto operator<=>(const flat_map_iterator& lhs, const flat_map_iterator& rhs) noexcept = default;

            flat_map_iterator& operator++() noexcept;
            flat_map_iterator operator++(int) noexcept;
            flat_map_iterator& operator--() noexcept;
            flat_map_iterator operator--(int) noexcept;

            flat_map_iterator& operator+=(std::ptrdiff_t n) noexcept;
            flat_map_iterator& operator-=(std::ptrdiff_t n) noexcept;
            flat_map_iterator operator+(std::ptrdiff_t n) const noexcept;
            flat_map_iterator operator-(std::ptrdiff_t n) const noexcept;

            std::ptrdiff_t operator-(const flat_map_iterator& other) const noexcept;

            std::pair<const K&, V&> operator[](std::ptrdiff_t n) noexcept;
            std::pair<const K&, const V&> operator[](std::ptrdiff_t n) const noexcept;

            std::pair<const K&, V&> operator*() noexcept;
            std::pair<const K&, const V&> operator*() const noexcept;

            flat_map_iterator_ptr_adapter<K, V> operator->() noexcept;
            flat_map_iterator_ptr_adapter<K, const V> operator->() const noexcept;

            bool operator==(const flat_map_iterator& other) const noexcept;

            operator flat_map_iterator<K, const V>() noexcept;
        };

        template <typename K, typename V>
        inline flat_map_iterator<K, V>& flat_map_iterator<K, V>::operator++() noexcept
        {
            ++pair.first;
            ++pair.second;
            return *this;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V> flat_map_iterator<K, V>::operator++(int) noexcept
        {
            auto copy = *this;
            ++(*this);
            return copy;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V>& flat_map_iterator<K, V>::operator--() noexcept
        {
            --pair.first;
            --pair.second;
            return *this;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V> flat_map_iterator<K, V>::operator--(int) noexcept
        {
            auto copy = *this;
            --(*this);
            return copy;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V>& flat_map_iterator<K, V>::operator+=(std::ptrdiff_t n) noexcept
        {
            pair.first += n;
            pair.second += n;
            return *this;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V>& flat_map_iterator<K, V>::operator-=(std::ptrdiff_t n) noexcept
        {
            pair.first -= n;
            pair.second -= n;
            return *this;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V> flat_map_iterator<K, V>::operator+(std::ptrdiff_t n) const noexcept
        {
            auto copy = *this;
            copy += n;
            return copy;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V> flat_map_iterator<K, V>::operator-(std::ptrdiff_t n) const noexcept
        {
            auto copy = *this;
            copy -= n;
            return copy;
        }

        template <typename K, typename V>
        inline std::ptrdiff_t flat_map_iterator<K, V>::operator-(const flat_map_iterator& other) const noexcept
        {
            return pair.first - other.pair.first;
        }

        template <typename K, typename V>
        inline std::pair<const K&, V&> flat_map_iterator<K, V>::operator[](std::ptrdiff_t n) noexcept
        {
            return std::pair<const K&, V&>(pair.first[n], pair.second[n]);
        }

        template <typename K, typename V>
        inline std::pair<const K&, const V&> flat_map_iterator<K, V>::operator[](std::ptrdiff_t n) const noexcept
        {
            return std::pair<const K&, const V&>(pair.first[n], pair.second[n]);
        }

        template <typename K, typename V>
        inline std::pair<const K&, V&> flat_map_iterator<K, V>::operator*() noexcept
        {
            return std::pair<const K&, V&>(*pair.first, *pair.second);
        }

        template <typename K, typename V>
        inline std::pair<const K&, const V&> flat_map_iterator<K, V>::operator*() const noexcept
        {
            return std::pair<const K&, const V&>(*pair.first, *pair.second);
        }

        template <typename K, typename V>
        inline flat_map_iterator_ptr_adapter<K, V> flat_map_iterator<K, V>::operator->() noexcept
        {
            return {std::pair<const K&, V&>(*pair.first, *pair.second)};
        }

        template <typename K, typename V>
        inline flat_map_iterator_ptr_adapter<K, const V> flat_map_iterator<K, V>::operator->() const noexcept
        {
            return {std::pair<const K&, const V&>(*pair.first, *pair.second)};
        }

        template <typename K, typename V>
        inline bool flat_map_iterator<K, V>::operator==(const flat_map_iterator& other) const noexcept
        {
            return pair.first == other.pair.first;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V>::operator flat_map_iterator<K, const V>() noexcept
        {
            return {{pair.first, pair.second}};
        }
    } // namespace detail

    template <typename K, typename V, typename Compare = std::less<K>, typename KeyContainer = std::vector<K>,
              typename ValueContainer = std::vector<V>>
    class flat_map
    {
      public:
        using key_type = K;
        using mapped_type = V;
        using key_compare = Compare;
        using value_type = std::pair<K, V>;
        using reference = std::pair<const K&, V&>;
        using const_reference = std::pair<const K&, const V&>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using iterator = detail::flat_map_iterator<K, V>;
        using const_iterator = detail::flat_map_iterator<K, const V>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using key_container_type = KeyContainer;
        using value_container_type = ValueContainer;

        struct containers
        {
            KeyContainer keys;
            ValueContainer values;
        };

        class value_compare
        {
            friend flat_map;

          private:
            key_compare comp;
            value_compare(key_compare c);

          public:
            bool operator()(const_reference x, const_reference y) const;
        };

        flat_map() = default;
        flat_map(std::initializer_list<value_type>&& values);

        flat_map& operator=(const flat_map& rhs) = default;
        flat_map& operator=(flat_map&& rhs) noexcept = default;
        flat_map& operator=(std::initializer_list<value_type> values);

        iterator begin() noexcept;
        const_iterator begin() const noexcept;
        const_iterator cbegin() const noexcept;

        iterator end() noexcept;
        const_iterator end() const noexcept;
        const_iterator cend() const noexcept;

        reverse_iterator rbegin() noexcept;
        const_reverse_iterator rbegin() const noexcept;
        const_reverse_iterator crbegin() const noexcept;

        reverse_iterator rend() noexcept;
        const_reverse_iterator rend() const noexcept;
        const_reverse_iterator crend() const noexcept;

        bool empty() const noexcept;
        size_type size() const noexcept;
        size_type max_size() const noexcept;

        void reserve(size_type new_capacity);
        void shrink_to_fit();

        std::pair<iterator, bool> insert(const value_type& value);
        std::pair<iterator, bool> insert(value_type&& value);

        template <typename O>
        std::pair<iterator, bool> insert_or_assign(const key_type& k, O&& obj);

        template <typename O>
        std::pair<iterator, bool> insert_or_assign(key_type&& k, O&& obj);

        template <typename InputIt>
        void insert(InputIt first, InputIt last);

        void insert(std::initializer_list<value_type> values);

        containers extract() && noexcept;
        void replace(KeyContainer&& keys, ValueContainer&& values);

        iterator erase(iterator position);
        iterator erase(const_iterator position);
        iterator erase(const_iterator first, const_iterator last);
        size_type erase(const key_type& key);

        void swap(flat_map& other) noexcept;

        void clear();

        iterator find(const key_type& key);
        const_iterator find(const key_type& key) const;

        mapped_type& operator[](const key_type& key);

        size_type count(const key_type& key) const;
        bool contains(const key_type& key) const;

        iterator lower_bound(const key_type& key);
        const_iterator lower_bound(const key_type& key) const;

        iterator upper_bound(const key_type& key);
        const_iterator upper_bound(const key_type& key) const;

        std::pair<iterator, iterator> equal_range(const key_type& key);
        std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const;

        key_compare key_comp() const;
        value_compare value_comp() const;
        const key_container_type& keys() const noexcept;
        const value_container_type& values() const noexcept;

      private:
        KeyContainer _keys;
        ValueContainer _values;

        key_compare _key_comparator;
        value_compare _value_comparator{_key_comparator};
    };

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::value_compare::value_compare(key_compare c) : comp(c)
    {
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline bool flat_map<K, V, Compare, KeyContainer, ValueContainer>::value_compare::operator()(
        const_reference x, const_reference y) const
    {
        return comp(x.first, y.first);
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::flat_map(std::initializer_list<value_type>&& values)
    {
        _keys.reserve(std::size(values));
        _values.reserve(std::size(values));

        for (auto&& value : values)
        {
            insert(value);
        }
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>& flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::operator=(std::initializer_list<value_type> values)
    {
        clear();
        for (auto&& value : values)
        {
            insert(value);
        }

        return *this;
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<K, V, Compare, KeyContainer,
                                                                                    ValueContainer>::begin() noexcept
    {
        return {{std::data(_keys), std::data(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::begin() const noexcept
    {
        return {{std::data(_keys), std::data(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::cbegin() const noexcept
    {
        return {{std::data(_keys), std::data(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<K, V, Compare, KeyContainer,
                                                                                    ValueContainer>::end() noexcept
    {
        return {{std::data(_keys) + std::size(_keys), std::data(_values) + std::size(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::end() const noexcept
    {
        return {{std::data(_keys) + std::size(_keys), std::data(_values) + std::size(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::cend() const noexcept
    {
        return {{std::data(_keys) + std::size(_keys), std::data(_values) + std::size(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::reverse_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::rbegin() noexcept
    {
        return {end()};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_reverse_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::rbegin() const noexcept
    {
        return {end()};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_reverse_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::crbegin() const noexcept
    {
        return {end()};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::reverse_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::rend() noexcept
    {
        return {begin()};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_reverse_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::rend() const noexcept
    {
        return {begin()};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_reverse_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::crend() const noexcept
    {
        return {begin()};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline bool flat_map<K, V, Compare, KeyContainer, ValueContainer>::empty() const noexcept
    {
        return std::empty(_keys);
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::size_type flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::size() const noexcept
    {
        return std::size(_keys);
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::size_type flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::max_size() const noexcept
    {
        return std::min(_keys.max_size(), _values.max_size());
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline void flat_map<K, V, Compare, KeyContainer, ValueContainer>::reserve(size_type new_capacity)
    {
        _keys.reserve(new_capacity);
        _values.reserve(new_capacity);
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline void flat_map<K, V, Compare, KeyContainer, ValueContainer>::shrink_to_fit()
    {
        _keys.shrink_to_fit();
        _values.shrink_to_fit();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline std::pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator, bool> flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::insert(const value_type& value)
    {
        auto it = std::lower_bound(std::begin(_keys), std::end(_keys), value.first, key_compare{});
        if (it != std::end(_keys) && !key_compare{}(value.first, *it))
        {
            return {iterator{{&*it, &*std::next(std::begin(_values), std::distance(std::begin(_keys), it))}}, false};
        }

        auto key_it = std::next(std::begin(_keys), std::distance(std::begin(_keys), it));
        auto value_it = std::next(std::begin(_values), std::distance(std::begin(_keys), it));

        key_it = _keys.insert(key_it, value.first);
        value_it = _values.insert(value_it, value.second);

        return {iterator{{&*key_it, &*value_it}}, true};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline std::pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator, bool> flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::insert(value_type&& value)
    {
        auto it = std::lower_bound(std::begin(_keys), std::end(_keys), value.first, key_compare{});
        if (it != std::end(_keys) && !key_compare{}(value.first, *it))
        {
            return {iterator{{&*it, &*std::next(std::begin(_values), std::distance(std::begin(_keys), it))}}, false};
        }

        auto key_it = std::next(std::begin(_keys), std::distance(std::begin(_keys), it));
        auto value_it = std::next(std::begin(_values), std::distance(std::begin(_keys), it));

        key_it = _keys.insert(key_it, std::move(value.first));
        value_it = _values.insert(value_it, std::move(value.second));

        return {iterator{{&*key_it, &*value_it}}, true};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    template <typename O>
    inline std::pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator, bool> flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::insert_or_assign(const key_type& k, O&& obj)
    {
        auto it = std::lower_bound(std::begin(_keys), std::end(_keys), k, key_compare{});
        if (it != std::end(_keys) && !key_compare{}(k, *it))
        {
            auto key_it = std::next(std::begin(_keys), std::distance(std::begin(_keys), it));
            auto value_it = std::next(std::begin(_values), std::distance(std::begin(_keys), it));

            *value_it = std::forward<O>(obj);

            return {iterator{{&*key_it, &*value_it}}, false};
        }

        auto key_it = std::next(std::begin(_keys), std::distance(std::begin(_keys), it));
        auto value_it = std::next(std::begin(_values), std::distance(std::begin(_keys), it));

        key_it = _keys.insert(key_it, k);
        value_it = _values.insert(value_it, std::forward<O>(obj));

        return {iterator{{&*key_it, &*value_it}}, true};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    template <typename O>
    inline std::pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator, bool> flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::insert_or_assign(key_type&& k, O&& obj)
    {
        auto it = std::lower_bound(std::begin(_keys), std::end(_keys), k, key_compare{});
        if (it != std::end(_keys) && !key_compare{}(k, *it))
        {
            auto key_it = std::next(std::begin(_keys), std::distance(std::begin(_keys), it));
            auto value_it = std::next(std::begin(_values), std::distance(std::begin(_keys), it));

            *value_it = std::forward<O>(obj);

            return {iterator{{&*key_it, &*value_it}}, false};
        }

        auto key_it = std::next(std::begin(_keys), std::distance(std::begin(_keys), it));
        auto value_it = std::next(std::begin(_values), std::distance(std::begin(_keys), it));

        key_it = _keys.insert(key_it, std::move(k));
        value_it = _values.insert(value_it, std::forward<O>(obj));

        return {iterator{{&*key_it, &*value_it}}, true};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    template <typename InputIt>
    inline void flat_map<K, V, Compare, KeyContainer, ValueContainer>::insert(InputIt first, InputIt last)
    {
        for (auto it = first; it != last; ++it)
        {
            insert(*it);
        }
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline void flat_map<K, V, Compare, KeyContainer, ValueContainer>::insert(std::initializer_list<value_type> values)
    {
        insert(std::begin(values), std::end(values));
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::containers flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::extract() && noexcept
    {
        return {std::move(_keys), std::move(_values)};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline void flat_map<K, V, Compare, KeyContainer, ValueContainer>::replace(KeyContainer&& keys,
                                                                               ValueContainer&& values)
    {
        _keys = std::move(keys);
        _values = std::move(values);
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::erase(iterator position)
    {
        const key_type* keys_ptr = std::data(_keys);

        auto idx = std::distance(keys_ptr, position.pair.first);

        auto key_it = std::next(std::begin(_keys), idx);
        auto value_it = std::next(std::begin(_values), idx);

        key_it = _keys.erase(key_it);
        value_it = _values.erase(value_it);

        return {{std::data(_keys) + std::distance(std::begin(_keys), key_it),
                 std::data(_values) + std::distance(std::begin(_values), value_it)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::erase(const_iterator position)
    {
        const key_type* keys_ptr = std::data(_keys);

        auto idx = std::distance(keys_ptr, position.pair.first);

        auto key_it = std::next(std::begin(_keys), idx);
        auto value_it = std::next(std::begin(_values), idx);

        key_it = _keys.erase(key_it);
        value_it = _values.erase(value_it);

        return {{std::data(_keys) + std::distance(std::begin(_keys), key_it),
                 std::data(_values) + std::distance(std::begin(_values), value_it)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::erase(const_iterator first, const_iterator last)
    {
        const key_type* keys_ptr = std::data(_keys);

        auto first_idx = std::distance(keys_ptr, first.pair.first);
        auto last_idx = std::distance(keys_ptr, last.pair.first);

        auto key_first = std::next(std::begin(_keys), first_idx);
        auto key_last = std::next(std::begin(_keys), last_idx);
        auto value_first = std::next(std::begin(_values), first_idx);
        auto value_last = std::next(std::begin(_values), last_idx);

        key_first = _keys.erase(key_first, key_last);
        value_first = _values.erase(value_first, value_last);

        return {{std::data(_keys) + std::distance(std::begin(_keys), key_first),
                 std::data(_values) + std::distance(std::begin(_values), value_first)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::size_type flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::erase(const key_type& key)
    {
        auto it = find(key);
        if (it == end())
        {
            return 0;
        }

        erase(it);

        return 1;
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline void flat_map<K, V, Compare, KeyContainer, ValueContainer>::swap(flat_map& other) noexcept
    {
        using std::swap;

        swap(_keys, other._keys);
        swap(_values, other._values);
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline void flat_map<K, V, Compare, KeyContainer, ValueContainer>::clear()
    {
        _keys.clear();
        _values.clear();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::find(const key_type& key)
    {
        auto it = std::lower_bound(std::begin(_keys), std::end(_keys), key, key_compare{});
        if (it != std::end(_keys) && !key_compare{}(key, *it))
        {
            return {{&*it, &*std::next(std::begin(_values), std::distance(std::begin(_keys), it))}};
        }

        return end();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::find(const key_type& key) const
    {
        auto it = std::lower_bound(std::begin(_keys), std::end(_keys), key, key_compare{});
        if (it != std::end(_keys) && !key_compare{}(key, *it))
        {
            return {{&*it, &*std::next(std::begin(_values), std::distance(std::begin(_keys), it))}};
        }

        return end();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::mapped_type& flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::operator[](const key_type& key)
    {
        auto it = std::lower_bound(std::begin(_keys), std::end(_keys), key, key_compare{});
        if (it != std::end(_keys) && !key_compare{}(key, *it))
        {
            return *std::next(std::begin(_values), std::distance(std::begin(_keys), it));
        }

        auto key_it = std::next(std::begin(_keys), std::distance(std::begin(_keys), it));
        auto value_it = std::next(std::begin(_values), std::distance(std::begin(_keys), it));

        _keys.insert(key_it, key);
        value_it = _values.insert(value_it, mapped_type{});

        return *value_it;
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::size_type flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::count(const key_type& key) const
    {
        return find(key) != end() ? 1 : 0;
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline bool flat_map<K, V, Compare, KeyContainer, ValueContainer>::contains(const key_type& key) const
    {
        return find(key) != end();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::lower_bound(const key_type& key)
    {
        auto it = std::lower_bound(std::begin(_keys), std::end(_keys), key, key_compare{});
        return {{&*it, &*std::next(std::begin(_values), std::distance(std::begin(_keys), it))}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::lower_bound(const key_type& key) const
    {
        auto it = std::lower_bound(std::begin(_keys), std::end(_keys), key, key_compare{});
        return {{&*it, &*std::next(std::begin(_values), std::distance(std::begin(_keys), it))}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::upper_bound(const key_type& key)
    {
        auto it = std::upper_bound(std::begin(_keys), std::end(_keys), key, key_compare{});
        return {{&*it, &*std::next(std::begin(_values), std::distance(std::begin(_keys), it))}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::upper_bound(const key_type& key) const
    {
        auto it = std::upper_bound(std::begin(_keys), std::end(_keys), key, key_compare{});
        return {{&*it, &*std::next(std::begin(_values), std::distance(std::begin(_keys), it))}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline std::pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator,
                     typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator>
    flat_map<K, V, Compare, KeyContainer, ValueContainer>::equal_range(const key_type& key)
    {
        auto lower = lower_bound(key);
        auto upper = upper_bound(key);
        return {lower, upper};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline std::pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator,
                     typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator>
    flat_map<K, V, Compare, KeyContainer, ValueContainer>::equal_range(const key_type& key) const
    {
        auto lower = lower_bound(key);
        auto upper = upper_bound(key);
        return {lower, upper};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::key_compare flat_map<K, V, Compare, KeyContainer,
                                                                                       ValueContainer>::key_comp() const
    {
        return _key_comparator;
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::value_compare flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::value_comp() const
    {
        return _value_comparator;
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline const typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::key_container_type& flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::keys() const noexcept
    {
        return _keys;
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline const typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::value_container_type& flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::values() const noexcept
    {
        return _values;
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline void swap(flat_map<K, V, Compare, KeyContainer, ValueContainer>& lhs,
                     flat_map<K, V, Compare, KeyContainer, ValueContainer>& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline bool operator==(const flat_map<K, V, Compare, KeyContainer, ValueContainer>& lhs,
                           const flat_map<K, V, Compare, KeyContainer, ValueContainer>& rhs)
    {
        return lhs.size() == rhs.size() && std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs));
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline auto operator<=>(const flat_map<K, V, Compare, KeyContainer, ValueContainer>& lhs,
                            const flat_map<K, V, Compare, KeyContainer, ValueContainer>& rhs)
    {
        return std::lexicographical_compare_three_way(std::begin(lhs), std::end(lhs), std::begin(rhs), std::end(rhs));
    }
} // namespace tempest::core

#endif // tempest_core_flat_map_hpp