#include "asset_explorer.hpp"

#include <tempest/ui.hpp>

namespace tempest::editor
{
    namespace
    {
        bool double_click_button(string_view content)
        {
            ui::ui_context::button(content.data());
            return ui::ui_context::is_hovered() && ui::ui_context::is_double_clicked(core::mouse_button::left);
        }
    } // namespace

    asset_explorer::asset_explorer() : _root_path{filesystem::current_path() / "assets"}, _current_path{_root_path}
    {
    }

    void asset_explorer::render()
    {
        if (ui::ui_context::begin_window({
                .name = "Asset Explorer",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
            const auto current_path = _current_path;
            const auto current_path_rel = filesystem::relative(current_path, _root_path);
            // Iterate over the current path, displaying the components of the path individually
            // Between each element, display a ">" character to indicate the hierarchy
            // If the next element is end, don't display a ">"

            if (ui::ui_context::button("/"))
            {
                _current_path = _root_path;
            }

            if (current_path_rel != filesystem::path("."))
            {
                ui::ui_context::no_line_break();

                auto current_path_builder = _root_path;

                for (auto it = current_path_rel.begin(); it != current_path_rel.end();)
                {
                    current_path_builder /= *it;
                    auto subpath = filesystem::path(*it).string();
                    subpath.append("/");

                    ++it;

                    if (ui::ui_context::button(subpath.c_str()))
                    {
                        _current_path = current_path_builder;
                    }

                    if (it != current_path_rel.end())
                    {
                        ui::ui_context::no_line_break();
                    }
                }
            }

            ui::ui_context::horizontal_separator();

            for (const auto& file : filesystem::directory_iterator(current_path))
            {
                if (!file.is_regular_file() && !file.is_directory())
                {
                    continue;
                }

                const auto& file_path = file.path();
                const auto filename = file_path.filename().generic_string();

                const auto is_file_selected =
                    _selected_path.transform([&](const filesystem::path& p) { return p == file_path; }).value_or(false);

                ui::ui_context::selectable_text(is_file_selected, filename);
                if (ui::ui_context::is_hovered() && ui::ui_context::is_double_clicked(core::mouse_button::left))
                {
                    _selected_path = file_path;
                    // Check if the selected path is a directory or is a symlink to a directory
                    if (filesystem::is_directory(file_path))
                    {
                        _current_path = current_path / file_path;
                        _selected_path.reset();
                    }
                }
            }
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