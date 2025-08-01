#include "asset_explorer.hpp"

#include <tempest/ui.hpp>

namespace tempest::editor
{
    void asset_explorer::render()
    {
        if (ui::ui_context::begin_window({
                .name = "Asset Explorer",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
            ui::ui_context::text("Asset Explorer is under construction.");
        }
        ui::ui_context::end_window();
    }

    bool asset_explorer::should_render() const noexcept
    {
        return true; // Always render for now.
    }

    bool asset_explorer::should_close() const noexcept
    {
        return false; // Do not close by default.
    }

    string_view asset_explorer::name() const noexcept
    {
        return "Asset Explorer";
    }
} // namespace tempest::editor