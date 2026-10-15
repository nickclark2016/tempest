#ifndef tempest_editor_core_editor_hpp
#define tempest_editor_core_editor_hpp

#include <tempest/api.hpp>
#include <tempest/concepts.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/functional.hpp>
#include <tempest/memory.hpp>
#include <tempest/menus/menu_item.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/tempest.hpp>
#include <tempest/ui.hpp>
#include <tempest/vector.hpp>
#include <tempest/command_history.hpp>
#include <tempest/windows/editor_window.hpp>
#include <tempest/windows/hierarchy_window.hpp>
#include <tempest/windows/inspector_window.hpp>
#include <tempest/windows/log_view_window.hpp>
#include <tempest/windows/viewport_window.hpp>

namespace tempest::editor
{
    class editor_engine_context;

    class TEMPEST_EDITOR_API editor_context
    {
      public:
        editor_context(editor_engine_context& ctx, window_handle win, ui_context& ui_ctx);
        ~editor_context();
        editor_context(const editor_context&) = delete;
        editor_context(editor_context&&) noexcept = delete;
        auto operator=(const editor_context&) -> editor_context& = delete;
        auto operator=(editor_context&&) noexcept -> editor_context& = delete;

        auto draw() -> void;

        template <derived_from<editor_window> T>
        auto register_window(unique_ptr<T> window) -> T*
        {
            auto ptr = window.get();
            _windows.push_back(tempest::move(window));
            return ptr;
        }

        auto register_on_paint_callback(function<void(engine_context&)>) -> void;
        auto register_on_update_callback(function<void(engine_context&)>) -> void;

        auto register_menu_item(unique_ptr<menu_item> menu) -> void;

        template <derived_from<menu_item> T>
        auto register_menu_item(unique_ptr<menu_item> menu) -> void
        {
            register_menu_item(unique_ptr<menu_item>(menu.release()));
        }

        [[nodiscard]] auto get_log_view_window() noexcept -> log_view_window*
        {
            return _log_view;
        }

        [[nodiscard]] auto get_log_view_window() const noexcept -> const log_view_window*
        {
            return _log_view;
        }

        [[nodiscard]] auto get_inspector_window() noexcept -> inspector_window*
        {
            return _inspector_view;
        }

        [[nodiscard]] auto get_inspector_window() const noexcept -> const inspector_window*
        {
            return _inspector_view;
        }

        [[nodiscard]] auto get_selection() noexcept -> editor_selection&
        {
            return _selection;
        }

        [[nodiscard]] auto get_selection() const noexcept -> const editor_selection&
        {
            return _selection;
        }

        [[nodiscard]] auto get_command_history() noexcept -> command_history&
        {
            return _history;
        }

        [[nodiscard]] auto get_command_history() const noexcept -> const command_history&
        {
            return _history;
        }

      private:
        auto _process_shortcuts() -> void;
        auto _draw_status_bar() -> void;
        auto _draw_log_status_pill() -> void;
        auto _calculate_log_pill_width() const -> float;
        auto _draw_profiler_status_pill() -> void;
        auto _calculate_profiler_pill_width() const -> float;
        auto _draw_profiler_tooltip() -> void;

        editor_engine_context* _engine_ctx;
        window_handle _win;
        ui_context* _ui_ctx;

        static constexpr inspector_window* default_inspector_view = nullptr;
        static constexpr hierarchy_window* default_hierarchy_view = nullptr;
        static constexpr viewport_window* default_viewport_view = nullptr;
        static constexpr log_view_window* default_log_view = nullptr;

        command_history _history;
        editor_selection _selection;
        vector<unique_ptr<editor_window>> _windows;
        inspector_window* _inspector_view = default_inspector_view;
        hierarchy_window* _hierarchy_view = default_hierarchy_view;
        viewport_window* _viewport_view = default_viewport_view;
        log_view_window* _log_view = default_log_view;

        class menu_hierarchy
        {
          public:
            menu_hierarchy() = default;
            menu_hierarchy(const menu_hierarchy&) = delete;
            menu_hierarchy(menu_hierarchy&&) noexcept = default;
            auto operator=(const menu_hierarchy&) -> menu_hierarchy& = delete;
            auto operator=(menu_hierarchy&&) noexcept -> menu_hierarchy& = default;

            void draw();
            void add_menu_item(unique_ptr<menu_item> item);

          private:
            struct menu_node
            {
                ~menu_node();

                string name;
                vector<unique_ptr<menu_node>> children;
                unique_ptr<menu_item> menu;
            };

            vector<unique_ptr<menu_node>> _root_nodes;
        };

        menu_hierarchy _menus;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_editor_hpp
