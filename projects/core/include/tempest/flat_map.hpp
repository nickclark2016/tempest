#ifndef tempest_core_flat_map_hpp
#define tempest_core_flat_map_hpp

#include <tempest/functional.hpp>
#include <tempest/int.hpp>
#include <tempest/iterator.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

#include <compare>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace tempest
{
    namespace detail
    {
        template <typename K, typename V>
        struct flat_map_iterator_ptr_adapter
        {
            pair<const K&, V&> p;

            friend auto operator<=>(const flat_map_iterator_ptr_adapter& lhs,
                                    const flat_map_iterator_ptr_adapter& rhs) noexcept = default;

            pair<const K&, V&> operator*() noexcept;
            pair<const K&, const V&> operator*() const noexcept;
            pair<const K&, V&>* operator->() noexcept;
            const pair<const K&, V&>* operator->() const noexcept;
        };

        template <typename K, typename V>
        inline pair<const K&, V&> flat_map_iterator_ptr_adapter<K, V>::operator*() noexcept
        {
            return p;
        }

        template <typename K, typename V>
        inline pair<const K&, const V&> flat_map_iterator_ptr_adapter<K, V>::operator*() const noexcept
        {
            return {p.first, p.second};
        }

        template <typename K, typename V>
        inline pair<const K&, V&>* flat_map_iterator_ptr_adapter<K, V>::operator->() noexcept
        {
            return &p;
        }

        template <typename K, typename V>
        inline const pair<const K&, V&>* flat_map_iterator_ptr_adapter<K, V>::operator->() const noexcept
        {
            return &p;
        }

        template <typename K, typename V>
        struct flat_map_iterator
        {
            using value_type = pair<const K, V>;
            using difference_type = ptrdiff_t;
            using reference = pair<const K&, V&>;
            using pointer = flat_map_iterator_ptr_adapter<K, V>;
            using const_reference = pair<const K&, const V&>;
            using const_pointer = flat_map_iterator_ptr_adapter<K, const V>;

            pair<const K*, V*> p;

            friend auto operator<=>(const flat_map_iterator& lhs, const flat_map_iterator& rhs) noexcept = default;

            flat_map_iterator& operator++() noexcept;
            flat_map_iterator operator++(int) noexcept;
            flat_map_iterator& operator--() noexcept;
            flat_map_iterator operator--(int) noexcept;

            flat_map_iterator& operator+=(difference_type n) noexcept;
            flat_map_iterator& operator-=(difference_type n) noexcept;
            flat_map_iterator operator+(difference_type n) const noexcept;
            flat_map_iterator operator-(difference_type n) const noexcept;

            difference_type operator-(const flat_map_iterator& other) const noexcept;

            reference operator[](difference_type n) noexcept;
            const_reference operator[](difference_type n) const noexcept;

            reference operator*() noexcept;
            const_reference operator*() const noexcept;

            pointer operator->() noexcept;
            const_pointer operator->() const noexcept;

            bool operator==(const flat_map_iterator& other) const noexcept;

            operator flat_map_iterator<K, const V>() noexcept;
        };

        template <typename K, typename V>
        inline flat_map_iterator<K, V>& flat_map_iterator<K, V>::operator++() noexcept
        {
            ++p.first;
            ++p.second;
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
            --p.first;
            --p.second;
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
        inline flat_map_iterator<K, V>& flat_map_iterator<K, V>::operator+=(difference_type n) noexcept
        {
            p.first += n;
            p.second += n;
            return *this;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V>& flat_map_iterator<K, V>::operator-=(difference_type n) noexcept
        {
            p.first -= n;
            p.second -= n;
            return *this;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V> flat_map_iterator<K, V>::operator+(difference_type n) const noexcept
        {
            auto copy = *this;
            copy += n;
            return copy;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V> flat_map_iterator<K, V>::operator-(difference_type n) const noexcept
        {
            auto copy = *this;
            copy -= n;
            return copy;
        }

        template <typename K, typename V>
        inline typename flat_map_iterator<K, V>::difference_type flat_map_iterator<K, V>::operator-(
            const flat_map_iterator& other) const noexcept
        {
            return p.first - other.p.first;
        }

        template <typename K, typename V>
        inline typename flat_map_iterator<K, V>::reference flat_map_iterator<K, V>::operator[](
            difference_type n) noexcept
        {
            return pair<const K&, V&>(p.first[n], p.second[n]);
        }

        template <typename K, typename V>
        inline typename flat_map_iterator<K, V>::const_reference flat_map_iterator<K, V>::operator[](
            difference_type n) const noexcept
        {
            return pair<const K&, const V&>(p.first[n], p.second[n]);
        }

        template <typename K, typename V>
        inline typename flat_map_iterator<K, V>::reference flat_map_iterator<K, V>::operator*() noexcept
        {
            return pair<const K&, V&>(*p.first, *p.second);
        }

        template <typename K, typename V>
        inline typename flat_map_iterator<K, V>::const_reference flat_map_iterator<K, V>::operator*() const noexcept
        {
            return pair<const K&, const V&>(*p.first, *p.second);
        }

        template <typename K, typename V>
        inline typename flat_map_iterator<K, V>::pointer flat_map_iterator<K, V>::operator->() noexcept
        {
            return {pair<const K&, V&>(*p.first, *p.second)};
        }

        template <typename K, typename V>
        inline typename flat_map_iterator<K, V>::const_pointer flat_map_iterator<K, V>::operator->() const noexcept
        {
            return {pair<const K&, const V&>(*p.first, *p.second)};
        }

        template <typename K, typename V>
        inline bool flat_map_iterator<K, V>::operator==(const flat_map_iterator& other) const noexcept
        {
            return p.first == other.p.first;
        }

        template <typename K, typename V>
        inline flat_map_iterator<K, V>::operator flat_map_iterator<K, const V>() noexcept
        {
            return {{p.first, p.second}};
        }
    } // namespace detail

    template <typename K, typename V, typename Compare = tempest::less<K>, typename KeyContainer = tempest::vector<K>,
              typename ValueContainer = tempest::vector<V>>
    class flat_map
    {
      public:
        using key_type = K;
        using mapped_type = V;
        using key_compare = Compare;
        using value_type = pair<K, V>;
        using reference = pair<const K&, V&>;
        using const_reference = pair<const K&, const V&>;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
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

        pair<iterator, bool> insert(const value_type& value);
        pair<iterator, bool> insert(value_type&& value);

        template <typename O>
        pair<iterator, bool> insert_or_assign(const key_type& k, O&& obj);

        template <typename O>
        pair<iterator, bool> insert_or_assign(key_type&& k, O&& obj);

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

        pair<iterator, iterator> equal_range(const key_type& key);
        pair<const_iterator, const_iterator> equal_range(const key_type& key) const;

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
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator begin(
        flat_map<K, V, Compare, KeyContainer, ValueContainer>& map) noexcept
    {
        return map.begin();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator begin(
        const flat_map<K, V, Compare, KeyContainer, ValueContainer>& map) noexcept
    {
        return map.begin();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator cbegin(
        const flat_map<K, V, Compare, KeyContainer, ValueContainer>& map) noexcept
    {
        return map.cbegin();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator end(
        flat_map<K, V, Compare, KeyContainer, ValueContainer>& map) noexcept
    {
        return map.end();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator end(
        const flat_map<K, V, Compare, KeyContainer, ValueContainer>& map) noexcept
    {
        return map.end();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator cend(
        const flat_map<K, V, Compare, KeyContainer, ValueContainer>& map) noexcept
    {
        return map.cend();
    }

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
        return {{tempest::data(_keys), tempest::data(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::begin() const noexcept
    {
        return {{tempest::data(_keys), tempest::data(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::cbegin() const noexcept
    {
        return {{tempest::data(_keys), tempest::data(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<K, V, Compare, KeyContainer,
                                                                                    ValueContainer>::end() noexcept
    {
        return {{tempest::data(_keys) + tempest::size(_keys), tempest::data(_values) + tempest::size(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::end() const noexcept
    {
        return {{tempest::data(_keys) + tempest::size(_keys), tempest::data(_values) + tempest::size(_values)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::cend() const noexcept
    {
        return {{tempest::data(_keys) + tempest::size(_keys), tempest::data(_values) + tempest::size(_values)}};
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
        return tempest::empty(_keys);
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::size_type flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::size() const noexcept
    {
        return tempest::size(_keys);
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
    inline pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator, bool> flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::insert(const value_type& value)
    {
        auto it = std::lower_bound(tempest::begin(_keys), tempest::end(_keys), value.first, key_compare{});
        if (it != tempest::end(_keys) && !key_compare{}(value.first, *it))
        {
            return {iterator{
                        {&*it, &*tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it))}},
                    false};
        }

        auto key_it = tempest::next(tempest::begin(_keys), tempest::distance(tempest::begin(_keys), it));
        auto value_it = tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it));

        key_it = _keys.insert(key_it, value.first);
        value_it = _values.insert(value_it, value.second);

        return {iterator{{&*key_it, &*value_it}}, true};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator, bool> flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::insert(value_type&& value)
    {
        auto it = std::lower_bound(tempest::begin(_keys), tempest::end(_keys), value.first, key_compare{});
        if (it != tempest::end(_keys) && !key_compare{}(value.first, *it))
        {
            return {iterator{
                        {&*it, &*tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it))}},
                    false};
        }

        auto key_it = tempest::next(tempest::begin(_keys), tempest::distance(tempest::begin(_keys), it));
        auto value_it = tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it));

        key_it = _keys.insert(key_it, tempest::move(value.first));
        value_it = _values.insert(value_it, tempest::move(value.second));

        return {iterator{{&*key_it, &*value_it}}, true};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    template <typename O>
    inline pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator, bool> flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::insert_or_assign(const key_type& k, O&& obj)
    {
        auto it = std::lower_bound(tempest::begin(_keys), tempest::end(_keys), k, key_compare{});
        if (it != tempest::end(_keys) && !key_compare{}(k, *it))
        {
            auto key_it = tempest::next(tempest::begin(_keys), tempest::distance(tempest::begin(_keys), it));
            auto value_it = tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it));

            *value_it = tempest::forward<O>(obj);

            return {iterator{{&*key_it, &*value_it}}, false};
        }

        auto key_it = tempest::next(tempest::begin(_keys), tempest::distance(tempest::begin(_keys), it));
        auto value_it = tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it));

        key_it = _keys.insert(key_it, k);
        value_it = _values.insert(value_it, tempest::forward<O>(obj));

        return {iterator{{&*key_it, &*value_it}}, true};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    template <typename O>
    inline pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator, bool> flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::insert_or_assign(key_type&& k, O&& obj)
    {
        auto it = std::lower_bound(tempest::begin(_keys), tempest::end(_keys), k, key_compare{});
        if (it != tempest::end(_keys) && !key_compare{}(k, *it))
        {
            auto key_it = tempest::next(tempest::begin(_keys), tempest::distance(tempest::begin(_keys), it));
            auto value_it = tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it));

            *value_it = tempest::forward<O>(obj);

            return {iterator{{&*key_it, &*value_it}}, false};
        }

        auto key_it = tempest::next(tempest::begin(_keys), tempest::distance(tempest::begin(_keys), it));
        auto value_it = tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it));

        key_it = _keys.insert(key_it, tempest::move(k));
        value_it = _values.insert(value_it, tempest::forward<O>(obj));

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
        return {tempest::move(_keys), tempest::move(_values)};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline void flat_map<K, V, Compare, KeyContainer, ValueContainer>::replace(KeyContainer&& keys,
                                                                               ValueContainer&& values)
    {
        _keys = tempest::move(keys);
        _values = tempest::move(values);
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::erase(iterator position)
    {
        const key_type* keys_ptr = tempest::data(_keys);

        auto idx = tempest::distance(keys_ptr, position.p.first);

        auto key_it = tempest::next(tempest::begin(_keys), idx);
        auto value_it = tempest::next(tempest::begin(_values), idx);

        key_it = _keys.erase(key_it);
        value_it = _values.erase(value_it);

        return {{tempest::data(_keys) + tempest::distance(tempest::begin(_keys), key_it),
                 tempest::data(_values) + tempest::distance(tempest::begin(_values), value_it)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::erase(const_iterator position)
    {
        const key_type* keys_ptr = tempest::data(_keys);

        auto idx = tempest::distance(keys_ptr, position.p.first);

        auto key_it = tempest::next(tempest::begin(_keys), idx);
        auto value_it = tempest::next(tempest::begin(_values), idx);

        key_it = _keys.erase(key_it);
        value_it = _values.erase(value_it);

        return {{tempest::data(_keys) + tempest::distance(tempest::begin(_keys), key_it),
                 tempest::data(_values) + tempest::distance(tempest::begin(_values), value_it)}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::erase(const_iterator first, const_iterator last)
    {
        const key_type* keys_ptr = tempest::data(_keys);

        auto first_idx = tempest::distance(keys_ptr, first.p.first);
        auto last_idx = tempest::distance(keys_ptr, last.p.first);

        auto key_first = tempest::next(tempest::begin(_keys), first_idx);
        auto key_last = tempest::next(tempest::begin(_keys), last_idx);
        auto value_first = tempest::next(tempest::begin(_values), first_idx);
        auto value_last = tempest::next(tempest::begin(_values), last_idx);

        key_first = _keys.erase(key_first, key_last);
        value_first = _values.erase(value_first, value_last);

        return {{tempest::data(_keys) + tempest::distance(tempest::begin(_keys), key_first),
                 tempest::data(_values) + tempest::distance(tempest::begin(_values), value_first)}};
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
        using tempest::swap;

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
        auto it = std::lower_bound(tempest::begin(_keys), tempest::end(_keys), key, key_compare{});
        if (it != tempest::end(_keys) && !key_compare{}(key, *it))
        {
            return {{&*it, &*tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it))}};
        }

        return end();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::find(const key_type& key) const
    {
        auto it = std::lower_bound(tempest::begin(_keys), tempest::end(_keys), key, key_compare{});
        if (it != tempest::end(_keys) && !key_compare{}(key, *it))
        {
            return {{&*it, &*tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it))}};
        }

        return end();
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline flat_map<K, V, Compare, KeyContainer, ValueContainer>::mapped_type& flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::operator[](const key_type& key)
    {
        auto it = std::lower_bound(tempest::begin(_keys), tempest::end(_keys), key, key_compare{});
        if (it != tempest::end(_keys) && !key_compare{}(key, *it))
        {
            return *tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it));
        }

        auto key_it = tempest::next(tempest::begin(_keys), tempest::distance(tempest::begin(_keys), it));
        auto value_it = tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it));

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
        auto it = std::lower_bound(tempest::begin(_keys), tempest::end(_keys), key, key_compare{});
        return {{&*it, &*tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it))}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::lower_bound(const key_type& key) const
    {
        auto it = std::lower_bound(tempest::begin(_keys), tempest::end(_keys), key, key_compare{});
        return {{&*it, &*tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it))}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::upper_bound(const key_type& key)
    {
        auto it = std::upper_bound(tempest::begin(_keys), tempest::end(_keys), key, key_compare{});
        return {{&*it, &*tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it))}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator flat_map<
        K, V, Compare, KeyContainer, ValueContainer>::upper_bound(const key_type& key) const
    {
        auto it = std::upper_bound(tempest::begin(_keys), tempest::end(_keys), key, key_compare{});
        return {{&*it, &*tempest::next(tempest::begin(_values), tempest::distance(tempest::begin(_keys), it))}};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator,
                typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::iterator>
    flat_map<K, V, Compare, KeyContainer, ValueContainer>::equal_range(const key_type& key)
    {
        auto lower = lower_bound(key);
        auto upper = upper_bound(key);
        return {lower, upper};
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline pair<typename flat_map<K, V, Compare, KeyContainer, ValueContainer>::const_iterator,
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
        return lhs.size() == rhs.size() && std::equal(tempest::begin(lhs), tempest::end(lhs), tempest::begin(rhs));
    }

    template <typename K, typename V, typename Compare, typename KeyContainer, typename ValueContainer>
    inline auto operator<=>(const flat_map<K, V, Compare, KeyContainer, ValueContainer>& lhs,
                            const flat_map<K, V, Compare, KeyContainer, ValueContainer>& rhs)
    {
        return std::lexicographical_compare_three_way(tempest::begin(lhs), tempest::end(lhs), tempest::begin(rhs),
                                                      tempest::end(rhs));
    }
} // namespace tempest

#endif // tempest_core_flat_map_hpp