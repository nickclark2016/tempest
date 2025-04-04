#ifndef tempest_editor_component_view_hpp
#define tempest_editor_component_view_hpp

#include <tempest/archetype.hpp>

namespace tempest::editor
{
    class component_view_factory
    {
      public:
        virtual ~component_view_factory() = default;

        virtual bool create_view(ecs::archetype_registry& registry, ecs::entity ent) const = 0;
    };
} // namespace tempest::editor

#endif // tempest_editor_component_view_hpp