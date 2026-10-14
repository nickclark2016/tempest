#ifndef tempest_editor_core_command_history_hpp
#define tempest_editor_core_command_history_hpp

#include <tempest/api.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/vector.hpp>

#ifndef TEMPEST_EDITOR_API
#    define TEMPEST_EDITOR_API
#endif

namespace tempest::editor
{
    /// @brief Internal undo/redo stack record for an isolated scene or play session.
    struct TEMPEST_EDITOR_API scene_undo_stack
    {
        static constexpr size_t default_saved_index = 0;

        vector<unique_ptr<editor_command>> undo_stack;
        vector<unique_ptr<editor_command>> redo_stack;
        optional<size_t> saved_index = default_saved_index;
    };

    /// @brief Transactional command history managing per-scene undo/redo stacks and drag coalescing.
    class TEMPEST_EDITOR_API command_history
    {
      public:
        static constexpr size_t default_max_depth = 100;
        static constexpr uint64_t default_gesture_id = 0;
        static constexpr scene::scene_manager* default_scene_manager = nullptr;

        explicit command_history(size_t max_depth = default_max_depth);
        ~command_history() = default;

        command_history(const command_history&) = delete;
        command_history(command_history&&) noexcept = default;
        auto operator=(const command_history&) -> command_history& = delete;
        auto operator=(command_history&&) noexcept -> command_history& = default;

        auto begin_gesture(uint64_t gesture_id = default_gesture_id) -> void;
        auto end_gesture() -> void;
        [[nodiscard]] auto is_in_gesture() const noexcept -> bool;

        auto execute(unique_ptr<editor_command> cmd, command_context& ctx) -> void;
        auto undo(command_context& ctx, scene::scene_handle scene = {}) -> bool;
        auto redo(command_context& ctx, scene::scene_handle scene = {}) -> bool;

        auto mark_saved(scene::scene_handle scene = {}) -> void;
        [[nodiscard]] auto is_dirty(scene::scene_handle scene = {}) const noexcept -> bool;
        [[nodiscard]] auto can_undo(scene::scene_handle scene = {}) const noexcept -> bool;
        [[nodiscard]] auto can_redo(scene::scene_handle scene = {}) const noexcept -> bool;

        auto clear(scene::scene_handle scene = {}) -> void;
        auto clear_all() -> void;

        [[nodiscard]] auto get_max_depth() const noexcept -> size_t
        {
            return _max_depth;
        }

        auto set_max_depth(size_t max_depth) -> void
        {
            _max_depth = max_depth;
        }

      private:
        [[nodiscard]] auto resolve_target_scene(scene::scene_handle scene) const noexcept -> scene::scene_handle;

        size_t _max_depth = default_max_depth;
        bool _in_gesture = false;
        bool _gesture_has_command = false;
        uint64_t _current_gesture_id = default_gesture_id;
        mutable scene::scene_manager* _scene_manager = default_scene_manager;
        flat_unordered_map<scene::scene_handle, scene_undo_stack> _stacks;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_command_history_hpp
