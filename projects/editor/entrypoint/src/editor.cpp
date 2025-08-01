#include "editor.hpp"

#include <tempest/ui.hpp>

namespace tempest::editor
{
    namespace
    {
        auto configure_dockspace(ui::ui_context& ui_context,
                                 const vector<tuple<unique_ptr<ui::pane>, editor_context::dock_location>>& panes)
        {
            using ds_config_node = ui::ui_context::dockspace_configure_node;

            auto dock_configure = ds_config_node{};

            dock_configure.left = make_unique<ds_config_node>();
            dock_configure.left->size = 0.2f;
            dock_configure.right = make_unique<ds_config_node>();
            dock_configure.right->size = 0.2f;
            dock_configure.bottom = make_unique<ds_config_node>();
            dock_configure.bottom->size = 0.2f;
            dock_configure.top = make_unique<ds_config_node>();
            dock_configure.top->size = 0.2f;

            for (const auto& [pane_ptr, location] : panes)
            {
                switch (location)
                {
                case editor_context::dock_location::left:
                    dock_configure.left->docked_windows.push_back(pane_ptr->name());
                    break;
                case editor_context::dock_location::right:
                    dock_configure.right->docked_windows.push_back(pane_ptr->name());
                    break;
                case editor_context::dock_location::top:
                    dock_configure.top->docked_windows.push_back(pane_ptr->name());
                    break;
                case editor_context::dock_location::bottom:
                    dock_configure.bottom->docked_windows.push_back(pane_ptr->name());
                    break;
                case editor_context::dock_location::center:
                    dock_configure.docked_windows.push_back(pane_ptr->name());
                    break;
                case editor_context::dock_location::none:
                    // If the pane is not docked, we do not add it to the dockspace configuration.
                    break;
                }
            }

            auto dock_config_info = ui::ui_context::dockspace_configure_info{
                .root = tempest::move(dock_configure),
                .name = "TempestDockspace",
            };

            return ui_context.configure_dockspace(tempest::move(dock_config_info));
        }

        enum class menu_bar_item
        {
            none,
            exit,
        };

        menu_bar_item draw_menu_bar()
        {
            auto action = menu_bar_item::none;

            if (ui::ui_context::begin_menu_bar())
            {
                if (ui::ui_context::begin_menu("File"))
                {
                    if (ui::ui_context::menu_item("Exit"))
                    {
                        action = menu_bar_item::exit;
                    }

                    ui::ui_context::end_menu();
                }

                ui::ui_context::end_menu_bar();
            }

            return action;
        }

        void handle_menu_bar_action(menu_bar_item action, engine_context& ctx)
        {
            switch (action)
            {
            case menu_bar_item::exit:
                ctx.request_close();
                break;
            default:
                break;
            }
        }
    } // namespace

    void editor_context::draw(engine_context& engine_ctx, ui::ui_context& ui_ctx)
    {
        ui_ctx.begin_ui_commands();

        if (ui::ui_context::begin_window({
                .name = "Editor Dockspace",
                .position = ui::ui_context::viewport_origin_tag,
                .size = ui::ui_context::fullscreen_tag,
                .flags =
                    make_enum_mask(ui::ui_context::window_flags::no_title, ui::ui_context::window_flags::no_collapse,
                                   ui::ui_context::window_flags::no_resize, ui::ui_context::window_flags::no_move,
                                   ui::ui_context::window_flags::no_bring_to_front_on_focus,
                                   ui::ui_context::window_flags::no_navigation_focus,
                                   ui::ui_context::window_flags::no_docking, ui::ui_context::window_flags::menubar),
            }))
        {
            ui::ui_context::dockspace(ui::ui_context::get_dockspace_id("TempestDockspace"));

            const auto menu_action = draw_menu_bar();
            handle_menu_bar_action(menu_action, engine_ctx);

            if (_dockspace_needs_configure)
            {
                _dockspace_layout = configure_dockspace(ui_ctx, _panes);
                _dockspace_needs_configure = false;
            }
        }

        ui::ui_context::end_window();

        // Render the panes
        for (auto it = _panes.begin(); it != _panes.end();)
        {
            auto& pane_ptr = get<0>(*it);
            if (pane_ptr->should_render())
            {
                pane_ptr->render();
            }

            if (pane_ptr->should_close())
            {
                it = _panes.erase(it);
            }
            else
            {
                ++it; // Move to the next pane
            }
        }

        ui_ctx.finish_ui_commands();
    }

    void editor_context::_register_pane_impl(unique_ptr<ui::pane> p, dock_location location)
    {
        if (p == nullptr)
        {
            return;
        }

        _panes.emplace_back(tempest::move(p), location);
        // If the pane is docked, we need to ensure the dockspace is configured
        if (location != dock_location::none)
        {
            _dockspace_needs_configure = true;
        }
    }
} // namespace tempest::editor