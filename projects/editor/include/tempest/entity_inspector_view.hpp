#ifndef tempest_editor_entity_inspector_view_hpp
#define tempest_editor_entity_inspector_view_hpp

#include <tempest/tempest.hpp>

namespace tempest::editor
{
    class entity_inspector_view
    {
      public:
        void update(tempest::engine& eng);
        
        void set_selected_entity(tempest::ecs::entity ent) noexcept;

      private:
        tempest::ecs::entity _selected_entity = tempest::ecs::null;
    };
} // namespace tempest::editor

#endif // tempest_editor_entity_inspector_view_hpp