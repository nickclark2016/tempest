#ifndef tempest_editor_editor_hpp
#define tempest_editor_editor_hpp

#include "hierarchy_view.hpp"

#include <tempest/tempest.hpp>

namespace tempest::editor
{
    class editor
    {
      public:
        void update(tempest::engine& eng);

        ecs::entity selected_entity() const noexcept { return _hierarchy_view.selected_entity(); }

      private:
        hierarchy_view _hierarchy_view;
    };
} // namespace tempest::editor

#endif // tempest_editor_editor_hpp