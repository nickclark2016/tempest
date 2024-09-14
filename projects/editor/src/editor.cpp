#include <tempest/editor.hpp>

#include <tempest/camera_component_view.hpp>
#include <tempest/transform_component_view.hpp>

#include <tempest/gltf_importer.hpp>
#include <tempest/memory.hpp>

namespace tempest::editor
{
    editor::editor(tempest::engine& eng)
    {
        _entity_inspector_view.register_component_view_factory<camera_component_view>();
        _entity_inspector_view.register_component_view_factory<transform_component_view>();

        eng.get_asset_database().register_importer("gltf", tempest::make_unique<tempest::assets::gltf_importer>());
    }

    void editor::update(tempest::engine& eng)
    {
        _hierarchy_view.update(eng);

        _entity_inspector_view.set_selected_entity(_hierarchy_view.selected_entity());
        _entity_inspector_view.update(eng);

        _asset_database_view.update(eng.get_asset_database());
    }
} // namespace tempest::editor
