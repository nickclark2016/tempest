#ifndef tempest_core_iterator_hpp
#define tempest_core_iterator_hpp

#include <tempest/concepts.hpp>
#include <tempest/move.hpp>

namespace tempest
{
    template <typename It>
    struct incrementable_traits
    {
    };

    template <typename It>
        requires is_object_v<It>
    struct incrementable_traits<It*>
    {
        using difference_type = ptrdiff_t;
    };

    template <typename It>
    struct incrementable_traits<const It> : incrementable_traits<It>
    {
    };

    template <typename It>
        requires requires { typename It::difference_type; }
    struct incrementable_traits<It>
    {
        using difference_type = It::difference_type;
    };

    template <typename It>
        requires(!requires { typename It::difference_type; }) && requires(const It& a, const It& b) {
            { b - a } -> integral;
        }
    struct incrementable_traits<It>
    {
        using difference_type = make_signed_t<decltype(declval<It>() - declval<It>())>;
    };

    template <typename T>
    using iter_difference_t = incrementable_traits<T>::difference_type;

    template <typename It>
    struct indirectly_readable_traits
    {
    };

    template <typename It>
        requires is_object_v<It>
    struct indirectly_readable_traits<It*>
    {
        using value_type = remove_cv_t<It>;
    };

    template <typename It>
        requires is_array_v<It>
    struct indirectly_readable_traits<It>
    {
        using value_type = remove_cv_t<remove_extent_t<It>>;
    };

    template <typename It>
    struct indirectly_readable_traits<const It> : indirectly_readable_traits<It>
    {
    };

    template <typename It>
        requires requires { typename It::value_type; }
    struct indirectly_readable_traits<It>
    {
        using value_type = It::value_type;
    };

    template <typename It>
        requires requires { typename It::element_type; }
    struct indirectly_readable_traits<It>
    {
        using value_type = remove_cv_t<typename It::element_type>;
    };

    template <typename It>
        requires requires {
            typename It::value_type;
            typename It::element_type;
        } && same_as<remove_cv_t<typename It::value_type>, remove_cv_t<typename It::element_type>>
    struct indirectly_readable_traits<It>
    {
        using value_type = remove_cv_t<typename It::value_type>;
    };

    template <typename It>
        requires requires {
            typename It::value_type;
            typename It::element_type;
        }
    struct indirectly_readable_traits<It>
    {
    };

    template <typename T>
    concept indirectly_readable = requires(T t) { typename indirectly_readable_traits<T>::value_type; };

    template <indirectly_readable It>
    using iter_reference_t = decltype(*declval<It&>());

    template <indirectly_readable It>
    using iter_value_t = remove_cvref_t<iter_reference_t<It>>;

    template <typename O, typename T>
    concept indirectly_writable = requires(O&& o, T&& t) {
        *o = tempest::forward<T>(t);
        *tempest::forward<O>(o) = tempest::forward<T>(t);
        const_cast<iter_reference_t<O>&&>(*o) = tempest::forward<T>(t);
        const_cast<iter_reference_t<O>&&>(*tempest::forward<O>(o)) = tempest::forward<T>(t);
    };

    template <typename It>
    concept weakly_iteratable = movable<It> && requires(It it) {
        typename iter_difference_t<It>;
        requires signed_integral<iter_difference_t<It>>;
        { ++it } -> same_as<It&>;
        it++;
    };

    template <typename It>
    concept iterator = requires(It it) {
        { *it } -> detail::can_reference;
        { ++it } -> same_as<It&>;
        { *it++ } -> detail::can_reference;
    } && copyable<It>;

    template <typename It>
    concept input_or_output_iterator = requires(It it) {
        { *it } -> detail::can_reference;
    } && weakly_iteratable<It>;

    template <typename S, typename I>
    concept sentinel_for = semiregular<S> && input_or_output_iterator<I> && detail::weakly_eq_cmp_with<S, I>;

