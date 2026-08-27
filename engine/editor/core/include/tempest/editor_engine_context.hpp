#ifndef tempest_editor_editor_engine_context_hpp
#define tempest_editor_editor_engine_context_hpp

#include <tempest/api.hpp>
#include <tempest/functional.hpp>
#include <tempest/tempest.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor
{
    class ui_context;

    enum class simulation_state
    {
        stopped,
        pause,
        play
    };

    class TEMPEST_EDITOR_API editor_engine_context final : public standalone_engine_context
    {
      public:
        editor_engine_context() = default;
        ~editor_engine_context() override = default;

        editor_engine_context(const editor_engine_context&) = delete;
        editor_engine_context(editor_engine_context&&) noexcept = delete;
        auto operator=(const editor_engine_context&) -> editor_engine_context& = delete;
        auto operator=(editor_engine_context&&) noexcept -> editor_engine_context& = delete;

        void register_on_editor_paint_callback(function<void(engine_context&)> callback);
        void register_on_editor_update_callback(function<void(engine_context&)> callback);

        [[nodiscard]] auto get_simulation_state() const noexcept -> simulation_state
        {
            return _sim_state;
        }

        auto set_simulation_state(simulation_state state) noexcept -> void
        {
            _sim_state = state;
        }

        auto set_ui_context(ui_context* ui_ctx) noexcept -> void
        {
            _ui_ctx = ui_ctx;
        }

        [[nodiscard]] auto get_ui_context() const noexcept -> ui_context*
        {
            return _ui_ctx;
        }

        auto run() -> void override;

      private:
        simulation_state _sim_state = simulation_state::stopped;
        ui_context* _ui_ctx{nullptr};

        struct
        {
            vector<function<void(engine_context&)>> on_paint;
            vector<function<void(engine_context&)>> on_update;
        } _editor_callbacks;

        auto _render_editor_frame() -> void;
    };
} // namespace tempest::editor

#endif // tempest_editor_editor_engine_context_hpp
