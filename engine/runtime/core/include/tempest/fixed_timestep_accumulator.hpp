#ifndef tempest_engine_fixed_timestep_accumulator_hpp
#define tempest_engine_fixed_timestep_accumulator_hpp

#include <tempest/algorithm.hpp>
#include <tempest/chrono.hpp>
#include <tempest/int.hpp>

namespace tempest
{
    class fixed_timestep_accumulator
    {
      public:
        static constexpr chrono::duration<double> default_fixed_delta = chrono::duration<double>{1.0 / 60.0};
        static constexpr chrono::duration<double> default_max_frame_delta = chrono::duration<double>{0.1};
        static constexpr chrono::nanoseconds default_fixed_delta_ns = chrono::nanoseconds{16'666'666};
        static constexpr chrono::nanoseconds default_max_frame_delta_ns = chrono::nanoseconds{100'000'000};
        static constexpr double default_time_scale = 1.0;

        explicit fixed_timestep_accumulator(chrono::nanoseconds fixed_delta,
                                            chrono::nanoseconds max_frame_delta = default_max_frame_delta_ns) noexcept
            : _fixed_delta_ns{fixed_delta},
              _max_frame_delta_ns{max_frame_delta},
              _fixed_delta_d{chrono::duration_cast<chrono::duration<double>>(fixed_delta)},
              _max_frame_delta_d{chrono::duration_cast<chrono::duration<double>>(max_frame_delta)}
        {
        }

        explicit fixed_timestep_accumulator(chrono::duration<double> fixed_delta = default_fixed_delta,
                                            chrono::duration<double> max_frame_delta = default_max_frame_delta) noexcept
            : _fixed_delta_ns{chrono::duration_cast<chrono::nanoseconds>(fixed_delta)},
              _max_frame_delta_ns{chrono::duration_cast<chrono::nanoseconds>(max_frame_delta)},
              _fixed_delta_d{fixed_delta},
              _max_frame_delta_d{max_frame_delta}
        {
        }

        explicit fixed_timestep_accumulator(float fixed_delta, float max_frame_delta = 0.1F) noexcept
            : fixed_timestep_accumulator(chrono::duration<double>{static_cast<double>(fixed_delta)},
                                         chrono::duration<double>{static_cast<double>(max_frame_delta)})
        {
        }

        auto accumulate(chrono::nanoseconds delta) noexcept -> void
        {
            if (_time_scale <= 0.0 || delta.count() <= 0)
            {
                return;
            }

            const auto scaled_delta =
                (_time_scale == 1.0)
                    ? delta
                    : chrono::nanoseconds{static_cast<int64_t>(static_cast<double>(delta.count()) * _time_scale)};
            const auto clamped_delta = (scaled_delta < _max_frame_delta_ns) ? scaled_delta : _max_frame_delta_ns;
            _accumulated_time += clamped_delta;
        }

        template <typename Rep, typename Period>
        auto accumulate(chrono::duration<Rep, Period> delta) noexcept -> void
        {
            accumulate(chrono::duration_cast<chrono::nanoseconds>(delta));
        }

        auto accumulate(float delta_seconds) noexcept -> void
        {
            accumulate(chrono::duration<double>{static_cast<double>(delta_seconds)});
        }

        [[nodiscard]] auto has_pending_ticks() const noexcept -> bool
        {
            return _accumulated_time >= _fixed_delta_ns;
        }

        auto consume_tick() noexcept -> void
        {
            if (_accumulated_time >= _fixed_delta_ns)
            {
                _accumulated_time -= _fixed_delta_ns;
                ++_total_ticks;
            }
        }

        [[nodiscard]] auto alpha() const noexcept -> float
        {
            if (_fixed_delta_ns.count() <= 0)
            {
                return 0.0F;
            }

            const auto raw_alpha = static_cast<float>(static_cast<double>(_accumulated_time.count()) /
                                                      static_cast<double>(_fixed_delta_ns.count()));
            return tempest::clamp(raw_alpha, 0.0F, 1.0F);
        }

