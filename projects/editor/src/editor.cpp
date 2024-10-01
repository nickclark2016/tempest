#include <tempest/editor.hpp>

#include <tempest/camera_component_view.hpp>
#include <tempest/material_component_view.hpp>
#include <tempest/mesh_component_view.hpp>
#include <tempest/transform_component_view.hpp>

#include <tempest/memory.hpp>

namespace tempest::editor
{
    editor::editor(tempest::engine& eng)
    {
        _entity_inspector_view.register_component_view_factory<camera_component_view>();
        _entity_inspector_view.register_component_view_factory<transform_component_view>();
        _entity_inspector_view.register_component_view_factory<mesh_component_view>(eng.get_mesh_registry());
        _entity_inspector_view.register_component_view_factory<material_component_view>(eng.get_material_registry(),
                                                                                        eng.get_texture_registry());
    }

    void editor::update(tempest::engine& eng)
    {
        _hierarchy_view.update(eng);

        _entity_inspector_view.set_selected_entity(_hierarchy_view.selected_entity());
        _entity_inspector_view.update(eng);
    }
} // namespace tempest::editor
