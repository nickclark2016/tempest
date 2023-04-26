#ifndef tempest_ecs_entity_hpp
#define tempest_ecs_entity_hpp

#include <compare>
#include <cstdint>

namespace tempest::ecs
{
    struct entity
    {
        std::uint32_t id;
        std::uint32_t generation;

        constexpr auto operator<=>(const entity&) const noexcept = default;
    };
}

#endif // tempest_ecs_entity_hpp