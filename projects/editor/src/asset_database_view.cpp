#include <tempest/asset_database_view.hpp>

#include <tempest/imgui_context.hpp>

namespace tempest::editor
{
    void asset_database_view::update(tempest::assets::asset_database& db)
    {
        using imgui = tempest::graphics::imgui_context;

        imgui::create_window("Asset Database", [&]() {
            for (const auto& [path, prefab] : db.prefabs())
            {
                imgui::label(prefab->name.c_str());
            }
        });
    }
}