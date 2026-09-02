#ifndef tempest_editor_editor_engine_context_hpp
#define tempest_editor_editor_engine_context_hpp

#include <tempest/api.hpp>
#include <tempest/editor_camera.hpp>
#include <tempest/functional.hpp>
#include <tempest/memory.hpp>
#include <tempest/profiler/profiler.hpp>
#include <tempest/string_view.hpp>
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
        editor_engine_context();
        ~editor_engine_context() override;

        editor_engine_context(const editor_engine_context&) = delete;
        editor_engine_context(editor_engine_context&&) noexcept = delete;
        auto operator=(const editor_engine_context&) -> editor_engine_context& = delete;
        auto operator=(editor_engine_context&&) noexcept -> editor_engine_context& = delete;

        void register_on_editor_paint_callback(function<void(engine_context&)> callback);
        void register_on_editor_update_callback(function<void(engine_context&)> callback);
        void clear_editor_callbacks();

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

        [[nodiscard]] auto get_editor_camera() noexcept -> editor_camera&
        {
            return _editor_camera;
        }

        [[nodiscard]] auto get_editor_camera() const noexcept -> const editor_camera&
        {
            return _editor_camera;
        }

        [[nodiscard]] auto get_web_server() noexcept -> profiler::web_server*
        {
            return _web_server.get();
        }

        [[nodiscard]] auto get_web_server() const noexcept -> const profiler::web_server*
        {
            return _web_server.get();
        }

        [[nodiscard]] auto get_last_telemetry_frame() const noexcept -> const profiler::telemetry_frame&
        {
            return _last_telemetry_frame;
        }

        [[nodiscard]] auto get_last_cpu_time_ms() const noexcept -> float
        {
            return _last_cpu_time_ms;
        }

        [[nodiscard]] auto get_last_gpu_time_ms() const noexcept -> float
        {
            return _last_gpu_time_ms;
        }

        [[nodiscard]] auto get_frame_index() const noexcept -> uint64_t
        {
            return _frame_index;
        }

        [[nodiscard]] auto is_recording() const noexcept -> bool
        {
            return _is_recording;
        }

        auto set_recording(bool recording) -> void;
        auto toggle_recording() -> bool;
        auto insert_bookmark_marker(string_view name = {}) -> void;
        auto open_profiler_in_browser() -> void;
        auto open_captures_folder() -> void;

        [[nodiscard]] auto is_live_stream_enabled() const noexcept -> bool
        {
            return _live_stream_enabled;
        }

        auto set_live_stream_enabled(bool enabled) noexcept -> void
        {
            _live_stream_enabled = enabled;
        }

        [[nodiscard]] auto is_gpu_stats_enabled() const noexcept -> bool
        {
            return _capture_gpu_stats;
        }

        auto set_gpu_stats_enabled(bool enabled) noexcept -> void;

        auto collect_and_broadcast_telemetry() -> void;

        auto run() -> void override;

      private:
        simulation_state _sim_state = simulation_state::stopped;
        ui_context* _ui_ctx{nullptr};
        editor_camera _editor_camera{};

        unique_ptr<profiler::web_server> _web_server{};
        uint64_t _frame_index{0};
        profiler::telemetry_frame _last_telemetry_frame{};
        float _last_cpu_time_ms{0.0f};
        float _last_gpu_time_ms{0.0f};
        bool _capture_gpu_stats{true};
        bool _live_stream_enabled{true};
        bool _is_recording{false};

        struct
        {
            vector<function<void(engine_context&)>> on_paint;
            vector<function<void(engine_context&)>> on_update;
        } _editor_callbacks;

        auto _render_editor_frame() -> void;
    };
} // namespace tempest::editor

#endif // tempest_editor_editor_engine_context_hpp
