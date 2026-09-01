#ifndef tempest_core_checked_hpp
#define tempest_core_checked_hpp

#include <tempest/assert.hpp>
#include <tempest/optional.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    template <typename T>
    class non_null
    {
      public:
        // Construct from non-null reference
        constexpr non_null(T& ref) noexcept : _ptr(&ref)
        {
        }

        // Construct from pointer with assertion
        template <typename U>
            requires(is_convertible_v<U*, T*>)
        constexpr non_null(U* ptr) noexcept : _ptr(ptr)
        {
            TEMPEST_ASSERT(ptr != nullptr);
        }

        // Converting constructor from non_null of convertible type
        template <typename U>
            requires(is_convertible_v<U*, T*>)
        constexpr non_null(const non_null<U>& other) noexcept : _ptr(other.get())
        {
        }

        // Disallow construction and assignment from nullptr
        non_null(decltype(nullptr)) = delete;
        auto operator=(decltype(nullptr)) -> non_null& = delete;

        // Default copy, move, and destructor
        constexpr non_null(const non_null&) noexcept = default;
        constexpr non_null(non_null&&) noexcept = default;
        constexpr auto operator=(const non_null&) noexcept -> non_null& = default;
        constexpr auto operator=(non_null&&) noexcept -> non_null& = default;
        ~non_null() = default;

        // Factory methods
        static constexpr auto create(T* ptr) noexcept -> optional<non_null<T>>
        {
            return ptr ? optional<non_null<T>>{non_null<T>{*ptr}} : nullopt;
        }

        static constexpr auto create_unchecked(T* ptr) noexcept -> non_null<T>
        {
            return non_null<T>{*ptr};
        }

        [[nodiscard]] constexpr auto get() const noexcept -> T*
        {
            return _ptr;
        }

        constexpr operator T&() const noexcept
        {
            return *_ptr;
        }

        constexpr auto operator*() & noexcept -> T&
        {
            return *_ptr;
        }

        constexpr auto operator*() const& noexcept -> const T&
        {
            return *_ptr;
        }

        constexpr auto operator*() && noexcept -> T&&
        {
            return tempest::move(*_ptr);
        }

        constexpr auto operator*() const&& noexcept -> const T&&
        {
            return tempest::move(*_ptr);
        }

        constexpr auto operator->() noexcept -> T*
        {
            return _ptr;
        }

        constexpr auto operator->() const noexcept -> const T*
        {
            return _ptr;
        }

        template <typename U>
        constexpr auto operator==(const non_null<U>& other) const noexcept -> bool
        {
            return _ptr == other.get();
        }

        template <typename U>
        constexpr auto operator!=(const non_null<U>& other) const noexcept -> bool
        {
            return _ptr != other.get();
        }

        constexpr auto operator==(const T* other) const noexcept -> bool
        {
            return _ptr == other;
        }

        constexpr auto operator!=(const T* other) const noexcept -> bool
        {
            return _ptr != other;
        }

      private:
        T* _ptr;
    };
} // namespace tempest

#endif // tempest_core_checked_hpp