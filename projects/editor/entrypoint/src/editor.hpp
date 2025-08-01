#ifndef tempest_editor_editor_hpp
#define tempest_editor_editor_hpp

#include <tempest/memory.hpp>
#include <tempest/pane.hpp>
#include <tempest/slot_map.hpp>
#include <tempest/tempest.hpp>
#include <tempest/tuple.hpp>
#include <tempest/ui.hpp>

namespace tempest::editor
{
    class editor_context
    {
      public:
        enum class dock_location
        {
            none,
            left,
            right,
            top,
            bottom,
            center,
        };

        editor_context() = default;
        editor_context(const editor_context&) = delete;
        editor_context(editor_context&&) noexcept = delete;
        ~editor_context() = default;

        editor_context& operator=(const editor_context&) = delete;
        editor_context& operator=(editor_context&&) noexcept = delete;

        void draw(engine_context& engine_ctx, ui::ui_context& ui_ctx);

        template <typename T>
            requires derived_from<T, ui::pane>
        T* register_pane(unique_ptr<T> p, dock_location location = dock_location::none)
        {
            auto* ptr = p.get();
            _register_pane_impl(tempest::move(p), location);
            return ptr;
        }

      private:
        bool _dockspace_needs_configure = true;
        ui::ui_context::dockspace_layout _dockspace_layout;

        vector<tuple<unique_ptr<ui::pane>, dock_location>> _panes;

        void _register_pane_impl(unique_ptr<ui::pane> p, dock_location location = dock_location::none);
    };
} // namespace tempest::editor

#endif // tempest_editor_editor_hpp
