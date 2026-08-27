#ifndef tempest_editor_ui_ui_hpp
#define tempest_editor_ui_ui_hpp

#include <tempest/api.hpp>
#include <tempest/cstring_view.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/rhi.hpp>
#include <tempest/string.hpp>
#include <tempest/vec3.hpp>
#include <tempest/window_manager.hpp>

struct ImGuiContext;

namespace tempest::editor
{
    class TEMPEST_EDITOR_API ui_context
    {
      public:
        ui_context(window_manager& win_mgr, window_handle win, rhi::device& device, rhi::data_format target_format,
                   uint32_t frames_in_flight);
        ui_context(const ui_context&) = delete;
        ui_context(ui_context&&) noexcept = delete;
        ~ui_context();

        auto operator=(const ui_context&) -> ui_context& = delete;
        auto operator=(ui_context&&) noexcept -> ui_context& = delete;

        auto begin_ui_commands() -> void;
        auto finish_ui_commands() -> void;

        auto render_ui_commands(rhi::command_list& cmd, uint32_t width, uint32_t height) -> void;

        [[nodiscard]] auto get_imgui_context() const noexcept -> ImGuiContext*;

      private:
        struct impl;
        unique_ptr<impl> _impl;
    };

    namespace ui
    {
        TEMPEST_EDITOR_API auto image(rhi::descriptor_handle descriptor, uint32_t width, uint32_t height) -> void;

        TEMPEST_EDITOR_API auto scalar(cstring_view label, float input) -> float;
        TEMPEST_EDITOR_API auto float3(cstring_view label, math::float3 input) -> math::float3;
        TEMPEST_EDITOR_API auto color3(cstring_view label, math::float3 input) -> math::float3;

        TEMPEST_EDITOR_API auto drag_integral(cstring_view label, int input, int minimum, int maximum) -> int;
        TEMPEST_EDITOR_API auto drag_scalar(cstring_view label, float input, float minimum, float maximum) -> float;

        TEMPEST_EDITOR_API auto input_text(cstring_view label, string& input) -> bool;
        TEMPEST_EDITOR_API auto input_text_with_hint(cstring_view label, cstring_view hint, string& input) -> bool;

        TEMPEST_EDITOR_API auto centered_button(cstring_view label) -> bool;
    } // namespace ui
} // namespace tempest::editor

#endif // tempest_editor_ui_ui_hpp