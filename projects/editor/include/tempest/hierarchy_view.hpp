#ifndef tempest_editor_hierarchy_view_hpp
#define tempest_editor_hierarchy_view_hpp

#include <tempest/relationship_component.hpp>
#include <tempest/tempest.hpp>

namespace tempest::editor
{
    class hierarchy_view
    {
      public:
        void update(engine& eng);

        ecs::entity selected_entity() const noexcept { return _selected_entity; }

      private:
        void _create_entities_view_dfs(ecs::archetype_registry& registry, ecs::entity parent);
        void _create_entities_view(ecs::archetype_registry& registry);

        ecs::entity _selected_entity = ecs::null;
    };
} // namespace tempest::editor

#endif // tempest_editor_hierarchy_view_hpp