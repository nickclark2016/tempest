#ifndef tempest_ecs_ecs_events_hpp
#define tempest_ecs_ecs_events_hpp

#include <tempest/int.hpp>
#include <tempest/string_view.hpp>

namespace tempest::ecs
{
    template <typename E>
    struct entity_component_added_event
    {
        E entity;
        uint32_t component_type_index;
    };

    template <typename E>
    struct entity_component_removed_event
    {
        E entity;
        uint32_t component_type_index;
    };

    template <typename E>
    struct entity_renamed_event
    {
        E entity;
        string_view old_name;
        string_view new_name;
    };

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

    struct world_restored_event
    {
    };

    struct simulation_started_event
    {
    };

    struct simulation_paused_event
    {
    };

    struct simulation_stopped_event
    {
    };
} // namespace tempest::ecs

#endif // tempest_ecs_ecs_events_hpp
