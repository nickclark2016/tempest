#ifndef tempest_ecs_keys_hpp
#define tempest_ecs_keys_hpp

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace tempest::ecs
{
    // clang-format off
    template <typename T>
    concept sparse_key = requires(T t, T t2)
    {
        { t.id } -> std::same_as<std::uint32_t&>;
    } && std::is_trivial<T>::value && std::equality_comparable<T>;
    // clang-format on
} // namespace tempest::ecs

#endif // tempest_ecs_keys_hpp