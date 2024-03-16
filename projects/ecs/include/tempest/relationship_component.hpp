#ifndef tempest_ecs_relationship_component_hpp
#define tempest_ecs_relationship_component_hpp

#include "traits.hpp"

namespace tempest::ecs
{
    template <typename E>
    struct relationship_component
    {
        E parent;
        E next_sibling;
        E first_child;
    };
}

#endif // tempest_ecs_relationship_component_hpp