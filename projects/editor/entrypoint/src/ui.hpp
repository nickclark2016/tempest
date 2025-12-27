#ifndef tempest_editor_ui_ui_hpp
#define tempest_editor_ui_ui_hpp

#include <tempest/frame_graph.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/slot_map.hpp>
#include <tempest/string_view.hpp>
#include <tempest/variant.hpp>
#include <tempest/vec2.hpp>

namespace tempest::editor::ui
{
    class ui_context
    {
      public:
        ui_context(rhi::window_surface* surface, rhi::device* device, rhi::image_format target_fmt,
                   uint32_t frames_in_flight) noexcept;
        ui_context(const ui_context&) = delete;
        ui_context(ui_context&&) noexcept = delete;
        ~ui_context();

        ui_context& operator=(const ui_context&) = delete;
        ui_context& operator=(ui_context&&) noexcept = delete;

        void begin_ui_commands();
        void finish_ui_commands();

        void render_ui_commands(graphics::graphics_task_execution_context& exec_ctx) noexcept;

      private:
        struct impl;
        unique_ptr<impl> _impl = nullptr;

        void _init_window_backend();
        void _init_render_backend();
        void _setup_font_textures();
    };

    graphics::graph_resource_handle<rhi::rhi_handle_type::image> create_ui_pass(
        string name, ui_context& ui_ctx, graphics::graph_builder& builder, rhi::device& dev,
        graphics::graph_resource_handle<rhi::rhi_handle_type::image> render_target);
} // namespace tempest::editor::ui

#endif // tempest_editor_ui_ui_hpp
