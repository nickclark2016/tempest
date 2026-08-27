#ifndef tempest_editor_core_editor_hpp
#define tempest_editor_core_editor_hpp

#include <tempest/api.hpp>
#include <tempest/concepts.hpp>
#include <tempest/functional.hpp>
#include <tempest/memory.hpp>
#include <tempest/menus/menu_item.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/tempest.hpp>
#include <tempest/ui.hpp>
#include <tempest/vector.hpp>
#include <tempest/windows/component_view.hpp>
#include <tempest/windows/editor_window.hpp>
#include <tempest/windows/entity_view_window.hpp>
#include <tempest/windows/scene_hierarchy_window.hpp>
#include <tempest/windows/viewport_window.hpp>

namespace tempest::editor
{
    class editor_engine_context;

    class TEMPEST_EDITOR_API editor_context
    {
      public:
        editor_context(editor_engine_context& ctx, window_handle win, ui_context& ui_ctx);
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

        template <derived_from<component_view_provider> T>
        auto register_component_view_provider(unique_ptr<T> provider) -> void
        {
            _entity_view->providers.push_back(tempest::move(provider));
        }

        auto register_on_paint_callback(function<void(engine_context&)>) -> void;
        auto register_on_update_callback(function<void(engine_context&)>) -> void;

        auto register_menu_item(unique_ptr<menu_item> menu) -> void;

        template <derived_from<menu_item> T>
        auto register_menu_item(unique_ptr<T> menu) -> void
        {
            register_menu_item(unique_ptr<menu_item>(menu.release()));
        }

      private:
        editor_engine_context* _engine_ctx;
        window_handle _win;
        ui_context* _ui_ctx;

        vector<unique_ptr<editor_window>> _windows;
        entity_view_window* _entity_view = nullptr;
        scene_hierarchy_window* _scene_hierarchy_view = nullptr;
        viewport_window* _viewport_view = nullptr;

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
