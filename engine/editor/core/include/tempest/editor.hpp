#ifndef tempest_editor_core_editor_hpp
#define tempest_editor_core_editor_hpp

#include <tempest/windows/editor_window.hpp>
#include <tempest/memory.hpp>
#include <tempest/string_view.hpp>
#include <tempest/tempest.hpp>
#include <tempest/ui.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor
{
    class TEMPEST_EDITOR_API editor_context
    {
      public:
        editor_context(engine_context& ctx, rhi::window_surface& win_surface, ui_context& ui_ctx);

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

      private:
        engine_context* _engine_ctx;
        rhi::window_surface* _win_surface;
        ui_context* _ui_ctx;

        vector<unique_ptr<editor_window>> _windows;

        graphics::graph_resource_handle<rhi::rhi_handle_type::image> _final_color_target;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_editor_hpp
