#ifndef tempest_editor_ui_ui_hpp
#define tempest_editor_ui_ui_hpp

#include <tempest/memory.hpp>
#include <tempest/rhi.hpp>

namespace tempest::editor::ui
{
    class ui_context
    {
      public:
        ui_context(rhi::window_surface* surface, rhi::device* device) noexcept;
        ui_context(const ui_context&) = delete;
        ui_context(ui_context&&) noexcept = delete;
        ~ui_context();

        ui_context& operator=(const ui_context&) = delete;
        ui_context& operator=(ui_context&&) noexcept = delete;

        void begin_ui_commands();
        void finish_ui_commands();

        void render_ui_commands(rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> command_list,
                                rhi::work_queue& wq) noexcept;

      private:   
        struct impl;
        unique_ptr<impl> _impl = nullptr;
    };
} // namespace tempest::editor::ui

#endif // tempest_editor_ui_ui_hpp
