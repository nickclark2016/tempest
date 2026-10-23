#include <tempest/windows/project_view_window.hpp>

#include <tempest/utility.hpp>

#include <imgui.h>

namespace tempest::editor
{
    project_view_window::project_view_window(assets::asset_database* asset_db)
        : _model{asset_db}
    {
    }

    project_view_window::project_view_window(project_tree_model model)
        : _model{tempest::move(model)}
    {
    }

    auto project_view_window::draw() -> void
    {
        if (!open)
        {
            return;
        }

        if (ImGui::Begin(window_name().data(), &open))
        {
            // Hierarchy tree rows, search bar, and item details to be rendered here in subsequent commits.
        }
        ImGui::End();
    }
} // namespace tempest::editor
