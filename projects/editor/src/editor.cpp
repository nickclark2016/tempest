#include <tempest/editor.hpp>

namespace tempest::editor
{
    void editor::update(tempest::engine& eng)
    {
        _hierarchy_view.update(eng);
    }
} // namespace tempest::editor
