#ifndef tempest_editor_entity_inspector_view_hpp
#define tempest_editor_entity_inspector_view_hpp

#include "component_view.hpp"

#include <tempest/tempest.hpp>

#include <memory>
#include <vector>

namespace tempest::editor
{
    class entity_inspector_view
    {
      public:
        void update(tempest::engine& eng);
        
        void set_selected_entity(tempest::ecs::entity ent) noexcept;
        
        template <typename T>
        void register_component_view_factory()
        {
            _component_view_factories.push_back(std::make_unique<T>());
        }

      private:
        tempest::ecs::entity _selected_entity = tempest::ecs::null;

        std::vector<std::unique_ptr<component_view_factory>> _component_view_factories;
    };
} // namespace tempest::editor

#endif // tempest_editor_entity_inspector_view_hpp