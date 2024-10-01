#ifndef tempest_editor_entity_inspector_view_hpp
#define tempest_editor_entity_inspector_view_hpp

#include <tempest/component_view.hpp>
#include <tempest/memory.hpp>
#include <tempest/tempest.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor
{
    class entity_inspector_view
    {
      public:
        void update(tempest::engine& eng);

        void set_selected_entity(tempest::ecs::entity ent) noexcept;

        template <typename T, typename... Ts>
        void register_component_view_factory(Ts&&... args)
        {
            _component_view_factories.push_back(tempest::make_unique<T>(tempest::forward<Ts>(args)...));
        }

      private:
        ecs::entity _selected_entity = ecs::null;

        vector<unique_ptr<component_view_factory>> _component_view_factories;
    };
} // namespace tempest::editor

#endif // tempest_editor_entity_inspector_view_hpp