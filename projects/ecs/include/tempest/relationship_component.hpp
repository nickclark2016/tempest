#ifndef tempest_ecs_relationship_component_hpp
#define tempest_ecs_relationship_component_hpp

#include <tempest/traits.hpp>

namespace tempest::ecs
{
    template <typename E>
    struct relationship_component
    {
        E parent;
        E next_sibling;
        E first_child;
    };

    template <typename E>
    struct is_duplicatable<relationship_component<E>> : false_type
    {
    };
} // namespace tempest::ecs

#endif // tempest_ecs_relationship_component_hpp