#ifndef tempest_profiler_zone_hpp
#define tempest_profiler_zone_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string_view.hpp>

namespace tempest::profiler
{
    template <bool Enabled = true>
    class scoped_zone_impl;

    template <>
    class TEMPEST_API scoped_zone_impl<true>
    {
      public:
        scoped_zone_impl(profiler_session& session, string_view name, source_location loc = source_location::current());
        scoped_zone_impl(thread_profiler_context& ctx, string_view name,
                         source_location loc = source_location::current());
        ~scoped_zone_impl();

        scoped_zone_impl(const scoped_zone_impl&) = delete;
        scoped_zone_impl& operator=(const scoped_zone_impl&) = delete;
        scoped_zone_impl(scoped_zone_impl&&) = delete;
        scoped_zone_impl& operator=(scoped_zone_impl&&) = delete;

        auto add_metric(string_view name, double value, metric_unit unit = metric_unit::raw) -> void;
        auto set_task_id(uint64_t task_id) -> void;

      private:
        thread_profiler_context* _ctx{nullptr};
        bool _active{false};
    };

    template <>
    class scoped_zone_impl<false>
    {
      public:
        constexpr scoped_zone_impl([[maybe_unused]] profiler_session& session, [[maybe_unused]] string_view name,
                                   [[maybe_unused]] source_location loc = source_location::current()) noexcept
        {
        }

        constexpr scoped_zone_impl([[maybe_unused]] thread_profiler_context& ctx, [[maybe_unused]] string_view name,
                                   [[maybe_unused]] source_location loc = source_location::current()) noexcept
        {
        }

        ~scoped_zone_impl() = default;

        scoped_zone_impl(const scoped_zone_impl&) = delete;
        scoped_zone_impl& operator=(const scoped_zone_impl&) = delete;
        scoped_zone_impl(scoped_zone_impl&&) = delete;
        scoped_zone_impl& operator=(scoped_zone_impl&&) = delete;

        constexpr auto add_metric([[maybe_unused]] string_view name, [[maybe_unused]] double value,
                                  [[maybe_unused]] metric_unit unit = metric_unit::raw) noexcept -> void
        {
        }

        constexpr auto set_task_id([[maybe_unused]] uint64_t task_id) noexcept -> void
        {
        }
    };

    using scoped_zone = scoped_zone_impl<true>;
    using disabled_scoped_zone = scoped_zone_impl<false>;

    inline auto emit_marker(profiler_session& session, string_view name,
                            source_location loc = source_location::current()) -> void
    {
        if (session.is_enabled())
        {
            auto& ctx = session.get_or_register_thread();
            ctx.add_marker(name, loc);
        }
    }

    inline auto emit_marker(thread_profiler_context& ctx, string_view name,
                            source_location loc = source_location::current()) -> void
    {
        if (ctx.get_session().is_enabled())
        {
            ctx.add_marker(name, loc);
        }
    }

    inline auto emit_metric(profiler_session& session, string_view name, double value,
                            metric_unit unit = metric_unit::raw) -> void
    {
        if (session.is_enabled())
        {
            auto& ctx = session.get_or_register_thread();
            ctx.add_metric(name, value, unit);
        }
    }

    inline auto emit_metric(thread_profiler_context& ctx, string_view name, double value,
                            metric_unit unit = metric_unit::raw) -> void
    {
        if (ctx.get_session().is_enabled())
        {
            ctx.add_metric(name, value, unit);
        }
    }
} // namespace tempest::profiler

#endif // tempest_profiler_zone_hpp
