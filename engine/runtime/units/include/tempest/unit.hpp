#ifndef tempest_units_unit_hpp
#define tempest_units_unit_hpp

namespace tempest::units
{
    template <typename Unit, typename Rep = double>
    class quantity
    {
      public:
        using unit_type = Unit;
        using rep_type = Rep;

        constexpr quantity() noexcept = default;
        constexpr explicit quantity(Rep value) noexcept : _value(value)
        {
        }

        constexpr auto value() const noexcept -> Rep
        {
            return _value;
        }

      private:
        Rep _value;
    };

    template <typename T>
    struct unit_traits;

    template <typename C, typename Rep>
    concept conversion = requires(Rep x) {
        { C::to_canonical(x) } -> same_as<Rep>;
        { C::from_canonical(x) } -> same_as<Rep>;
    };

    struct identity
    {
        template <typename U>
        static constexpr U to_canonical(U value)
        {
            return value;
        }

        template <typename U>
        static constexpr U from_canonical(U value)
        {
            return value;
        }
    };

    template <typename T, T Scale, T Offset>
    struct affine
    {
        static constexpr T scale = Scale;
        static constexpr T offset = Offset;

        template <typename U>
        static constexpr U to_canonical(U value)
        {
            return (value * scale) + offset;
        }

        template <typename U>
        static constexpr U from_canonical(U value)
        {
            return (value - offset) / scale;
        }
    };

    template <typename To, typename From, typename Rep>
    [[nodiscard]] constexpr auto quantity_cast(quantity<From, Rep> qua)
    {
        const auto canonical = unit_traits<From>::conversion::to_canonical(qua.value());
        return quantity<To, Rep>(unit_traits<To>::conversion::from_canonical(canonical));
    }
} // namespace tempest::units

#endif // tempest_units_unit_hpp