    template <typename It>
    concept input_iterator = iterator<It> && equality_comparable<It> && requires(It it) {
        typename incrementable_traits<It>::difference_type;
        typename indirectly_readable_traits<It>::value_type;
        typename common_reference_t<iter_reference_t<It>&&, typename indirectly_readable_traits<It>::value_type&>;
        *it++;
        typename common_reference_t<decltype(*++it)&&, iter_reference_t<It>&>;

        requires signed_integral<typename incrementable_traits<It>::difference_type>;
    };

    template <typename It, typename T>
    concept output_iterator = input_or_output_iterator<It> && indirectly_writable<It, T> &&
                              requires(It it, T&& t) { *it++ = tempest::forward<T>(t); };

    template <typename It>
    concept forward_iterator =
        input_iterator<It> &&
        sentinel_for<It, It> &&
        requires(It it) {
            { it++ } -> convertible_to<const It&>;
            { *it++ } -> same_as<iter_reference_t<It>>;
        };

    template <typename It>
    concept bidirectional_iterator = forward_iterator<It> && requires(It it) {
        { --it } -> same_as<It&>;
        { it-- } -> convertible_to<const It&>;
        { *it-- } -> same_as<iter_reference_t<It>>;
    };

    template <typename It>
    concept random_access_iterator = bidirectional_iterator<It> && totally_ordered<It> &&
                                     requires(It it, incrementable_traits<It>::difference_type n) {
                                         { it += n } -> same_as<It&>;
                                         { it -= n } -> same_as<It&>;
                                         { it + n } -> same_as<It>;
                                         { n + it } -> same_as<It>;
                                         { it - n } -> same_as<It>;
                                         { it - it } -> same_as<decltype(n)>;
                                         { it[n] } -> same_as<iter_reference_t<It>>;
                                     };

    template <typename It>
    concept contiguous_iterator =
        random_access_iterator<It> && is_lvalue_reference_v<iter_reference_t<It>> &&
        same_as<iter_value_t<It>, remove_cvref_t<iter_reference_t<It>>> && requires(const It& it) {
            { &*it } -> same_as<add_pointer_t<iter_reference_t<It>>>;
        };

    namespace detail
    {
        template <typename T>
        concept has_pointer_type_test = requires { typename T::pointer; };

        template <typename T>
        concept has_arrow_operator_test = requires { declval<T&>().operator->(); };

        template <typename T>
        concept has_reference_test = requires { typename T::reference; };

        template <typename T, bool, bool>
        struct pointer_type_impl;

        template <typename T>
        struct pointer_type_impl<T, true, false>
        {
            using type = T::pointer;
        };

        template <typename T>
        struct pointer_type_impl<T, false, true>
        {
            using type = decltype(declval<T&>().operator->());
        };

        template <typename T>
        struct pointer_type_impl<T, true, true>
        {
            using type = T::pointer;
        };

        template <typename T>
        struct pointer_type_impl<T, false, false>
        {
            using type = void;
        };

        template <typename T>
        using pointer_type = pointer_type_impl<T, has_pointer_type_test<T>, has_arrow_operator_test<T>>::type;

        template <typename T, bool>
        struct reference_type_impl;

        template <typename T>
        struct reference_type_impl<T, true>
        {
            using type = T::reference;
        };

        template <typename T>
        struct reference_type_impl<T, false>
        {
            using type = iter_reference_t<T>;
        };

        template <typename T>
        using reference_type = reference_type_impl<T, has_reference_test<T>>::type;
    } // namespace detail

    template <typename It>
    struct iterator_traits
    {
        using difference_type = incrementable_traits<It>::difference_type;
        using value_type = indirectly_readable_traits<It>::value_type;
        using pointer = detail::pointer_type<It>;
        using reference = detail::reference_type<It>;
    };

    template <input_iterator It, typename D>
    constexpr void advance(It& it, D n)
    {
        if constexpr (contiguous_iterator<It>)
        {
            it += n;
        }
        else
        {
            while (n > 0)
            {
                ++it;
                --n;
            }

            if constexpr (random_access_iterator<It>)
            {
                while (n < 0)
                {
                    --it;
                    ++n;
                }
            }
        }
    }

    template <input_iterator It>
    constexpr auto next(It it, typename iterator_traits<It>::difference_type n = 1) -> It
    {
        advance(it, n);
        return it;
    }

