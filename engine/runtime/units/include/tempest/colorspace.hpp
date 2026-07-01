#ifndef tempest_units_colorspace_hpp
#define tempest_units_colorspace_hpp

#include <tempest/concepts.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/unit.hpp>

#include <cmath>

namespace tempest::units
{
    template <typename T, uint32_t Channels, bool Normalized, uint32_t AlphaChannel = Channels>
    struct color_type
    {
        using value_type = T;
        static constexpr bool normalized = Normalized;
        static constexpr uint32_t channels = Channels;
        static constexpr bool has_alpha = AlphaChannel != Channels;
        static constexpr uint32_t alpha_channel = AlphaChannel;

        T values[channels]; // NOLINT
    };

    using color_rgb_32 = color_type<float, 3, true>;
    using color_rgba_32 = color_type<float, 4, true, 3>;

    namespace color
    {
        struct color_dimension
        {
        };

        struct iec_srgb
        {
        };

        struct linear
        {
        };

        struct iec_srgb_curves
        {
            template <typename T>
            static constexpr T to_canonical(T x)
            {
                auto color = x;

                if constexpr (!T::normalized)
                {
                    for (uint32_t idx = 0; idx < T::channels; ++idx)
                    {
                        color.values[idx] = color.values[idx] / numeric_limits<typename T::value_type>::max();
                    }
                }

                for (uint32_t idx = 0; idx < T::channels; ++idx)
                {
                    if (idx != T::alpha_channel)
                    {
                        color.values[idx] = channel_to_canonical(x.values[idx]);
                    }
                    else
                    {
                        color.values[idx] = x.values[idx];
                    }
                }
                return color;
            }

            template <typename T>
            static constexpr T from_canonical(T x)
            {
                auto color = x;
                for (uint32_t idx = 0; idx < T::channels; ++idx)
                {
                    if (idx != T::alpha_channel)
                    {
                        color.values[idx] = channel_from_canonical(x.values[idx]);
                    }
                    else
                    {
                        color.values[idx] = x.values[idx];
                    }
                }

                if constexpr (!T::normalized)
                {
                    for (uint32_t idx = 0; idx < T::channels; ++idx)
                    {
                        color.values[idx] = color.values[idx] * numeric_limits<typename T::value_type>::max();
                    }
                }

                return color;
            }

          private:
            template <typename T>
            static constexpr T channel_to_canonical(T x)
            {
                const auto val = static_cast<double>(x);
                const auto result = val <= 0.0405 ? (val / 12.92) : std::pow((val + 0.055) / 1.055, 2.4);
                return static_cast<T>(result);
            }

            template <typename T>
            static constexpr T channel_from_canonical(T x)
            {
                const auto value = static_cast<double>(x);
                const auto result = value <= 0.0031308 ? (value * 12.92) : 1.055 * (std::pow(value, 1.0 / 2.4) - 0.055);
                return static_cast<T>(result);
            }
        };
    } // namespace color

    template <>
    struct unit_traits<color::linear>
    {
        using dimension = color::color_dimension;
        using conversion = identity;
    };

    template <>
    struct unit_traits<color::iec_srgb>
    {
        using dimension = color::color_dimension;
        using conversion = color::iec_srgb_curves;
    };
} // namespace tempest::units

#endif // tempest_units_colorspace_hpp
