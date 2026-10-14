#include <tempest/command_history.hpp>

#include <tempest/utility.hpp>

namespace tempest::editor
{
    command_history::command_history(size_t max_depth) : _max_depth{max_depth}
    {
    }

    auto command_history::begin_gesture(uint64_t gesture_id) -> void
    {
        _in_gesture = true;
        _current_gesture_id = gesture_id;
        _gesture_has_command = false;
    }

    auto command_history::end_gesture() -> void
    {
        _in_gesture = false;
        _current_gesture_id = default_gesture_id;
        _gesture_has_command = false;
    }

    auto command_history::is_in_gesture() const noexcept -> bool
    {
        return _in_gesture;
    }

    auto command_history::resolve_target_scene(scene::scene_handle scene) const noexcept -> scene::scene_handle
    {
        if (scene.is_valid())
        {
            return scene;
        }

        if (_scene_manager != nullptr && _scene_manager->get_active_scene().is_valid())
        {
            return _scene_manager->get_active_scene();
        }

        return {};
    }

    auto command_history::execute(unique_ptr<editor_command> cmd, command_context& ctx) -> void
    {
        if (cmd == nullptr)
        {
            return;
        }

        _scene_manager = ctx.scene_manager.get();

        cmd->apply(ctx);

        const auto target = cmd->target_scene().is_valid() ? cmd->target_scene() : resolve_target_scene({});
        auto& stack = _stacks[target];

        if (_in_gesture && _gesture_has_command && !stack.undo_stack.empty())
        {
            if (stack.undo_stack.back()->merge_with(*cmd))
            {
                if (target.is_valid())
                {
                    ctx.scene_manager->set_dirty(target, is_dirty(target));
                }
                return;
            }
        }

        if (!stack.redo_stack.empty())
        {
            if (stack.saved_index.has_value() && *stack.saved_index > stack.undo_stack.size())
            {
                stack.saved_index = nullopt;
            }
            stack.redo_stack.clear();
        }

        stack.undo_stack.push_back(tempest::move(cmd));
        if (_in_gesture)
        {
            _gesture_has_command = true;
        }

        if (stack.undo_stack.size() > _max_depth)
        {
            stack.undo_stack.erase(stack.undo_stack.begin());
            if (stack.saved_index.has_value())
            {
                if (*stack.saved_index == 0)
                {
                    stack.saved_index = nullopt;
                }
                else
                {
                    --(*stack.saved_index);
                }
            }
        }

        if (target.is_valid())
        {
            ctx.scene_manager->set_dirty(target, is_dirty(target));
        }
    }

    auto command_history::undo(command_context& ctx, scene::scene_handle scene) -> bool
    {
        _scene_manager = ctx.scene_manager.get();
        const auto target = resolve_target_scene(scene);
        auto iter = _stacks.find(target);
        if (iter == _stacks.end() || iter->second.undo_stack.empty())
        {
            return false;
        }

        auto& stack = iter->second;
        auto cmd = tempest::move(stack.undo_stack.back());
        stack.undo_stack.pop_back();

        cmd->revert(ctx);
        stack.redo_stack.push_back(tempest::move(cmd));

        const auto dirty = is_dirty(target);
        if (target.is_valid())
        {
            ctx.scene_manager->set_dirty(target, dirty);
        }

        return true;
    }

    auto command_history::redo(command_context& ctx, scene::scene_handle scene) -> bool
    {
        _scene_manager = ctx.scene_manager.get();
        const auto target = resolve_target_scene(scene);
        auto iter = _stacks.find(target);
        if (iter == _stacks.end() || iter->second.redo_stack.empty())
        {
            return false;
        }

        auto& stack = iter->second;
        auto cmd = tempest::move(stack.redo_stack.back());
        stack.redo_stack.pop_back();

        cmd->apply(ctx);
        stack.undo_stack.push_back(tempest::move(cmd));

        const auto dirty = is_dirty(target);
        if (target.is_valid())
        {
            ctx.scene_manager->set_dirty(target, dirty);
        }

        return true;
    }

    auto command_history::mark_saved(scene::scene_handle scene) -> void
    {
        const auto target = resolve_target_scene(scene);
        auto& stack = _stacks[target];
        stack.saved_index = stack.undo_stack.size();
        if (_scene_manager != nullptr && target.is_valid())
        {
            _scene_manager->set_dirty(target, false);
        }
    }

    auto command_history::is_dirty(scene::scene_handle scene) const noexcept -> bool
    {
        const auto target = resolve_target_scene(scene);
        const auto iter = _stacks.find(target);
        if (iter == _stacks.end())
        {
            return false;
        }

        const auto& stack = iter->second;
        if (!stack.saved_index.has_value())
        {
            return true;
        }

        return stack.undo_stack.size() != *stack.saved_index;
    }

    auto command_history::can_undo(scene::scene_handle scene) const noexcept -> bool
    {
        const auto target = resolve_target_scene(scene);
        const auto iter = _stacks.find(target);
        if (iter == _stacks.end())
        {
            return false;
        }
        return !iter->second.undo_stack.empty();
    }

    auto command_history::can_redo(scene::scene_handle scene) const noexcept -> bool
    {
        const auto target = resolve_target_scene(scene);
        const auto iter = _stacks.find(target);
        if (iter == _stacks.end())
        {
            return false;
        }
        return !iter->second.redo_stack.empty();
    }

    auto command_history::clear(scene::scene_handle scene) -> void
    {
        const auto target = resolve_target_scene(scene);
        auto iter = _stacks.find(target);
        if (iter != _stacks.end())
        {
            iter->second.undo_stack.clear();
            iter->second.redo_stack.clear();
            iter->second.saved_index = 0;
            if (_scene_manager != nullptr && target.is_valid())
            {
                _scene_manager->set_dirty(target, false);
            }
        }
    }

    auto command_history::clear_all() -> void
    {
        for (auto& [handle, stack] : _stacks)
        {
            stack.undo_stack.clear();
            stack.redo_stack.clear();
            stack.saved_index = 0;
            if (_scene_manager != nullptr && handle.is_valid())
            {
                _scene_manager->set_dirty(handle, false);
            }
        }
        _stacks.clear();
        _in_gesture = false;
        _gesture_has_command = false;
        _current_gesture_id = default_gesture_id;
    }
} // namespace tempest::editor