    template <bidirectional_iterator It>
    constexpr auto prev(It it, typename iterator_traits<It>::difference_type n = 1) -> It
    {
        advance(it, -n);
        return it;
    }

    template <input_iterator It>
    constexpr auto distance(It first, It last) -> iter_difference_t<It>
    {
        if constexpr (contiguous_iterator<It>)
        {
            return last - first;
        }
        else
        {
            iter_difference_t<It> n = 0;

            while (first != last)
            {
                ++first;
                ++n;
            }

            return n;
        }
    }

    template <typename T>
    constexpr auto begin(T& t) -> decltype(t.begin())
    {
        return t.begin();
    }

    template <typename T>
    constexpr auto begin(const T& t) -> decltype(t.begin())
    {
        return t.begin();
    }

    template <typename T, size_t N>
    constexpr auto begin(T (&array)[N]) noexcept
    {
        return array;
    }

    template <typename T>
    constexpr auto cbegin(const T& t) -> decltype(begin(t))
    {
        return t.cbegin();
    }

    template <typename T>
    constexpr auto end(T& t) -> decltype(t.end())
    {
        return t.end();
    }

    template <typename T>
    constexpr auto end(const T& t) -> decltype(t.end())
    {
        return t.end();
    }

    template <typename T, size_t N>
    constexpr auto end(T (&array)[N]) noexcept
    {
        return array + N;
    }

    template <typename T>
    constexpr auto cend(const T& t) -> decltype(end(t))
    {
        return t.cend();
    }

    template <typename T>
    constexpr auto size(const T& t) -> decltype(t.size())
    {
        return t.size();
    }

    template <typename T>
    constexpr auto ssize(const T& t) -> common_type_t<make_signed_t<decltype(t.size())>, ptrdiff_t>
    {
        return static_cast<make_signed_t<decltype(t.size())>>(t.size());
    }

    template <typename T>
    constexpr auto data(T& t) -> decltype(t.data())
    {
        return t.data();
    }

    template <typename T>
    constexpr auto data(const T& t) -> decltype(t.data())
    {
        return t.data();
    }

    template <bidirectional_iterator It>
    class reverse_iterator
    {
      public:
        using iterator_type = It;
        using value_type = iter_value_t<It>;
        using difference_type = iter_difference_t<It>;
        using pointer = iterator_traits<It>::pointer;
        using reference = iterator_traits<It>::reference;

        constexpr reverse_iterator() noexcept = default;
        constexpr explicit reverse_iterator(It it) noexcept;

        template <typename U>
            requires convertible_to<U, It>
        constexpr reverse_iterator(const reverse_iterator<U>& other) noexcept;

        template <typename U>
            requires convertible_to<U, It>
        constexpr auto operator=(const reverse_iterator<U>& other) noexcept -> reverse_iterator&;

        constexpr auto base() const noexcept -> iterator_type;

        constexpr auto operator[](difference_type n) const noexcept -> reference;

        constexpr auto operator++() noexcept -> reverse_iterator&;
        constexpr auto operator++(int) noexcept -> reverse_iterator;
        constexpr auto operator--() noexcept -> reverse_iterator&;
        constexpr auto operator--(int) noexcept -> reverse_iterator;
        constexpr auto operator+(difference_type n) const noexcept -> reverse_iterator;
        constexpr auto operator+=(difference_type n) noexcept -> reverse_iterator&;
        constexpr auto operator-(difference_type n) const noexcept -> reverse_iterator;
        constexpr auto operator-=(difference_type n) noexcept -> reverse_iterator&;

        constexpr auto operator*() const noexcept -> reference;
        constexpr auto operator->() const noexcept -> pointer
            requires(is_pointer_v<It> || requires(const It it) { it.operator->(); });

      private:
        It _it;
    };

    template <bidirectional_iterator It>
    constexpr reverse_iterator<It>::reverse_iterator(It it) noexcept : _it(it)
    {
    }

    template <bidirectional_iterator It>
    template <typename U>
        requires convertible_to<U, It>
    constexpr reverse_iterator<It>::reverse_iterator(const reverse_iterator<U>& other) noexcept
        : _it(other.base())
    {
    }

