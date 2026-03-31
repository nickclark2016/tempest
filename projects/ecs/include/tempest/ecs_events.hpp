#ifndef tempest_ecs_ecs_events_hpp
#define tempest_ecs_ecs_events_hpp

namespace tempest::ecs
{
    template <typename E>
    struct entity_created_event
    {
        E entity;
    };

    template <typename E>
    struct entity_destroyed_event
    {
        E entity;
    };

    template <typename E, typename C>
    struct component_added_event
    {
        E entity;
        C component;
    };

    template <typename E, typename C>
    struct component_removed_event
    {
        E entity;
        C component;
    };

    template <typename E, typename C>
    struct component_replaced_event
    {
        E entity;
        C old_component;
        C new_component;
    };
}

#endif // tempest_ecs_ecs_events_hpp