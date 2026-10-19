#ifndef tempest_editor_core_play_mode_controller_hpp
#define tempest_editor_core_play_mode_controller_hpp

#include <tempest/archetype.hpp>
#include <tempest/checked.hpp>
#include <tempest/editor_camera.hpp>
#include <tempest/functional.hpp>
#include <tempest/game_module.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/scene_manager.hpp>

namespace tempest::editor
{
    class editor_engine_context;
    enum class simulation_state : uint8_t;

    class TEMPEST_EDITOR_API play_mode_controller
    {
      public:
        explicit play_mode_controller(editor_engine_context& context);
        ~play_mode_controller() = default;

        play_mode_controller(const play_mode_controller&) = delete;
        play_mode_controller(play_mode_controller&&) noexcept = delete;
        auto operator=(const play_mode_controller&) -> play_mode_controller& = delete;
        auto operator=(play_mode_controller&&) noexcept -> play_mode_controller& = delete;

        auto play(const play_settings& settings = {}) -> bool;
        auto pause() -> void;
        auto step() -> bool;
        auto stop() -> void;

        auto set_on_play_begin(function<void(const play_settings&)> callback) -> void;
        auto set_on_play_end(function<void()> callback) -> void;

        [[nodiscard]] auto get_state() const noexcept -> simulation_state;
        [[nodiscard]] auto is_playing() const noexcept -> bool;
        [[nodiscard]] auto is_paused() const noexcept -> bool;
        [[nodiscard]] auto is_edit() const noexcept -> bool;

      private:
        non_null<editor_engine_context> _context;
        optional<ecs::registry_snapshot> _saved_registry_snapshot;
        optional<scene::scene_manager_snapshot> _saved_scene_snapshot;
        optional<editor_camera> _saved_editor_camera;
        function<void(const play_settings&)> _on_play_begin;
        function<void()> _on_play_end;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_play_mode_controller_hpp
