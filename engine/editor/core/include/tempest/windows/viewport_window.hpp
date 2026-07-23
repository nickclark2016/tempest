#ifndef tempest_editor_core_viewport_window_hpp
#define tempest_editor_core_viewport_window_hpp

#include <tempest/renderer.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vec2.hpp>
#include <tempest/windows/editor_window.hpp>

namespace tempest::editor
{
    class editor_engine_context;

    class TEMPEST_EDITOR_API viewport_window final : public editor_window
    {
      public:
        explicit viewport_window(editor_engine_context& ctx);

        auto desired_initial_dock() const -> editor_window::dock_location override;
        auto window_name() const -> string_view override;
        auto draw() -> void override;

        auto aspect_ratio() const -> float;

        [[nodiscard]] auto is_mode_supported([[maybe_unused]] simulation_state state) const noexcept -> bool override
        {
            return true;
        }

      private:
        editor_engine_context* _ctx;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::image> _viewport_texture;
        math::uint2 _viewport_size{};
        bool _open = true;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_viewport_window_hpp