    template <bidirectional_iterator It>
    template <typename U>
        requires convertible_to<U, It>
    constexpr auto reverse_iterator<It>::operator=(const reverse_iterator<U>& other) noexcept -> reverse_iterator<It>&
    {
        _it = other.base();
        return *this;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::base() const noexcept -> It
    {
        return _it;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator[](
        difference_type n) const noexcept -> reverse_iterator<It>::reference
    {
        return *(*this + n);
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator++() noexcept -> reverse_iterator<It>&
    {
        --_it;
        return *this;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator++(int) noexcept -> reverse_iterator<It>
    {
        auto copy = *this;
        --_it;
        return copy;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator--() noexcept -> reverse_iterator<It>&
    {
        ++_it;
        return *this;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator--(int) noexcept -> reverse_iterator<It>
    {
        auto copy = *this;
        ++_it;
        return copy;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator+(difference_type n) const noexcept -> reverse_iterator<It>
    {
        auto copy = *this;
        copy += n;
        return copy;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator+=(difference_type n) noexcept -> reverse_iterator<It>&
    {
        if constexpr (random_access_iterator<It>)
        {
            _it -= n;
        }
        else
        {
            // Loop to decrement
            while (n > 0)
            {
                --_it;
                --n;
            }
        }

        return *this;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator-(difference_type n) const noexcept -> reverse_iterator<It>
    {
        auto copy = *this;
        copy -= n;
        return copy;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator-=(difference_type n) noexcept -> reverse_iterator<It>&
    {
        if constexpr (random_access_iterator<It>)
        {
            _it += n;
        }
        else
        {
            // Loop to increment
            while (n > 0)
            {
                ++_it;
                --n;
            }
        }
        return *this;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator*() const noexcept -> reverse_iterator<It>::reference
    {
        auto copy = _it;
        return *--copy;
    }

    template <bidirectional_iterator It>
    constexpr auto reverse_iterator<It>::operator->() const noexcept -> reverse_iterator<It>::pointer
        requires(is_pointer_v<It> || requires(const It it) { it.operator->(); })
    {
        return &this->operator*();
    }

    template <bidirectional_iterator It>
    constexpr auto operator==(const reverse_iterator<It>& lhs, const reverse_iterator<It>& rhs) noexcept -> bool
    {
        return lhs.base() == rhs.base();
    }

    template <bidirectional_iterator It>
    constexpr auto operator!=(const reverse_iterator<It>& lhs, const reverse_iterator<It>& rhs) noexcept -> bool
    {
        return !(lhs == rhs);
    }

    template <bidirectional_iterator It>
    constexpr auto operator<=>(const reverse_iterator<It>& lhs, const reverse_iterator<It>& rhs) noexcept
    {
        return lhs.base() <=> rhs.base();
    }

    template <bidirectional_iterator It>
    constexpr auto operator+(typename reverse_iterator<It>::difference_type n,
                                                    const reverse_iterator<It>& it) noexcept -> reverse_iterator<It>
    {
        return it + n;
    }

    template <bidirectional_iterator It>
    constexpr auto operator-(typename reverse_iterator<It>::difference_type n,
                                                    const reverse_iterator<It>& it) noexcept -> reverse_iterator<It>
    {
        return it - n;
    }

    template <bidirectional_iterator It>
    constexpr auto make_reverse_iterator(It it) noexcept -> reverse_iterator<It>
    {
        return reverse_iterator<It>(it);
    }

    template <typename T>
    concept iterable = requires(T& t) {
        { begin(t) } -> input_or_output_iterator;
        { end(t) } -> sentinel_for<decltype(begin(t))>;
    };

    template <typename S, typename I>
    inline constexpr bool disabled_sized_sentinel_for = false;

    template <typename S, typename I>
    concept sized_sentinel_for =
        sentinel_for<S, I> && !disabled_sized_sentinel_for<remove_cvref_t<S>, remove_cvref_t<I>> &&
        requires(const I& i, const S& s) {
            { s - i } -> same_as<iter_difference_t<I>>;
            { i - s } -> same_as<iter_difference_t<I>>;
        };

    template <typename Container>
    class back_insert_iterator
    {
      public:
        using value_type = void;
        using difference_type = ptrdiff_t;
        using pointer = void;
        using reference = void;
        using container_type = Container;

        constexpr back_insert_iterator() noexcept = default;

        constexpr explicit back_insert_iterator(Container& c) noexcept
            : _container{__builtin_addressof(c)}
        {
        }

        constexpr auto operator=(const Container::value_type& value) -> back_insert_iterator&
        {
            _container->push_back(value);
            return *this;
        }

        constexpr auto operator=(Container::value_type&& value) -> back_insert_iterator&
        {
            _container->push_back(tempest::move(value));
            return *this;
        }

        [[nodiscard]] constexpr auto operator*() noexcept -> back_insert_iterator&
        {
            return *this;
        }

        constexpr auto operator++() noexcept -> back_insert_iterator&
        {
            return *this;
        }

        constexpr auto operator++(int) noexcept -> back_insert_iterator
        {
            return *this;
        }

      protected:
        Container* _container{nullptr};
    };

    template <typename Container>
    [[nodiscard]] constexpr auto back_inserter(Container& c) noexcept -> back_insert_iterator<Container>
    {
        return back_insert_iterator<Container>(c);
    }

    template <typename Container>
    class front_insert_iterator
    {
      public:
        using value_type = void;
        using difference_type = ptrdiff_t;
        using pointer = void;
        using reference = void;
        using container_type = Container;

        constexpr front_insert_iterator() noexcept = default;

        constexpr explicit front_insert_iterator(Container& c) noexcept
            : _container{__builtin_addressof(c)}
        {
        }

        constexpr auto operator=(const Container::value_type& value) -> front_insert_iterator&
        {
            _container->push_front(value);
            return *this;
        }

        constexpr auto operator=(Container::value_type&& value) -> front_insert_iterator&
        {
            _container->push_front(tempest::move(value));
            return *this;
        }

        [[nodiscard]] constexpr auto operator*() noexcept -> front_insert_iterator&
        {
            return *this;
        }

        constexpr auto operator++() noexcept -> front_insert_iterator&
        {
            return *this;
        }

        constexpr auto operator++(int) noexcept -> front_insert_iterator
        {
            return *this;
        }

      protected:
        Container* _container{nullptr};
    };

    template <typename Container>
    [[nodiscard]] constexpr auto front_inserter(Container& c) noexcept -> front_insert_iterator<Container>
    {
        return front_insert_iterator<Container>(c);
    }

    template <typename Container>
    class insert_iterator
    {
      public:
        using value_type = void;
        using difference_type = ptrdiff_t;
        using pointer = void;
        using reference = void;
        using container_type = Container;

        constexpr insert_iterator() noexcept = default;

        constexpr insert_iterator(Container& c, Container::iterator i) noexcept
            : _container{__builtin_addressof(c)}, _iter{i}
        {
        }

        constexpr auto operator=(const Container::value_type& value) -> insert_iterator&
        {
            _iter = _container->insert(_iter, value);
            ++_iter;
            return *this;
        }

        constexpr auto operator=(Container::value_type&& value) -> insert_iterator&
        {
            _iter = _container->insert(_iter, tempest::move(value));
            ++_iter;
            return *this;
        }

        [[nodiscard]] constexpr auto operator*() noexcept -> insert_iterator&
        {
            return *this;
        }

        constexpr auto operator++() noexcept -> insert_iterator&
        {
            return *this;
        }

        constexpr auto operator++(int) noexcept -> insert_iterator
        {
            return *this;
        }

      protected:
        Container* _container{nullptr};
        Container::iterator _iter{};
    };

    template <typename Container>
    [[nodiscard]] constexpr auto inserter(Container& c, typename Container::iterator i) noexcept
        -> insert_iterator<Container>
    {
        return insert_iterator<Container>(c, i);
    }
} // namespace tempest

#endif // tempest_core_iterator_hpp