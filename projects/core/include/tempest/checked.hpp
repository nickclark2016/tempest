#ifndef tempest_core_checked_hpp
#define tempest_core_checked_hpp

#include <tempest/optional.hpp>

namespace tempest
{
    template <typename T>
    class non_null
    {
        constexpr non_null() = default;

      public:
        static constexpr optional<non_null<T>> create(T* ptr) noexcept;
        static constexpr non_null<T> create_unchecked(T* ptr) noexcept;

        constexpr T& operator*() & noexcept;
        constexpr const T& operator*() const& noexcept;
        constexpr T&& operator*() && noexcept;
        constexpr const T&& operator*() const&& noexcept;

        constexpr T* operator->() noexcept;
        constexpr const T* operator->() const noexcept;

      private:
        T* _ptr;
    };

    template <typename T>
    constexpr optional<non_null<T>> non_null<T>::create(T* ptr)
    {
        return ptr ? optional<non_null<T>>{non_null<T>{ptr}} : nullopt;
    }

    template <typename T>
    constexpr non_null<T> non_null<T>::create_unchecked(T* ptr)
    {
        return non_null<T>{ptr};
    }

    template <typename T>
    constexpr T& non_null<T>::operator*() & noexcept
    {
        return *_ptr;
    }

    template <typename T>
    constexpr const T& non_null<T>::operator*() const& noexcept
    {
        return *_ptr;
    }

    template <typename T>
    constexpr T&& non_null<T>::operator*() && noexcept
    {
        return tempest::move(*_ptr);
    }

    template <typename T>
    constexpr const T&& non_null<T>::operator*() const&& noexcept
    {
        return tempest::move(*_ptr);
    }

    template <typename T>
    constexpr T* non_null<T>::operator->() noexcept
    {
        return _ptr;
    }

    template <typename T>
    constexpr const T* non_null<T>::operator->() const noexcept
    {
        return _ptr;
    }
} // namespace tempest

#endif // tempest_core_checked_hpp