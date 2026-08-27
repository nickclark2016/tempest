#include <tempest/editor_engine_context.hpp>

#include <tempest/move.hpp>

namespace tempest::editor
{
    void editor_engine_context::register_on_editor_paint_callback(function<void(engine_context&)> callback)
    {
        _editor_callbacks.on_paint.push_back(tempest::move(callback));
    }

    void editor_engine_context::register_on_editor_update_callback(function<void(engine_context&)> callback)
    {
        _editor_callbacks.on_update.push_back(tempest::move(callback));
    }
} // namespace tempest::editor