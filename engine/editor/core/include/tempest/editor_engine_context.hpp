#ifndef tempest_editor_editor_engine_context_hpp
#define tempest_editor_editor_engine_context_hpp

#include <tempest/api.hpp>
#include <tempest/functional.hpp>
#include <tempest/tempest.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor
{
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
        editor_engine_context(const editor_engine_context&) = delete;
        editor_engine_context(editor_engine_context&&) noexcept = delete;
        auto operator=(const editor_engine_context&) -> editor_engine_context& = delete;
        auto operator=(editor_engine_context&&) noexcept -> editor_engine_context& = delete;

        void register_on_editor_paint_callback(function<void(engine_context&)> callback);
        void register_on_editor_update_callback(function<void(engine_context&)> callback);

        [[nodiscard]] auto get_simulation_state() const -> simulation_state
        {
            return _sim_state;
        }

        auto set_simulation_state(simulation_state state) -> void
        {
            _sim_state = state;
        }

      private:
        simulation_state _sim_state = simulation_state::stopped;

        struct
        {
            vector<function<void(engine_context&)>> on_paint;
            vector<function<void(engine_context&)>> on_update;
        } _editor_callbacks;
    };
} // namespace tempest::editor

#endif // tempest_editor_editor_engine_context_hpp
