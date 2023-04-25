#ifndef tempest_ecs_entity_hpp
#define tempest_ecs_entity_hpp

#include <cstdint>

namespace tempest::ecs
{
    struct entity
    {
        std::uint32_t id;
        std::uint32_t generation;
    };
}

#endif // tempest_ecs_entity_hpp