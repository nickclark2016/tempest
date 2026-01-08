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
        using value_type = T;
        using error_type = E;
        using unexpected_type = unexpected<E>;

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

        template <typename F>
        constexpr auto and_then(F&& func) &;

        template <typename F>
        constexpr auto and_then(F&& func) const&;

        template <typename F>
        constexpr auto and_then(F&& func) &&;

        template <typename F>
        constexpr auto and_then(F&& func) const&&;

        template <typename F>
        constexpr auto transform(F&& func) &;

        template <typename F>
        constexpr auto transform(F&& func) const&;

        template <typename F>
        constexpr auto transform(F&& func) &&;

        template <typename F>
        constexpr auto transform(F&& func) const&&;

        template <typename F>
        constexpr auto or_else(F&& func) &;

        template <typename F>
        constexpr auto or_else(F&& func) const&;

        template <typename F>
        constexpr auto or_else(F&& func) &&;

        template <typename F>
        constexpr auto or_else(F&& func) const&&;

        template <typename F>
        constexpr auto transform_error(F&& func) &;

        template <typename F>
        constexpr auto transform_error(F&& func) const&;

        template <typename F>
        constexpr auto transform_error(F&& func) &&;

        template <typename F>
        constexpr auto transform_error(F&& func) const&&;

        template <typename... Args>
        constexpr T& emplace(Args&&... args);

        constexpr void swap(expected& other) noexcept(is_nothrow_move_constructible_v<T> && is_nothrow_swappable_v<T> &&
                                                      is_nothrow_move_constructible_v<E> && is_nothrow_swappable_v<E>);

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

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::and_then(F&& func) &
    {
        if (has_value())
        {
            return invoke(tempest::forward<F>(func), _value);
        }
        else
        {
            return expected<E, T>(unexpect, _error);
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::and_then(F&& func) const&
    {
        if (has_value())
        {
            return invoke(tempest::forward<F>(func), _value);
        }
        else
        {
            return expected<E, T>(unexpect, _error);
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::and_then(F&& func) &&
    {
        if (has_value())
        {
            return invoke(tempest::forward<F>(func), tempest::move(_value));
        }
        else
        {
            return expected<E, T>(unexpect, tempest::move(_error));
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::and_then(F&& func) const&&
    {
        if (has_value())
        {
            return invoke(tempest::forward<F>(func), tempest::move(_value));
        }
        else
        {
            return expected<E, T>(unexpect, tempest::move(_error));
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::transform(F&& func) &
    {
        using U = invoke_result_t<F, T&>;
        using result_t = expected<U, E>;

        if (has_value())
        {
            return result_t(invoke(tempest::forward<F>(func), _value));
        }
        else
        {
            return result_t(unexpect, _error);
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::transform(F&& func) const&
    {
        using U = invoke_result_t<F, const T&>;
        using result_t = expected<U, E>;

        if (has_value())
        {
            return result_t(invoke(tempest::forward<F>(func), _value));
        }
        else
        {
            return result_t(unexpect, _error);
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::transform(F&& func) &&
    {
        using U = invoke_result_t<F, T&&>;
        using result_t = expected<U, E>;

        if (has_value())
        {
            return result_t(invoke(tempest::forward<F>(func), tempest::move(_value)));
        }
        else
        {
            return result_t(unexpect, tempest::move(_error));
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::transform(F&& func) const&&
    {
        using U = invoke_result_t<F, const T&&>;
        using result_t = expected<U, E>;

        if (has_value())
        {
            return result_t(invoke(tempest::forward<F>(func), tempest::move(_value)));
        }
        else
        {
            return result_t(unexpect, tempest::move(_error));
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::or_else(F&& func) &
    {
        using result_t = invoke_result_t<F, E&>;

        if (!has_value())
        {
            return invoke(tempest::forward<F>(func), _error);
        }
        else
        {
            return result_t(_value);
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::or_else(F&& func) const&
    {
        using result_t = invoke_result_t<F, const E&>;

        if (!has_value())
        {
            return invoke(tempest::forward<F>(func), _error);
        }
        else
        {
            return result_t(_value);
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::or_else(F&& func) &&
    {
        using result_t = invoke_result_t<F, E&&>;

        if (!has_value())
        {
            return invoke(tempest::forward<F>(func), tempest::move(_error));
        }
        else
        {
            return result_t(tempest::move(_value));
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::or_else(F&& func) const&&
    {
        using result_t = invoke_result_t<F, const E&&>;

        if (!has_value())
        {
            return invoke(tempest::forward<F>(func), tempest::move(_error));
        }
        else
        {
            return result_t(tempest::move(_value));
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::transform_error(F&& func) &
    {
        using U = invoke_result_t<F, E&>;
        using result_t = expected<T, U>;

        if (has_value())
        {
            return result_t(_value);
        }
        else
        {
            return result_t(unexpect, invoke(tempest::forward<F>(func), _error));
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::transform_error(F&& func) const&
    {
        using U = invoke_result_t<F, const E&>;
        using result_t = expected<T, U>;

        if (has_value())
        {
            return result_t(_value);
        }
        else
        {
            return result_t(unexpect, invoke(tempest::forward<F>(func), _error));
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::transform_error(F&& func) &&
    {
        using U = invoke_result_t<F, E&&>;
        using result_t = expected<T, U>;

        if (has_value())
        {
            return result_t(_value);
        }
        else
        {
            return result_t(unexpect, invoke(tempest::forward<F>(func), tempest::move(_error)));
        }
    }

    template <typename T, typename E>
    template <typename F>
    inline constexpr auto expected<T, E>::transform_error(F&& func) const&&
    {
        using U = invoke_result_t<F, const E&&>;
        using result_t = expected<T, U>;

        if (has_value())
        {
            return result_t(_value);
        }
        else
        {
            return result_t(unexpect, invoke(tempest::forward<F>(func), tempest::move(_error)));
        }
    }

    template <typename T, typename E>
    template <typename... Args>
    inline constexpr T& expected<T, E>::emplace(Args&&... args)
    {
        if (_has_value)
        {
            tempest::destroy_at(&_value);
        }

        (void)tempest::construct_at(&_value, tempest::forward<Args>(args)...);
        _has_value = true;
        return _value;
    }

    template <typename T, typename E>
    inline constexpr void expected<T, E>::swap(expected& other) noexcept(is_nothrow_move_constructible_v<T> &&
                                                                         is_nothrow_swappable_v<T> &&
                                                                         is_nothrow_move_constructible_v<E> &&
                                                                         is_nothrow_swappable_v<E>)
    {
        // 4 cases, both have value, both have error, this has value other has error, this has error other has value
        if (_has_value && other._has_value)
        {
            tempest::swap(_value, other._value);
        }
        else if (!_has_value && other._has_value)
        {
            tempest::swap(_error, other._error);
        }
        else if (_has_value && !other._has_value)
        {
            auto temp_error = tempest::move(other._error);
            tempest::destroy_at(&other._error);
            (void)tempest::construct_at(&other._value, tempest::move(_value));
            tempest::destroy_at(&_value);
            (void)tempest::construct_at(&_error, tempest::move(temp_error));
            tempest::swap(_has_value, other._has_value);
        }
        else
        {
            auto temp_value = tempest::move(other._value);
            tempest::destroy_at(&other._value);
            (void)tempest::construct_at(&other._error, tempest::move(_error));
            tempest::destroy_at(&_error);
            (void)tempest::construct_at(&_value, tempest::move(temp_value));
            tempest::swap(_has_value, other._has_value);
        }
    }

    template <typename E>
    class expected<void, E>
    {
      public:
        using value_type = void;
        using error_type = E;
        using unexpected_type = unexpected<E>;

        constexpr expected();
        constexpr expected(const expected& other)
            requires is_copy_constructible_v<E>;
        constexpr expected(expected&& other) noexcept(is_nothrow_move_constructible_v<E>)
            requires is_move_constructible_v<E>;

        template <typename U, typename G>
            requires is_constructible_v<E, const G&>
        constexpr explicit(!is_convertible_v<const G&, E>) expected(const expected<U, G>& other);

        template <typename U, typename G>
            requires is_constructible_v<E, G>
        constexpr explicit(!is_convertible_v<G&&, E>) expected(expected<U, G>&& other);

        template <typename G>
            requires is_constructible_v<E, const G&>
        constexpr explicit(!is_convertible_v<const G&, E>) expected(const unexpected<G>& value);

        template <typename G>
            requires is_constructible_v<E, G>
        constexpr explicit(!is_convertible_v<G&&, E>) expected(unexpected<G>&& value);

        constexpr explicit expected(in_place_t);

        template <typename... Args>
            requires is_constructible_v<E, Args...>
        constexpr explicit expected(unexpect_t, Args&&... args);

        constexpr ~expected();

        constexpr expected& operator=(const expected& other);
        constexpr expected& operator=(expected&& other) noexcept(is_nothrow_move_constructible_v<E> &&
                                                                 is_nothrow_move_assignable_v<E>);

        template <typename G>
            requires(is_constructible_v<E, const G&> && is_assignable_v<E&, const G&>)
        constexpr expected& operator=(const unexpected<G>& value);

        template <typename G>
            requires(is_constructible_v<E, G> && is_assignable_v<E&, G>)
        constexpr expected& operator=(unexpected<G>&& value);

        constexpr void operator*() const noexcept;
        constexpr bool has_value() const noexcept;
        constexpr explicit operator bool() const noexcept;

        constexpr const E& error() const&;
        constexpr E& error() &;
        constexpr const E&& error() const&&;
        constexpr E&& error() &&;

        template <typename G = remove_cv_t<E>>
        constexpr E error_or(G&& default_error) const&;

        template <typename G = remove_cv_t<E>>
        constexpr E error_or(G&& default_error) &&;

        template <typename F>
        constexpr auto and_then(F&& func) &;

        template <typename F>
        constexpr auto and_then(F&& func) const&;

        template <typename F>
        constexpr auto and_then(F&& func) &&;

        template <typename F>
        constexpr auto and_then(F&& func) const&&;

        template <typename F>
        constexpr auto transform(F&& func) &;

        template <typename F>
        constexpr auto transform(F&& func) const&;

        template <typename F>
        constexpr auto transform(F&& func) &&;

        template <typename F>
        constexpr auto transform(F&& func) const&&;

        template <typename F>
        constexpr auto or_else(F&& func) &;

        template <typename F>
        constexpr auto or_else(F&& func) const&;

        template <typename F>
        constexpr auto or_else(F&& func) &&;

        template <typename F>
        constexpr auto or_else(F&& func) const&&;

        template <typename F>
        constexpr auto transform_error(F&& func) &;

        template <typename F>
        constexpr auto transform_error(F&& func) const&;

        template <typename F>
        constexpr auto transform_error(F&& func) &&;

        template <typename F>
        constexpr auto transform_error(F&& func) const&&;

        constexpr void emplace() noexcept;

        constexpr void swap(expected& other) noexcept(is_nothrow_move_constructible_v<E> && is_nothrow_swappable_v<E>);

      private:
        union {
            char _dummy;
            E _error;
        };

        bool _has_value;
    };

    template <typename E>
    inline constexpr expected<void, E>::expected() : _dummy{}, _has_value{true}
    {
    }

    template <typename E>
    inline constexpr expected<void, E>::expected(const expected& other)
        requires is_copy_constructible_v<E>
        : _has_value{other._has_value}
    {
        if (!other._has_value)
        {
            (void)tempest::construct_at(&_error, other._error);
        }
    }

    template <typename E>
    inline constexpr expected<void, E>::expected(expected&& other) noexcept(is_nothrow_move_constructible_v<E>)
        requires is_move_constructible_v<E>
        : _has_value{other._has_value}
    {
        if (!other._has_value)
        {
            (void)tempest::construct_at(&_error, tempest::move(other._error));
        }
    }

    template <typename E>
    template <typename U, typename G>
        requires is_constructible_v<E, const G&>
    inline constexpr expected<void, E>::expected(const expected<U, G>& other) : _has_value{other._has_value}
    {
        if (!other._has_value)
        {
            (void)tempest::construct_at(&_error, other._error);
        }
    }

    template <typename E>
    template <typename U, typename G>
        requires is_constructible_v<E, G>
    inline constexpr expected<void, E>::expected(expected<U, G>&& other) : _has_value{other._has_value}
    {
        if (!other._has_value)
        {
            (void)tempest::construct_at(&_error, tempest::move(other._error));
        }
    }

    template <typename E>
    template <typename G>
        requires is_constructible_v<E, const G&>
    inline constexpr expected<void, E>::expected(const unexpected<G>& value) : _error{value.value}, _has_value{false}
    {
    }

    template <typename E>
    template <typename G>
        requires is_constructible_v<E, G>
    inline constexpr expected<void, E>::expected(unexpected<G>&& value)
        : _error{tempest::move(value.value)}, _has_value{false}
    {
    }

    template <typename E>
    inline constexpr expected<void, E>::expected(in_place_t) : _dummy{}, _has_value{true}
    {
    }

    template <typename E>
    template <typename... Args>
        requires is_constructible_v<E, Args...>
    inline constexpr expected<void, E>::expected(unexpect_t, Args&&... args)
        : _error{tempest::forward<Args>(args)...}, _has_value{false}
    {
    }

    template <typename E>
    inline constexpr expected<void, E>::~expected()
    {
        if (!_has_value)
        {
            tempest::destroy_at(&_error);
        }
    }

    template <typename E>
    inline constexpr expected<void, E>& expected<void, E>::operator=(const expected& other)
    {
        if (&other == this)
        {
            return *this;
        }

        if (!_has_value)
        {
            tempest::destroy_at(&_error);
        }

        if (!other._has_value)
        {
            (void)tempest::construct_at(&_error, other._error);
        }

        _has_value = other._has_value;

        return *this;
    }

    template <typename E>
    inline constexpr expected<void, E>& expected<void, E>::operator=(expected&& other) noexcept(
        is_nothrow_move_constructible_v<E> && is_nothrow_move_assignable_v<E>)
    {
        if (&other == this)
        {
            return *this;
        }

        if (!_has_value)
        {
            tempest::destroy_at(&_error);
        }

        if (!other._has_value)
        {
            (void)tempest::construct_at(&_error, tempest::move(other._error));
        }

        _has_value = other._has_value;

        return *this;
    }

    template <typename E>
    template <typename G>
        requires(is_constructible_v<E, const G&> && is_assignable_v<E&, const G&>)
    inline constexpr expected<void, E>& expected<void, E>::operator=(const unexpected<G>& value)
    {
        if (!_has_value)
        {
            _error = value.value;
        }
        else
        {
            (void)tempest::construct_at(&_error, value.value);
        }

        _has_value = false;

        return *this;
    }

    template <typename E>
    template <typename G>
        requires(is_constructible_v<E, G> && is_assignable_v<E&, G>)
    inline constexpr expected<void, E>& expected<void, E>::operator=(unexpected<G>&& value)
    {
        if (!_has_value)
        {
            _error = tempest::move(value.value);
        }
        else
        {
            (void)tempest::construct_at(&_error, tempest::move(value.value));
        }

        _has_value = false;

        return *this;
    }

    template <typename E>
    inline constexpr void expected<void, E>::operator*() const noexcept
    {
    }

    template <typename E>
    inline constexpr bool expected<void, E>::has_value() const noexcept
    {
        return _has_value;
    }

    template <typename E>
    inline constexpr expected<void, E>::operator bool() const noexcept
    {
        return has_value();
    }

    template <typename E>
    inline constexpr const E& expected<void, E>::error() const&
    {
        return _error;
    }

    template <typename E>
    inline constexpr E& expected<void, E>::error() &
    {
        return _error;
    }

    template <typename E>
    inline constexpr const E&& expected<void, E>::error() const&&
    {
        return tempest::move(_error);
    }

    template <typename E>
    inline constexpr E&& expected<void, E>::error() &&
    {
        return tempest::move(_error);
    }

    template <typename E>
    template <typename G>
    inline constexpr E expected<void, E>::error_or(G&& default_error) const&
    {
        return has_value() ? static_cast<E>(forward<G>(default_error)) : _error;
    }

    template <typename E>
    template <typename G>
    inline constexpr E expected<void, E>::error_or(G&& default_error) &&
    {
        return has_value() ? static_cast<E>(forward<G>(default_error)) : tempest::move(_error);
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::and_then(F&& func) &
    {
        using result_t = invoke_result_t<F>;

        if (has_value())
        {
            return invoke(tempest::forward<F>(func));
        }
        else
        {
            return expected<typename result_t::value_type, E>(unexpect, _error);
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::and_then(F&& func) const&
    {
        using result_t = invoke_result_t<F>;

        if (has_value())
        {
            return invoke(tempest::forward<F>(func));
        }
        else
        {
            return expected<typename result_t::value_type, E>(unexpect, _error);
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::and_then(F&& func) &&
    {
        using result_t = invoke_result_t<F>;

        if (has_value())
        {
            return invoke(tempest::forward<F>(func));
        }
        else
        {
            return expected<typename result_t::value_type, E>(unexpect, tempest::move(_error));
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::and_then(F&& func) const&&
    {
        using result_t = invoke_result_t<F>;

        if (has_value())
        {
            return invoke(tempest::forward<F>(func));
        }
        else
        {
            return expected<typename result_t::value_type, E>(unexpect, tempest::move(_error));
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::transform(F&& func) &
    {
        using U = invoke_result_t<F>;
        using result_t = expected<U, E>;

        if (has_value())
        {
            return result_t(invoke(tempest::forward<F>(func)));
        }
        else
        {
            return result_t(unexpect, _error);
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::transform(F&& func) const&
    {
        using U = invoke_result_t<F>;
        using result_t = expected<U, E>;

        if (has_value())
        {
            return result_t(invoke(tempest::forward<F>(func)));
        }
        else
        {
            return result_t(unexpect, _error);
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::transform(F&& func) &&
    {
        using U = invoke_result_t<F>;
        using result_t = expected<U, E>;

        if (has_value())
        {
            return result_t(invoke(tempest::forward<F>(func)));
        }
        else
        {
            return result_t(unexpect, tempest::move(_error));
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::transform(F&& func) const&&
    {
        using U = invoke_result_t<F>;
        using result_t = expected<U, E>;

        if (has_value())
        {
            return result_t(invoke(tempest::forward<F>(func)));
        }
        else
        {
            return result_t(unexpect, tempest::move(_error));
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::or_else(F&& func) &
    {
        using result_t = invoke_result_t<F, E>;

        if (!has_value())
        {
            return invoke(tempest::forward<F>(func), _error);
        }
        else
        {
            return result_t();
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::or_else(F&& func) const&
    {
        using result_t = invoke_result_t<F, E>;

        if (!has_value())
        {
            return invoke(tempest::forward<F>(func), _error);
        }
        else
        {
            return result_t();
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::or_else(F&& func) &&
    {
        using result_t = invoke_result_t<F, E>;

        if (!has_value())
        {
            return invoke(tempest::forward<F>(func), tempest::move(_error));
        }
        else
        {
            return result_t();
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::or_else(F&& func) const&&
    {
        using result_t = invoke_result_t<F, E>;

        if (!has_value())
        {
            return invoke(tempest::forward<F>(func), tempest::move(_error));
        }
        else
        {
            return result_t();
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::transform_error(F&& func) &
    {
        using U = invoke_result_t<F, E&>;
        using result_t = expected<void, U>;

        if (has_value())
        {
            return result_t();
        }
        else
        {
            return result_t(unexpect, invoke(tempest::forward<F>(func), _error));
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::transform_error(F&& func) const&
    {
        using U = invoke_result_t<F, const E&>;
        using result_t = expected<void, U>;

        if (has_value())
        {
            return result_t();
        }
        else
        {
            return result_t(unexpect, invoke(tempest::forward<F>(func), _error));
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::transform_error(F&& func) &&
    {
        using U = invoke_result_t<F, E&&>;
        using result_t = expected<void, U>;

        if (has_value())
        {
            return result_t();
        }
        else
        {
            return result_t(unexpect, invoke(tempest::forward<F>(func), tempest::move(_error)));
        }
    }

    template <typename E>
    template <typename F>
    inline constexpr auto expected<void, E>::transform_error(F&& func) const&&
    {
        using U = invoke_result_t<F, const E&&>;
        using result_t = expected<void, U>;

        if (has_value())
        {
            return result_t();
        }
        else
        {
            return result_t(unexpect, invoke(tempest::forward<F>(func), tempest::move(_error)));
        }
    }

    template <typename E>
    inline constexpr void expected<void, E>::emplace() noexcept
    {
        if (!_has_value)
        {
            destroy_at(&_error);
        }

        _has_value = true;
    }

    template <typename E>
    inline constexpr void expected<void, E>::swap(expected& other) noexcept(is_nothrow_move_constructible_v<E> &&
                                                                            is_nothrow_swappable_v<E>)
    {
        // 4 cases, both have value, both have error, this has value other has error, this has error other has value
        if (_has_value && other._has_value)
        {
            tempest::swap(_has_value, other._has_value);
        }
        else if (!_has_value && !other._has_value)
        {
            // No-op
        }
        else if (_has_value && !other._has_value)
        {
            auto temp_error = tempest::move(other._error);
            tempest::destroy_at(&other._error);
            other._has_value = true;
            tempest::destroy_at(&_error);
            (void)tempest::construct_at(&_error, tempest::move(temp_error));
            _has_value = false;
        }
        else
        {
            auto temp_error = tempest::move(_error);
            tempest::destroy_at(&_error);
            _has_value = true;
            tempest::destroy_at(&other._error);
            (void)tempest::construct_at(&other._error, tempest::move(temp_error));
            other._has_value = false;
        }
    }

    template <typename T, typename E>
    inline constexpr void swap(expected<T, E>& a, expected<T, E>& b) noexcept(noexcept(a.swap(b)))
    {
        a.swap(b);
    }

    template <typename T, typename E>
    inline constexpr bool operator==(const expected<T, E>& lhs, const expected<T, E>& rhs)
    {
        if (lhs.has_value() != rhs.has_value())
        {
            return false;
        }

        if (lhs.has_value())
        {
            if constexpr (is_void_v<T>)
            {
                return true;
            }
            else
            {
                return *lhs == *rhs;
            }
        }
        else
        {
            return lhs.error() == rhs.error();
        }
    }

    namespace detail
    {
        template <typename Exp, typename Callable>
        struct expected_visitor;

        template <typename T, typename E, typename Callable>
        struct expected_visitor<expected<T, E>&, Callable>
        {
            auto operator()(expected<T, E>& exp, Callable&& func) const
            {
                using result_type = invoke_result_t<Callable, T&>;
                if constexpr (is_void_v<result_type>)
                {
                    if (exp.has_value())
                    {
                        invoke(tempest::forward<Callable>(func), exp.value());
                    }
                    else
                    {
                        invoke(tempest::forward<Callable>(func), exp.error());
                    }
                }
                else
                {
                    if (exp.has_value())
                    {
                        return invoke(tempest::forward<Callable>(func), exp.value());
                    }
                    else
                    {
                        return invoke(tempest::forward<Callable>(func), exp.error());
                    }
                }
            }
        };

        template <typename E, typename Callable>
        struct expected_visitor<expected<void, E>&, Callable>
        {
            auto operator()(expected<void, E>& exp, Callable&& func) const
            {
                using result_type = invoke_result_t<Callable>;
                if constexpr (is_void_v<result_type>)
                {
                    if (exp.has_value())
                    {
                        invoke(tempest::forward<Callable>(func));
                    }
                    else
                    {
                        invoke(tempest::forward<Callable>(func), exp.error());
                    }
                }
                else
                {
                    if (exp.has_value())
                    {
                        return invoke(tempest::forward<Callable>(func));
                    }
                    else
                    {
                        return invoke(tempest::forward<Callable>(func), exp.error());
                    }
                }
            }
        };

        template <typename T, typename E, typename Callable>
        struct expected_visitor<const expected<T, E>&, Callable>
        {
            auto operator()(const expected<T, E>& exp, Callable&& func) const
            {
                using result_type = invoke_result_t<Callable, const T&>;
                if constexpr (is_void_v<result_type>)
                {
                    if (exp.has_value())
                    {
                        invoke(tempest::forward<Callable>(func), exp.value());
                    }
                    else
                    {
                        invoke(tempest::forward<Callable>(func), exp.error());
                    }
                }
                else
                {
                    if (exp.has_value())
                    {
                        return invoke(tempest::forward<Callable>(func), exp.value());
                    }
                    else
                    {
                        return invoke(tempest::forward<Callable>(func), exp.error());
                    }
                }
            }
        };

        template <typename E, typename Callable>
        struct expected_visitor<const expected<void, E>&, Callable>
        {
            auto operator()(const expected<void, E>& exp, Callable&& func) const
            {
                using result_type = invoke_result_t<Callable>;
                if constexpr (is_void_v<result_type>)
                {
                    if (exp.has_value())
                    {
                        invoke(tempest::forward<Callable>(func));
                    }
                    else
                    {
                        invoke(tempest::forward<Callable>(func), exp.error());
                    }
                }
                else
                {
                    if (exp.has_value())
                    {
                        return invoke(tempest::forward<Callable>(func));
                    }
                    else
                    {
                        return invoke(tempest::forward<Callable>(func), exp.error());
                    }
                }
            }
        };

        template <typename T, typename E, typename Callable>
        struct expected_visitor<expected<T, E>&&, Callable>
        {
            auto operator()(expected<T, E>&& exp, Callable&& func) const
            {
                using result_type = invoke_result_t<Callable, T&&>;
                if constexpr (is_void_v<result_type>)
                {
                    if (exp.has_value())
                    {
                        invoke(tempest::forward<Callable>(func), tempest::move(exp).value());
                    }
                    else
                    {
                        invoke(tempest::forward<Callable>(func), tempest::move(exp).error());
                    }
                }
                else
                {
                    if (exp.has_value())
                    {
                        return invoke(tempest::forward<Callable>(func), tempest::move(exp).value());
                    }
                    else
                    {
                        return invoke(tempest::forward<Callable>(func), tempest::move(exp).error());
                    }
                }
            }
        };

        template <typename E, typename Callable>
        struct expected_visitor<expected<void, E>&&, Callable>
        {
            auto operator()(expected<void, E>&& exp, Callable&& func) const
            {
                using result_type = invoke_result_t<Callable>;
                if constexpr (is_void_v<result_type>)
                {
                    if (exp.has_value())
                    {
                        invoke(tempest::forward<Callable>(func));
                    }
                    else
                    {
                        invoke(tempest::forward<Callable>(func), tempest::move(exp).error());
                    }
                }
                else
                {
                    if (exp.has_value())
                    {
                        return invoke(tempest::forward<Callable>(func));
                    }
                    else
                    {
                        return invoke(tempest::forward<Callable>(func), tempest::move(exp).error());
                    }
                }
            }
        };

        template <typename T, typename E, typename Callable>
        struct expected_visitor<const expected<T, E>&&, Callable>
        {
            auto operator()(const expected<T, E>&& exp, Callable&& func) const
            {
                using result_type = invoke_result_t<Callable, const T&&>;
                if constexpr (is_void_v<result_type>)
                {
                    if (exp.has_value())
                    {
                        invoke(tempest::forward<Callable>(func), tempest::move(exp).value());
                    }
                    else
                    {
                        invoke(tempest::forward<Callable>(func), tempest::move(exp).error());
                    }
                }
                else
                {
                    if (exp.has_value())
                    {
                        return invoke(tempest::forward<Callable>(func), tempest::move(exp).value());
                    }
                    else
                    {
                        return invoke(tempest::forward<Callable>(func), tempest::move(exp).error());
                    }
                }
            }
        };

        template <typename E, typename Callable>
        struct expected_visitor<const expected<void, E>&&, Callable>
        {
            auto operator()(const expected<void, E>&& exp, Callable&& func) const
            {
                using result_type = invoke_result_t<Callable>;
                if constexpr (is_void_v<result_type>)
                {
                    if (exp.has_value())
                    {
                        invoke(tempest::forward<Callable>(func));
                    }
                    else
                    {
                        invoke(tempest::forward<Callable>(func), tempest::move(exp).error());
                    }
                }
                else
                {
                    if (exp.has_value())
                    {
                        return invoke(tempest::forward<Callable>(func));
                    }
                    else
                    {
                        return invoke(tempest::forward<Callable>(func), tempest::move(exp).error());
                    }
                }
            }
        };
    } // namespace detail

    template <typename T, typename E, typename Callable>
    inline constexpr auto visit(expected<T, E>& exp, Callable&& func)
    {
        return detail::expected_visitor<expected<T, E>&, Callable>()(exp, tempest::forward<Callable>(func));
    }

    template <typename T, typename E, typename Callable>
    inline constexpr auto visit(const expected<T, E>& exp, Callable&& func)
    {
        return detail::expected_visitor<const expected<T, E>&, Callable>()(exp, tempest::forward<Callable>(func));
    }
} // namespace tempest

#endif // tempest_core_expected_hpp