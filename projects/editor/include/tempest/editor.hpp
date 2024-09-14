#ifndef tempest_editor_editor_hpp
#define tempest_editor_editor_hpp

#include "asset_database_view.hpp"
#include "entity_inspector_view.hpp"
#include "hierarchy_view.hpp"

#include <tempest/tempest.hpp>

namespace tempest::editor
{
    class editor
    {
      public:
        editor(tempest::engine& eng);
        void update(tempest::engine& eng);

        ecs::entity selected_entity() const noexcept { return _hierarchy_view.selected_entity(); }

      private:
        asset_database_view _asset_database_view;
        hierarchy_view _hierarchy_view;
        entity_inspector_view _entity_inspector_view;
    };
} // namespace tempest::editor

#endif // tempest_editor_editor_hpp