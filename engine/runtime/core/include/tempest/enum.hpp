#ifndef tempest_core_enum_hpp
#define tempest_core_enum_hpp

#include <tempest/api.hpp>
#include <tempest/concepts.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    template <enumeration EnumType>

    class TEMPEST_API enum_mask
    {
      public:
        constexpr enum_mask() noexcept = default;
        constexpr explicit enum_mask(EnumType value) noexcept : _value(value)
        {
        }

        constexpr enum_mask operator|(const enum_mask& other) const noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(other.value());
            return enum_mask(static_cast<EnumType>(lhs_underlying | rhs_underlying));
        }

        constexpr enum_mask operator&(const enum_mask& other) const noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(other.value());
            return enum_mask(static_cast<EnumType>(lhs_underlying & rhs_underlying));
        }

        constexpr enum_mask operator^(const enum_mask& other) const noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(other.value());
            return enum_mask(static_cast<EnumType>(lhs_underlying ^ rhs_underlying));
        }

        constexpr enum_mask operator|(EnumType value) const noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(value);
            return enum_mask(static_cast<EnumType>(lhs_underlying | rhs_underlying));
        }

        constexpr enum_mask operator&(EnumType value) const noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(value);
            return enum_mask(static_cast<EnumType>(lhs_underlying & rhs_underlying));
        }

        constexpr enum_mask operator^(EnumType value) const noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(value);
            return enum_mask(static_cast<EnumType>(lhs_underlying ^ rhs_underlying));
        }

        constexpr enum_mask operator~() const noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            return enum_mask(static_cast<EnumType>(~lhs_underlying));
        }

        constexpr enum_mask& operator|=(const enum_mask& other) noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(other.value());
            _value = static_cast<EnumType>(lhs_underlying | rhs_underlying);
            return *this;
        }

        constexpr enum_mask& operator&=(const enum_mask& other) noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(other.value());
            _value = static_cast<EnumType>(lhs_underlying & rhs_underlying);
            return *this;
        }

        constexpr enum_mask& operator^=(const enum_mask& other) noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(other.value());
            _value = static_cast<EnumType>(lhs_underlying ^ rhs_underlying);
            return *this;
        }

        constexpr enum_mask& operator|=(EnumType value) noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(value);
            _value = static_cast<EnumType>(lhs_underlying | rhs_underlying);
            return *this;
        }

        constexpr enum_mask& operator&=(EnumType value) noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(value);
            _value = static_cast<EnumType>(lhs_underlying & rhs_underlying);
            return *this;
        }

        constexpr enum_mask& operator^=(EnumType value) noexcept
        {
            auto lhs_underlying = to_underlying(_value);
            auto rhs_underlying = to_underlying(value);
            _value = static_cast<EnumType>(lhs_underlying ^ rhs_underlying);
            return *this;
        }

        constexpr bool operator==(const enum_mask& other) const noexcept
        {
            return _value == other.value();
        }

        constexpr bool operator!=(const enum_mask& other) const noexcept
        {
            return !(*this == other);
        }

        constexpr bool operator==(EnumType value) const noexcept
        {
            return _value == value;
        }

        constexpr bool operator!=(EnumType value) const noexcept
        {
            return !(*this == value);
        }

        constexpr EnumType value() const noexcept
        {
            return _value;
        }

        constexpr operator EnumType() const noexcept
        {
            return _value;
        }

        constexpr operator bool() const noexcept
        {
            return to_underlying(_value) != 0;
        }

      private:
        EnumType _value{};
    };

    template <typename EnumType, typename... EnumTypes>
        requires(same_as<EnumType, EnumTypes> && ...)
    constexpr enum_mask<EnumType> make_enum_mask(EnumType value, EnumTypes... values) noexcept
    {
        auto v = to_underlying(value);
        ((v |= to_underlying(values)), ...);
        return enum_mask<EnumType>(static_cast<EnumType>(v));
    }
} // namespace tempest

#endif // tempest_core_enum_hpp
