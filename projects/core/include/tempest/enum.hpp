#ifndef tempest_core_enum_hpp
#define tempest_core_enum_hpp

#include <tempest/concepts.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    template <enumeration EnumType>
    
    class enum_mask
    {
      public:
        constexpr enum_mask() noexcept = default;
        constexpr explicit enum_mask(EnumType value) noexcept;

        constexpr enum_mask operator|(const enum_mask& other) const noexcept;
        constexpr enum_mask operator&(const enum_mask& other) const noexcept;
        constexpr enum_mask operator^(const enum_mask& other) const noexcept;

        constexpr enum_mask operator|(EnumType value) const noexcept;
        constexpr enum_mask operator&(EnumType value) const noexcept;
        constexpr enum_mask operator^(EnumType value) const noexcept;

        constexpr enum_mask operator~() const noexcept;

        constexpr enum_mask& operator|=(const enum_mask& other) noexcept;
        constexpr enum_mask& operator&=(const enum_mask& other) noexcept;
        constexpr enum_mask& operator^=(const enum_mask& other) noexcept;

        constexpr enum_mask& operator|=(EnumType value) noexcept;
        constexpr enum_mask& operator&=(EnumType value) noexcept;
        constexpr enum_mask& operator^=(EnumType value) noexcept;

        constexpr bool operator==(const enum_mask& other) const noexcept;
        constexpr bool operator!=(const enum_mask& other) const noexcept;

        constexpr bool operator==(EnumType value) const noexcept;
        constexpr bool operator!=(EnumType value) const noexcept;

        constexpr EnumType value() const noexcept;
        constexpr operator EnumType() const noexcept;

      private:
        EnumType _value{};
    };

    template <enumeration EnumType>
    constexpr enum_mask<EnumType>::enum_mask(EnumType value) noexcept : _value(value)
    {
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType> enum_mask<EnumType>::operator|(const enum_mask& other) const noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(other.value());
        return enum_mask(static_cast<EnumType>(lhs_underlying | rhs_underlying));
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType> enum_mask<EnumType>::operator&(const enum_mask& other) const noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(other.value());
        return enum_mask(static_cast<EnumType>(lhs_underlying & rhs_underlying));
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType> enum_mask<EnumType>::operator^(const enum_mask& other) const noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(other.value());
        return enum_mask(static_cast<EnumType>(lhs_underlying ^ rhs_underlying));
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType> enum_mask<EnumType>::operator|(EnumType value) const noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(value);
        return enum_mask(static_cast<EnumType>(lhs_underlying | rhs_underlying));
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType> enum_mask<EnumType>::operator&(EnumType value) const noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(value);
        return enum_mask(static_cast<EnumType>(lhs_underlying & rhs_underlying));
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType> enum_mask<EnumType>::operator^(EnumType value) const noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(value);
        return enum_mask(static_cast<EnumType>(lhs_underlying ^ rhs_underlying));
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType> enum_mask<EnumType>::operator~() const noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        return enum_mask(static_cast<EnumType>(~lhs_underlying));
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType>& enum_mask<EnumType>::operator|=(const enum_mask& other) noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(other.value());
        _value = static_cast<EnumType>(lhs_underlying | rhs_underlying);
        return *this;
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType>& enum_mask<EnumType>::operator&=(const enum_mask& other) noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(other.value());
        _value = static_cast<EnumType>(lhs_underlying & rhs_underlying);
        return *this;
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType>& enum_mask<EnumType>::operator^=(const enum_mask& other) noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(other.value());
        _value = static_cast<EnumType>(lhs_underlying ^ rhs_underlying);
        return *this;
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType>& enum_mask<EnumType>::operator|=(EnumType value) noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(value);
        _value = static_cast<EnumType>(lhs_underlying | rhs_underlying);
        return *this;
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType>& enum_mask<EnumType>::operator&=(EnumType value) noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(value);
        _value = static_cast<EnumType>(lhs_underlying & rhs_underlying);
        return *this;
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType>& enum_mask<EnumType>::operator^=(EnumType value) noexcept
    {
        auto lhs_underlying = to_underlying(_value);
        auto rhs_underlying = to_underlying(value);
        _value = static_cast<EnumType>(lhs_underlying ^ rhs_underlying);
        return *this;
    }

    template <enumeration EnumType>
    constexpr bool enum_mask<EnumType>::operator==(const enum_mask& other) const noexcept
    {
        return _value == other.value();
    }

    template <enumeration EnumType>
    constexpr bool enum_mask<EnumType>::operator!=(const enum_mask& other) const noexcept
    {
        return !(*this == other);
    }

    template <enumeration EnumType>
    constexpr bool enum_mask<EnumType>::operator==(EnumType value) const noexcept
    {
        return _value == value;
    }

    template <enumeration EnumType>
    constexpr bool enum_mask<EnumType>::operator!=(EnumType value) const noexcept
    {
        return !(*this == value);
    }

    template <enumeration EnumType>
    constexpr EnumType enum_mask<EnumType>::value() const noexcept
    {
        return _value;
    }

    template <enumeration EnumType>
    constexpr enum_mask<EnumType>::operator EnumType() const noexcept
    {
        return _value;
    }
} // namespace tempest

#endif // tempest_core_enum_hpp
