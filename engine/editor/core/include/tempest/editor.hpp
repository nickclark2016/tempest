#ifndef tempest_editor_core_editor_hpp
#define tempest_editor_core_editor_hpp

#include <tempest/memory.hpp>
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
        editor_context(editor_engine_context& ctx, rhi::window_surface& win_surface, ui_context& ui_ctx);

        auto draw() -> void;

        [[nodiscard]] auto get_color_output() const noexcept
            -> graphics::graph_resource_handle<rhi::rhi_handle_type::image>;

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

      private:
        editor_engine_context* _engine_ctx;
        rhi::window_surface* _win_surface;
        ui_context* _ui_ctx;

        vector<unique_ptr<editor_window>> _windows;
        entity_view_window* _entity_view = nullptr;
        scene_hierarchy_window* _scene_hierarchy_view = nullptr;
        viewport_window* _viewport_view = nullptr;

        graphics::graph_resource_handle<rhi::rhi_handle_type::image> _final_color_target;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_editor_hpp
