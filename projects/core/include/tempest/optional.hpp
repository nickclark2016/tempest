#ifndef tempest_core_optional_hpp
#define tempest_core_optional_hpp

#include <tempest/memory.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    struct nullopt_t
    {
        explicit constexpr nullopt_t() noexcept = default;
    };

    inline constexpr nullopt_t nullopt{};

    template <typename T>
    class optional
    {
      public:
        using value_type = T;
        using iterator = T*;
        using const_iterator = const T*;

        constexpr optional() noexcept = default;
        constexpr optional(nullopt_t) noexcept;
        constexpr optional(const optional& other);
        constexpr optional(optional&& other) noexcept(is_nothrow_move_constructible_v<T> &&
                                                      is_nothrow_destructible_v<T>);

        template <typename U>
            requires(is_convertible_v<U, T>)
        constexpr optional(const optional<U>& other);

        template <typename U>
            requires(is_convertible_v<U, T>)
        constexpr optional(optional<U>&& other);

        template <typename U = T>
            requires(is_constructible_v<T, U>)
        constexpr optional(U&& value);

        ~optional()
            requires is_trivially_destructible_v<T>
        = default;

        ~optional() noexcept(is_nothrow_destructible_v<T>);

        optional& operator=(nullopt_t) noexcept(is_nothrow_destructible_v<T>);

        constexpr optional& operator=(const optional& other);
        constexpr optional& operator=(optional&& other) noexcept(is_nothrow_move_assignable_v<T> &&
                                                                 is_nothrow_move_constructible_v<T> &&
                                                                 is_nothrow_destructible_v<T>);

        template <typename U>
            requires(is_convertible_v<U, T>)
        constexpr optional& operator=(const optional<U>& other);

        template <typename U>
            requires(is_convertible_v<U, T>)
        constexpr optional& operator=(optional<U>&& other);

        template <typename U = T>
            requires(is_constructible_v<T, U>)
        constexpr optional& operator=(U&& value);

        constexpr iterator begin() noexcept;
        constexpr const_iterator begin() const noexcept;
        constexpr const_iterator cbegin() const noexcept;

        constexpr iterator end() noexcept;
        constexpr const_iterator end() const noexcept;
        constexpr const_iterator cend() const noexcept;

        constexpr T& value() &;
        constexpr const T& value() const&;

        constexpr T&& value() &&;
        constexpr const T&& value() const&&;

        template <typename U>
        constexpr T value_or(U&& default_value) const&;

        template <typename U>
        constexpr T value_or(U&& default_value) &&;

        constexpr T* operator->() noexcept;
        constexpr const T* operator->() const noexcept;

        constexpr T& operator*() & noexcept;
        constexpr const T& operator*() const& noexcept;

        constexpr T&& operator*() && noexcept;
        constexpr const T&& operator*() const&& noexcept;

        constexpr explicit operator bool() const noexcept;
        constexpr bool has_value() const noexcept;

        constexpr void reset() noexcept(is_nothrow_destructible_v<T>);
        constexpr void swap(optional& other) noexcept(is_nothrow_swappable_v<T>);

        template <typename... Ts>
        constexpr T& emplace(Ts&&... args) noexcept(is_nothrow_constructible_v<T, Ts...> &&
                                                    is_nothrow_destructible_v<T>);

        // template <typename Fn>
        // constexpr auto and_then(Fn&& fn) &;

        // template <typename Fn>
        // constexpr auto and_then(Fn&& fn) const&;

        // template <typename Fn>
        // constexpr auto and_then(Fn&& fn) &&;

        // template <typename Fn>
        // constexpr auto and_then(Fn&& fn) const&&;

        // template <typename Fn>
        // constexpr auto transform(Fn&& fn) &;

        // template <typename Fn>
        // constexpr auto transform(Fn&& fn) const&;

        // template <typename Fn>
        // constexpr auto transform(Fn&& fn) &&;

        // template <typename Fn>
        // constexpr auto transform(Fn&& fn) const&&;

        // template <typename Fn>
        // constexpr optional or_else(Fn&& f) const&;

        // template <typename Fn>
        // constexpr optional or_else(Fn&& f) &&;

      private:
        union impl {
            T value;

            impl() = default;
            ~impl() = default;
        } _data;

        bool _has_value = false;
    };

    inline constexpr nullopt_t none()
    {
        return nullopt;
    }

    template <typename T>
    inline constexpr optional<decay_t<T>> some(T&& value) noexcept(is_nothrow_move_constructible_v<T>)
    {
        return optional<decay_t<T>>(move(value));
    }

    template <typename T, typename... Ts>
    inline constexpr optional<decay_t<T>> some(Ts&&... ts) noexcept(is_nothrow_constructible_v<T, Ts...>)
    {
        return optional<decay_t<T>>(forward<Ts>(ts)...);
    }

    template <typename T>
    inline constexpr optional<decay_t<T>> make_optional(T&& value) noexcept(is_nothrow_move_constructible_v<T>)
    {
        return optional<decay_t<T>>(move(value));
    }

    template <typename T, typename... Ts>
    inline constexpr optional<decay_t<T>> make_optional(Ts&&... ts) noexcept(is_nothrow_constructible_v<T, Ts...>)
    {
        return optional<decay_t<T>>(forward<Ts>(ts)...);
    }

    template <typename T>
    inline constexpr optional<T>::optional(nullopt_t) noexcept
    {
    }

    template <typename T>
    inline constexpr optional<T>::optional(const optional& other)
    {
        if (other.has_value())
        {
            (void)tempest::construct_at(&_data.value, other._data.value);
            _has_value = true;
        }
    }

    template <typename T>
    inline constexpr optional<T>::optional(optional&& other) noexcept(is_nothrow_move_constructible_v<T> &&
                                                                      is_nothrow_destructible_v<T>)
    {
        if (other.has_value())
        {
            (void)tempest::construct_at(&_data.value, tempest::move(other._data.value));
            other.reset();
            _has_value = true;
        }
    }

    template <typename T>
    template <typename U>
        requires(is_convertible_v<U, T>)
    inline constexpr optional<T>::optional(const optional<U>& other)
    {
        if (other.has_value())
        {
            (void)tempest::construct_at(&_data.value, other._data.value);
            _has_value = true;
        }
    }

    template <typename T>
    template <typename U>
        requires(is_convertible_v<U, T>)
    inline constexpr optional<T>::optional(optional<U>&& other)
    {
        if (other.has_value())
        {
            (void)tempest::construct_at(&_data.value, tempest::move(other._data.value));
            other.reset();
            _has_value = true;
        }
    }

    template <typename T>
    template <typename U>
        requires(is_constructible_v<T, U>)
    inline constexpr optional<T>::optional(U&& value)
    {
        (void)tempest::construct_at(&_data.value, tempest::forward<U>(value));
        _has_value = true;
    }

    template <typename T>
    inline optional<T>::~optional() noexcept(is_nothrow_destructible_v<T>)
    {
        if (has_value())
        {
            tempest::destroy_at(&_data.value);
            _has_value = false;
        }
    }

    template <typename T>
    inline optional<T>& optional<T>::operator=(nullopt_t) noexcept(is_nothrow_destructible_v<T>)
    {
        if (has_value())
        {
            tempest::destroy_at(&_data.value);
            _has_value = false;
        }

        return *this;
    }

    template <typename T>
    inline constexpr optional<T>& optional<T>::operator=(const optional& other)
    {
        if (this != &other)
        {
            if (has_value())
            {
                tempest::destroy_at(&_data.value);
            }

            if (other.has_value())
            {
                (void)tempest::construct_at(&_data.value, other._data.value);
                _has_value = true;
            }
            else
            {
                _has_value = false;
            }
        }

        return *this;
    }

    template <typename T>
    inline constexpr optional<T>& optional<T>::operator=(optional&& other) noexcept(
        is_nothrow_move_assignable_v<T> && is_nothrow_move_constructible_v<T> && is_nothrow_destructible_v<T>)
    {
        if (this != &other)
        {
            if (has_value())
            {
                tempest::destroy_at(&_data.value);
            }

            if (other.has_value())
            {
                (void)tempest::construct_at(&_data.value, tempest::move(other._data.value));
                other.reset();
                _has_value = true;
            }
            else
            {
                _has_value = false;
            }
        }

        return *this;
    }

    template <typename T>
    template <typename U>
        requires(is_convertible_v<U, T>)
    inline constexpr optional<T>& optional<T>::operator=(const optional<U>& other)
    {
        if (has_value())
        {
            tempest::destroy_at(&_data.value);
        }

        if (other.has_value())
        {
            (void)tempest::construct_at(&_data.value, other._data.value);
            _has_value = true;
        }
        else
        {
            _has_value = false;
        }

        return *this;
    }

    template <typename T>
    template <typename U>
        requires(is_convertible_v<U, T>)
    inline constexpr optional<T>& optional<T>::operator=(optional<U>&& other)
    {
        if (has_value())
        {
            tempest::destroy_at(&_data.value);
        }

        if (other.has_value())
        {
            (void)tempest::construct_at(&_data.value, tempest::move(other._data.value));
            other.reset();
            _has_value = true;
        }
        else
        {
            _has_value = false;
        }

        return *this;
    }

    template <typename T>
    template <typename U>
        requires(is_constructible_v<T, U>)
    inline constexpr optional<T>& optional<T>::operator=(U&& value)
    {
        if (has_value())
        {
            tempest::destroy_at(&_data.value);
        }

        (void)tempest::construct_at(&_data.value, tempest::forward<U>(value));
        _has_value = true;

        return *this;
    }

    template <typename T>
    inline constexpr typename optional<T>::iterator optional<T>::begin() noexcept
    {
        return has_value() ? &_data.value : nullptr;
    }

    template <typename T>
    inline constexpr typename optional<T>::const_iterator optional<T>::begin() const noexcept
    {
        return has_value() ? &_data.value : nullptr;
    }

    template <typename T>
    inline constexpr typename optional<T>::const_iterator optional<T>::cbegin() const noexcept
    {
        return begin();
    }

    template <typename T>
    inline constexpr typename optional<T>::iterator optional<T>::end() noexcept
    {
        return has_value() ? begin() + 1 : nullptr;
    }

    template <typename T>
    inline constexpr typename optional<T>::const_iterator optional<T>::end() const noexcept
    {
        return has_value() ? begin() + 1 : nullptr;
    }

    template <typename T>
    inline constexpr typename optional<T>::const_iterator optional<T>::cend() const noexcept
    {
        return end();
    }

    template <typename T>
    inline constexpr T& optional<T>::value() &
    {
        return _data.value;
    }

    template <typename T>
    inline constexpr const T& optional<T>::value() const&
    {
        return _data.value;
    }

    template <typename T>
    inline constexpr T&& optional<T>::value() &&
    {
        return tempest::move(_data.value);
    }

    template <typename T>
    inline constexpr const T&& optional<T>::value() const&&
    {
        return tempest::move(_data.value);
    }

    template <typename T>
    template <typename U>
    inline constexpr T optional<T>::value_or(U&& default_value) const&
    {
        return has_value() ? _data.value : static_cast<T>(forward<U>(default_value));
    }

    template <typename T>
    template <typename U>
    inline constexpr T optional<T>::value_or(U&& default_value) &&
    {
        return has_value() ? tempest::move(_data.value) : static_cast<T>(forward<U>(default_value));
    }

    template <typename T>
    inline constexpr T* optional<T>::operator->() noexcept
    {
        return &_data.value;
    }

    template <typename T>
    inline constexpr const T* optional<T>::operator->() const noexcept
    {
        return &_data.value;
    }

    template <typename T>
    inline constexpr T& optional<T>::operator*() & noexcept
    {
        return _data.value;
    }

    template <typename T>
    inline constexpr const T& optional<T>::operator*() const& noexcept
    {
        return _data.value;
    }

    template <typename T>
    inline constexpr T&& optional<T>::operator*() && noexcept
    {
        return tempest::move(_data.value);
    }

    template <typename T>
    inline constexpr const T&& optional<T>::operator*() const&& noexcept
    {
        return tempest::move(_data.value);
    }

    template <typename T>
    inline constexpr optional<T>::operator bool() const noexcept
    {
        return has_value();
    }

    template <typename T>
    inline constexpr bool optional<T>::has_value() const noexcept
    {
        return _has_value;
    }

    template <typename T>
    inline constexpr void optional<T>::reset() noexcept(is_nothrow_destructible_v<T>)
    {
        if (has_value())
        {
            tempest::destroy_at(&_data.value);
            _has_value = false;
        }
    }

    template <typename T>
    inline constexpr void optional<T>::swap(optional& other) noexcept(is_nothrow_swappable_v<T>)
    {
        if (has_value() && other.has_value())
        {
            tempest::swap(_data.value, other._data.value);
        }
        else if (has_value())
        {
            (void)tempest::construct_at(&other._data.value, tempest::move(_data.value));
            tempest::destroy_at(&_data.value);
            _has_value = false;
            other._has_value = true;
        }
        else if (other.has_value())
        {
            (void)tempest::construct_at(&_data.value, tempest::move(other._data.value));
            tempest::destroy_at(&other._data.value);
            _has_value = true;
            other._has_value = false;
        }
    }

    template <typename T>
    template <typename... Ts>
    inline constexpr T& optional<T>::emplace(Ts&&... args) noexcept(is_nothrow_constructible_v<T, Ts...> &&
                                                                    is_nothrow_destructible_v<T>)
    {
        if (has_value())
        {
            tempest::destroy_at(&_data.value);
        }

        (void)tempest::construct_at(&_data.value, tempest::forward<Ts>(args)...);
        _has_value = true;

        return _data.value;
    }

    template <typename T>
    inline constexpr void swap(optional<T>& lhs, optional<T>& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    template <typename T>
    inline constexpr bool operator==(const optional<T>& lhs, const optional<T>& rhs)
    {
        return lhs.has_value() == rhs.has_value() && (!lhs.has_value() || *lhs == *rhs);
    }

    template <typename T>
    inline constexpr bool operator!=(const optional<T>& lhs, const optional<T>& rhs)
    {
        return !(lhs == rhs);
    }
} // namespace tempest

#endif // tempest_core_optional_hpp