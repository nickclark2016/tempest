#ifndef tempest_core_expected_hpp
#define tempest_core_expected_hpp

#include <tempest/memory.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    template <typename T>
    struct unexpected
    {
        T value;
    };

    struct unexpect_t
    {
        explicit unexpect_t() = default;
    };

    inline constexpr unexpect_t unexpect{};

    template <typename T, typename E>
    class expected
    {
      public:
        constexpr expected()
            requires is_default_constructible_v<T>;

        constexpr expected(const expected& other)
            requires is_copy_constructible_v<T> && is_copy_constructible_v<E>;

        constexpr expected(expected&& other) noexcept(is_nothrow_move_constructible_v<T> &&
                                                      is_nothrow_move_constructible_v<E>)
            requires is_move_constructible_v<T> && is_move_constructible_v<E>;

        template <typename U, typename G>
            requires is_constructible_v<T, const U&> && is_constructible_v<E, const G&>
        constexpr expected(const expected<U, G>& other);

        template <typename U, typename G>
            requires is_constructible_v<T, U&&> && is_constructible_v<E, G>
        constexpr expected(expected<U, G>&& other);

        template <typename U = T>
            requires(!is_same_v<remove_cvref_t<U>, in_place_t> && !is_same_v<expected<T, E>, remove_cvref_t<U>> &&
                     is_constructible_v<T, U>)
        constexpr explicit(!is_convertible_v<U, T>) expected(U&& value);

        template <typename G>
            requires is_constructible_v<E, const G&>
        constexpr explicit(!is_convertible_v<const G&, E>) expected(const unexpected<G>& value);

        template <typename G>
            requires is_constructible_v<E, G>
        constexpr explicit(!is_convertible_v<G&&, E>) expected(unexpected<G>&& value);

        template <typename... Args>
            requires is_constructible_v<T, Args...>
        constexpr explicit expected(in_place_t, Args&&... args);

        template <typename... Args>
            requires is_constructible_v<T, Args...>
        constexpr explicit expected(unexpect_t, Args&&... args);

        constexpr ~expected();

        constexpr expected& operator=(const expected& other)
            requires is_copy_constructible_v<T> && is_copy_constructible_v<E> && is_copy_assignable_v<T> &&
                     is_copy_assignable_v<E> &&
                     (is_nothrow_move_constructible_v<T> || !is_nothrow_move_constructible_v<T>);

        constexpr expected& operator=(expected&& other) noexcept(is_nothrow_move_constructible_v<T> &&
                                                                 is_nothrow_move_constructible_v<E> &&
                                                                 is_nothrow_move_assignable_v<T> &&
                                                                 is_nothrow_move_assignable_v<E>)
            requires is_move_constructible_v<T> && is_move_constructible_v<E> && is_move_assignable_v<T> &&
                     is_move_assignable_v<E> &&
                     (is_nothrow_move_constructible_v<T> || !is_nothrow_move_constructible_v<T>);

        template <typename U = T>
            requires(!is_same_v<expected<T, E>, remove_cvref_t<U>> && is_constructible_v<T, U> &&
                     is_assignable_v<T&, U> &&
                     (is_nothrow_constructible_v<T, U> || is_nothrow_move_constructible_v<T> ||
                      is_nothrow_move_constructible_v<E>))
        constexpr expected& operator=(U&& value);

        template <typename G>
            requires(is_constructible_v<E, const G&> && is_assignable_v<E&, const G&> &&
                     (is_nothrow_constructible_v<E, const G&> || is_nothrow_move_constructible_v<T> ||
                      is_nothrow_move_constructible_v<E>))
        constexpr expected& operator=(const unexpected<G>& value);

        template <typename G>
            requires(is_constructible_v<E, G> && is_assignable_v<E&, G> &&
                     (is_nothrow_constructible_v<E, G> || is_nothrow_move_constructible_v<T> ||
                      is_nothrow_move_constructible_v<E>))
        constexpr expected& operator=(unexpected<G>&& value);

        constexpr const T* operator->() const noexcept;
        constexpr T* operator->() noexcept;
        constexpr const T& operator*() const& noexcept;
        constexpr T& operator*() & noexcept;
        constexpr const T&& operator*() const&& noexcept;
        constexpr T&& operator*() && noexcept;

        constexpr explicit operator bool() const noexcept;
        constexpr bool has_value() const noexcept;

        constexpr const T& value() const&;
        constexpr T& value() &;
        constexpr const T&& value() const&&;
        constexpr T&& value() &&;

        constexpr const E& error() const&;
        constexpr E& error() &;
        constexpr const E&& error() const&&;
        constexpr E&& error() &&;

        template <typename U>
            requires is_copy_constructible_v<T> && is_convertible_v<U, T>
        constexpr T value_or(U&& default_value) const&;

        template <typename U>
            requires is_move_constructible_v<T> && is_convertible_v<U, T>
        constexpr T value_or(U&& default_value) &&;

        template <typename G = E>
            requires is_copy_constructible_v<E> && is_convertible_v<G, E>
        constexpr E error_or(G&& default_error) const&;

        template <typename G = E>
            requires is_move_constructible_v<E> && is_convertible_v<G, E>
        constexpr E error_or(G&& default_error) &&;

      private:
        union {
            T _value;
            E _error;
        };

        bool _has_value;
    };

    template <typename T, typename E>
    inline constexpr expected<T, E>::expected()
        requires is_default_constructible_v<T>
        : _value{}, _has_value{true}
    {
    }

    template <typename T, typename E>
    inline constexpr expected<T, E>::expected(const expected& other)
        requires is_copy_constructible_v<T> && is_copy_constructible_v<E>
        : _has_value{other._has_value}
    {
        if (_has_value)
        {
            (void)tempest::construct_at(&_value, other._value);
        }
        else
        {
            (void)tempest::construct_at(&_error, other._error);
        }
    }

    template <typename T, typename E>
    inline constexpr expected<T, E>::expected(expected&& other) noexcept(is_nothrow_move_constructible_v<T> &&
                                                                         is_nothrow_move_constructible_v<E>)
        requires is_move_constructible_v<T> && is_move_constructible_v<E>
        : _has_value{other._has_value}
    {
        if (_has_value)
        {
            (void)tempest::construct_at(&_value, tempest::move(other._value));
        }
        else
        {
            (void)tempest::construct_at(&_error, tempest::move(other._error));
        }
    }

    template <typename T, typename E>
    template <typename U, typename G>
        requires is_constructible_v<T, const U&> && is_constructible_v<E, const G&>
    inline constexpr expected<T, E>::expected(const expected<U, G>& other) : _has_value{other._has_value}
    {
        if (_has_value)
        {
            (void)tempest::construct_at(&_value, other._value);
        }
        else
        {
            (void)tempest::construct_at(&_error, other._error);
        }
    }

    template <typename T, typename E>
    template <typename U, typename G>
        requires is_constructible_v<T, U&&> && is_constructible_v<E, G>
    inline constexpr expected<T, E>::expected(expected<U, G>&& other) : _has_value{other._has_value}
    {
        if (_has_value)
        {
            (void)tempest::construct_at(&_value, tempest::move(other._value));
        }
        else
        {
            (void)tempest::construct_at(&_error, tempest::move(other._error));
        }
    }

    template <typename T, typename E>
    template <typename U>
        requires(!is_same_v<remove_cvref_t<U>, in_place_t> && !is_same_v<expected<T, E>, remove_cvref_t<U>> &&
                 is_constructible_v<T, U>)
    inline constexpr expected<T, E>::expected(U&& value) : _value{tempest::forward<U>(value)}, _has_value{true}
    {
    }

    template <typename T, typename E>
    template <typename G>
        requires is_constructible_v<E, const G&>
    inline constexpr expected<T, E>::expected(const unexpected<G>& value) : _error{value.value}, _has_value{false}
    {
    }

    template <typename T, typename E>
    template <typename G>
        requires is_constructible_v<E, G>
    inline constexpr expected<T, E>::expected(unexpected<G>&& value)
        : _error{tempest::move(value.value)}, _has_value{false}
    {
    }

    template <typename T, typename E>
    template <typename... Args>
        requires is_constructible_v<T, Args...>
    inline constexpr expected<T, E>::expected(in_place_t, Args&&... args)
        : _value{tempest::forward<Args>(args)...}, _has_value{true}
    {
    }

    template <typename T, typename E>
    template <typename... Args>
        requires is_constructible_v<T, Args...>
    inline constexpr expected<T, E>::expected(unexpect_t, Args&&... args)
        : _error{tempest::forward<Args>(args)...}, _has_value{false}
    {
    }

    template <typename T, typename E>
    inline constexpr expected<T, E>::~expected()
    {
        if (_has_value)
        {
            tempest::destroy_at(&_value);
        }
        else
        {
            tempest::destroy_at(&_error);
        }
    }

    template <typename T, typename E>
    inline constexpr expected<T, E>& expected<T, E>::operator=(const expected& other)
        requires is_copy_constructible_v<T> && is_copy_constructible_v<E> && is_copy_assignable_v<T> &&
                 is_copy_assignable_v<E> && (is_nothrow_move_constructible_v<T> || !is_nothrow_move_constructible_v<T>)
    {
        if (this != tempest::addressof(other))
        {
            if (_has_value && other._has_value)
            {
                _value = other._value;
            }
            else if (!_has_value && !other._has_value)
            {
                _error = other._error;
            }
            else if (_has_value && !other._has_value)
            {
                tempest::destroy_at(&_value);
                (void)tempest::construct_at(&_error, other._error);
            }
            else
            {
                tempest::destroy_at(&_error);
                (void)tempest::construct_at(&_value, other._value);
            }

            _has_value = other._has_value;
        }

        return *this;
    }

    template <typename T, typename E>
    inline constexpr expected<T, E>& expected<T, E>::operator=(expected&& other) noexcept(
        is_nothrow_move_constructible_v<T> && is_nothrow_move_constructible_v<E> && is_nothrow_move_assignable_v<T> &&
        is_nothrow_move_assignable_v<E>)
        requires is_move_constructible_v<T> && is_move_constructible_v<E> && is_move_assignable_v<T> &&
                 is_move_assignable_v<E> && (is_nothrow_move_constructible_v<T> || !is_nothrow_move_constructible_v<T>)
    {
        if (this != tempest::addressof(other))
        {
            if (_has_value && other._has_value)
            {
                _value = tempest::move(other._value);
            }
            else if (!_has_value && !other._has_value)
            {
                _error = tempest::move(other._error);
            }
            else if (_has_value && !other._has_value)
            {
                tempest::destroy_at(&_value);
                (void)tempest::construct_at(&_error, tempest::move(other._error));
            }
            else
            {
                tempest::destroy_at(&_error);
                (void)tempest::construct_at(&_value, tempest::move(other._value));
            }

            _has_value = other._has_value;
        }

        return *this;
    }

    template <typename T, typename E>
    template <typename U>
        requires(!is_same_v<expected<T, E>, remove_cvref_t<U>> && is_constructible_v<T, U> && is_assignable_v<T&, U> &&
                 (is_nothrow_constructible_v<T, U> || is_nothrow_move_constructible_v<T> ||
                  is_nothrow_move_constructible_v<E>))
    inline constexpr expected<T, E>& expected<T, E>::operator=(U&& value)
    {
        if (_has_value)
        {
            _value = tempest::forward<U>(value);
        }
        else
        {
            tempest::destroy_at(&_error);
            (void)tempest::construct_at(&_value, tempest::forward<U>(value));
        }

        _has_value = true;

        return *this;
    }

    template <typename T, typename E>
    template <typename G>
        requires(is_constructible_v<E, const G&> && is_assignable_v<E&, const G&> &&
                 (is_nothrow_constructible_v<E, const G&> || is_nothrow_move_constructible_v<T> ||
                  is_nothrow_move_constructible_v<E>))
    inline constexpr expected<T, E>& expected<T, E>::operator=(const unexpected<G>& value)
    {
        if (_has_value)
        {
            tempest::destroy_at(&_value);
            (void)tempest::construct_at(&_error, value.value);
        }
        else
        {
            _error = value.value;
        }

        _has_value = false;

        return *this;
    }

    template <typename T, typename E>
    template <typename G>
        requires(is_constructible_v<E, G> && is_assignable_v<E&, G> &&
                 (is_nothrow_constructible_v<E, G> || is_nothrow_move_constructible_v<T> ||
                  is_nothrow_move_constructible_v<E>))
    inline constexpr expected<T, E>& expected<T, E>::operator=(unexpected<G>&& value)
    {
        if (_has_value)
        {
            tempest::destroy_at(&_value);
            (void)tempest::construct_at(&_error, tempest::move(value.value));
        }
        else
        {
            _error = tempest::move(value.value);
        }

        _has_value = false;

        return *this;
    }

    template <typename T, typename E>
    inline constexpr const T* expected<T, E>::operator->() const noexcept
    {
        return &_value;
    }

    template <typename T, typename E>
    inline constexpr T* expected<T, E>::operator->() noexcept
    {
        return &_value;
    }

    template <typename T, typename E>
    inline constexpr const T& expected<T, E>::operator*() const& noexcept
    {
        return _value;
    }

    template <typename T, typename E>
    inline constexpr T& expected<T, E>::operator*() & noexcept
    {
        return _value;
    }

    template <typename T, typename E>
    inline constexpr const T&& expected<T, E>::operator*() const&& noexcept
    {
        return tempest::move(_value);
    }

    template <typename T, typename E>
    inline constexpr T&& expected<T, E>::operator*() && noexcept
    {
        return tempest::move(_value);
    }

    template <typename T, typename E>
    inline constexpr expected<T, E>::operator bool() const noexcept
    {
        return has_value();
    }

    template <typename T, typename E>
    inline constexpr bool expected<T, E>::has_value() const noexcept
    {
        return _has_value;
    }

    template <typename T, typename E>
    inline constexpr const T& expected<T, E>::value() const&
    {
        return _value;
    }

    template <typename T, typename E>
    inline constexpr T& expected<T, E>::value() &
    {
        return _value;
    }

    template <typename T, typename E>
    inline constexpr const T&& expected<T, E>::value() const&&
    {
        return tempest::move(_value);
    }

    template <typename T, typename E>
    inline constexpr T&& expected<T, E>::value() &&
    {
        return tempest::move(_value);
    }

    template <typename T, typename E>
    inline constexpr const E& expected<T, E>::error() const&
    {
        return _error;
    }

    template <typename T, typename E>
    inline constexpr E& expected<T, E>::error() &
    {
        return _error;
    }

    template <typename T, typename E>
    inline constexpr const E&& expected<T, E>::error() const&&
    {
        return tempest::move(_error);
    }

    template <typename T, typename E>
    inline constexpr E&& expected<T, E>::error() &&
    {
        return tempest::move(_error);
    }

    template <typename T, typename E>
    template <typename U>
        requires is_copy_constructible_v<T> && is_convertible_v<U, T>
    inline constexpr T expected<T, E>::value_or(U&& default_value) const&
    {
        return has_value() ? _value : static_cast<T>(forward<U>(default_value));
    }

    template <typename T, typename E>
    template <typename U>
        requires is_move_constructible_v<T> && is_convertible_v<U, T>
    inline constexpr T expected<T, E>::value_or(U&& default_value) &&
    {
        return has_value() ? tempest::move(_value) : static_cast<T>(forward<U>(default_value));
    }

    template <typename T, typename E>
    template <typename G>
        requires is_copy_constructible_v<E> && is_convertible_v<G, E>
    inline constexpr E expected<T, E>::error_or(G&& default_error) const&
    {
        return has_value() ? static_cast<E>(forward<G>(default_error)) : _error;
    }

    template <typename T, typename E>
    template <typename G>
        requires is_move_constructible_v<E> && is_convertible_v<G, E>
    inline constexpr E expected<T, E>::error_or(G&& default_error) &&
    {
        return has_value() ? static_cast<E>(forward<G>(default_error)) : tempest::move(_error);
    }
} // namespace tempest

#endif // tempest_core_expected_hpp