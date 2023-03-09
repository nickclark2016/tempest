#ifndef tempest_simd_align_hpp__
#define tempest_simd_align_hpp__

#include <type_traits>

namespace tempest::math::simd
{
    template <typename T> struct simd_align : std::integral_constant<size_t, 1>
    {
    };

    template <> struct simd_align<float> : std::integral_constant<size_t, 16>
    {
    };

    template <> struct simd_align<double> : std::integral_constant<size_t, 32>
    {
    };

    template <typename T, std::size_t D> struct storage_type
    {
        static constexpr std::size_t size = D;
        using type = T[D];
    };

    template <typename T> struct storage_type<T, 2>
    {
        static constexpr std::size_t size = 4;
        using type = T[4];
    };

    template <> 
    struct storage_type<double, 2>
    {
        static constexpr std::size_t size = 2;
        using type = double[2];
    };

    template <typename T> struct storage_type<T, 3>
    {
        static constexpr std::size_t size = 4;
        using type = T[4];
    };

    template <typename T, std::size_t N> using storage_type_t = typename storage_type<T, N>::type;

    template <typename T, std::size_t N> inline constexpr auto storage_type_size = storage_type<T, N>::size;
} // namespace tempest::math::simd

#endif // tempest_simdalign_hpp__