#ifndef tempest_simd_align_hpp
#define tempest_simd_align_hpp

#include <type_traits>

namespace tempest::math::simd
{
    template <typename T> struct simd_align : std::integral_constant<size_t, 1>
    {
    };

    template <> struct simd_align<std::int32_t> : std::integral_constant<size_t, 16>
    {
    };

    template <> struct simd_align<std::uint32_t> : std::integral_constant<size_t, 16>
    {
    };

    template <> struct simd_align<float> : std::integral_constant<size_t, 16>
    {
    };

    template <> struct simd_align<double> : std::integral_constant<size_t, 32>
    {
    };

    template <typename T, std::size_t D, std::size_t R> struct storage_type
    {
        static constexpr std::size_t size = D * R;
        using type = T[D * R];
    };

    template <typename T> struct storage_type<T, 2, 1>
    {
        static constexpr std::size_t size = 4;
        using type = T[4];
    };

    template <> struct storage_type<double, 2, 1>
    {
        static constexpr std::size_t size = 2;
        using type = double[2];
    };

    template <typename T> struct storage_type<T, 3, 1>
    {
        static constexpr std::size_t size = 4;
        using type = T[4];
    };

    template <typename T, std::size_t N> using storage_type_t = typename storage_type<T, N, 1>::type;

    template <typename T, std::size_t N> inline constexpr auto storage_type_size = storage_type<T, N, 1>::size;

    template <typename T, std::size_t N, std::size_t R> using mat_storage_type_t = typename storage_type<T, N, R>::type;

    template <typename T, std::size_t N, std::size_t R> inline constexpr auto mat_storage_type_size = storage_type<T, N, R>::size;
} // namespace tempest::math::simd

#endif // tempest_simdalign_hpp