#include <tempest/profiler/zone.hpp>

namespace tempest::profiler
{
    scoped_zone_impl<true>::scoped_zone_impl(profiler_session& session, string_view name, source_location loc)
    {
        if (session.is_enabled())
        {
            _ctx = &session.get_or_register_thread();
            _ctx->begin_zone(name, loc);
            _active = true;
        }
    }

    scoped_zone_impl<true>::scoped_zone_impl(thread_profiler_context& ctx, string_view name, source_location loc)
        : _ctx(&ctx), _active(ctx.get_session().is_enabled())
    {
        if (_active)
        {
            _ctx->begin_zone(name, loc);
        }
    }

    scoped_zone_impl<true>::~scoped_zone_impl()
    {
        if (_active && _ctx != nullptr)
        {
            _ctx->end_zone();
        }
    }

    auto scoped_zone_impl<true>::add_metric(string_view name, double value, metric_unit unit) -> void
    {
        if (_active && _ctx != nullptr)
        {
            _ctx->add_metric(name, value, unit);
        }
    }

    auto scoped_zone_impl<true>::set_task_id(uint64_t task_id) -> void
    {
        if (_active && _ctx != nullptr)
        {
            _ctx->set_current_zone_task_id(task_id);
        }
    }
} // namespace tempest::profiler