        auto reset() noexcept -> void
        {
            _accumulated_time = chrono::nanoseconds::zero();
        }

        template <typename Rep, typename Period, typename TickFn>
        auto step(chrono::duration<Rep, Period> delta, TickFn&& tick_fn) -> void
        {
            accumulate(delta);
            while (has_pending_ticks())
            {
                if constexpr (requires { tick_fn(_fixed_delta_d); })
                {
                    tick_fn(_fixed_delta_d);
                }
                else
                {
                    tick_fn(static_cast<float>(_fixed_delta_d.count()));
                }
                consume_tick();
            }
        }

        template <typename TickFn>
        auto step(float delta_seconds, TickFn&& tick_fn) -> void
        {
            step(chrono::duration<double>{static_cast<double>(delta_seconds)}, tempest::forward<TickFn>(tick_fn));
        }

        [[nodiscard]] auto fixed_delta() const noexcept -> chrono::duration<double>
        {
            return _fixed_delta_d;
        }

        [[nodiscard]] auto fixed_delta_ns() const noexcept -> chrono::nanoseconds
        {
            return _fixed_delta_ns;
        }

        auto set_fixed_delta(chrono::duration<double> fixed_delta) noexcept -> void
        {
            _fixed_delta_d = fixed_delta;
            _fixed_delta_ns = chrono::duration_cast<chrono::nanoseconds>(fixed_delta);
        }

        auto set_fixed_delta(chrono::nanoseconds fixed_delta) noexcept -> void
        {
            _fixed_delta_ns = fixed_delta;
            _fixed_delta_d = chrono::duration_cast<chrono::duration<double>>(fixed_delta);
        }

        auto set_fixed_delta(float fixed_delta) noexcept -> void
        {
            set_fixed_delta(chrono::duration<double>{static_cast<double>(fixed_delta)});
        }

        [[nodiscard]] auto max_frame_delta() const noexcept -> chrono::duration<double>
        {
            return _max_frame_delta_d;
        }

        [[nodiscard]] auto max_frame_delta_ns() const noexcept -> chrono::nanoseconds
        {
            return _max_frame_delta_ns;
        }

        auto set_max_frame_delta(chrono::duration<double> max_delta) noexcept -> void
        {
            _max_frame_delta_d = max_delta;
            _max_frame_delta_ns = chrono::duration_cast<chrono::nanoseconds>(max_delta);
        }

        auto set_max_frame_delta(chrono::nanoseconds max_delta) noexcept -> void
        {
            _max_frame_delta_ns = max_delta;
            _max_frame_delta_d = chrono::duration_cast<chrono::duration<double>>(max_delta);
        }

        auto set_max_frame_delta(float max_delta) noexcept -> void
        {
            set_max_frame_delta(chrono::duration<double>{static_cast<double>(max_delta)});
        }

        [[nodiscard]] auto time_scale() const noexcept -> double
        {
            return _time_scale;
        }

        auto set_time_scale(double scale) noexcept -> void
        {
            _time_scale = scale;
        }

        [[nodiscard]] auto accumulated_time() const noexcept -> chrono::nanoseconds
        {
            return _accumulated_time;
        }

        [[nodiscard]] auto accumulated_time_d() const noexcept -> chrono::duration<double>
        {
            return chrono::duration_cast<chrono::duration<double>>(_accumulated_time);
        }

        [[nodiscard]] auto total_ticks() const noexcept -> uint64_t
        {
            return _total_ticks;
        }

      private:
        chrono::nanoseconds _fixed_delta_ns = default_fixed_delta_ns;
        chrono::nanoseconds _max_frame_delta_ns = default_max_frame_delta_ns;
        chrono::duration<double> _fixed_delta_d = default_fixed_delta;
        chrono::duration<double> _max_frame_delta_d = default_max_frame_delta;
        double _time_scale = default_time_scale;
        chrono::nanoseconds _accumulated_time = chrono::nanoseconds::zero();
        uint64_t _total_ticks = 0;
    };
} // namespace tempest

#endif // tempest_engine_fixed_timestep_accumulator_hpp
