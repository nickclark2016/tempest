#ifndef tempest_editor_ui_ui_hpp
#define tempest_editor_ui_ui_hpp

#include <tempest/frame_graph.hpp>
#include <tempest/vec3.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/slot_map.hpp>
#include <tempest/string_view.hpp>
#include <tempest/variant.hpp>
#include <tempest/vec2.hpp>

namespace tempest::editor
{
    class TEMPEST_EDITOR_API ui_context
    {
      public:
        ui_context(rhi::window_surface* surface, rhi::device* device, rhi::image_format target_fmt,
                   uint32_t frames_in_flight) noexcept;
        ui_context(const ui_context&) = delete;
        ui_context(ui_context&&) noexcept = delete;
        ~ui_context();

        ui_context& operator=(const ui_context&) = delete;
        ui_context& operator=(ui_context&&) noexcept = delete;

        auto begin_ui_commands() -> void;
        auto finish_ui_commands() -> void;

        auto render_ui_commands(graphics::graphics_task_execution_context& exec_ctx) noexcept -> void;

      private:
        struct impl;
        unique_ptr<impl> _impl = nullptr;

        auto _init_window_backend() -> void;
        auto _init_render_backend() -> void;
        auto _setup_font_textures() -> void;
    };

    auto create_ui_pass(string name, ui_context& ui_ctx, graphics::graph_builder& builder, rhi::device& dev,
                        graphics::graph_resource_handle<rhi::rhi_handle_type::image> render_target,
                        span<graphics::graph_resource_handle<rhi::rhi_handle_type::image>> targets_to_read = {})
        -> graphics::graph_resource_handle<rhi::rhi_handle_type::image>;

    namespace ui
    {
        TEMPEST_EDITOR_API auto image(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> img, uint32_t width, uint32_t height) -> void;

        TEMPEST_EDITOR_API auto scalar(string_view label, float input) -> float;
        TEMPEST_EDITOR_API auto float3(string_view label, math::float3 input) -> math::float3;
        TEMPEST_EDITOR_API auto color3(string_view label, math::float3 input) -> math::float3;

        TEMPEST_EDITOR_API auto drag_integral(string_view label, int input, int minimum, int maximum) -> int;
        TEMPEST_EDITOR_API auto drag_scalar(string_view label, float input, float minimum, float maximum) -> float;
    }
} // namespace tempest::editor::ui

#endif // tempest_editor_ui_ui_hpp