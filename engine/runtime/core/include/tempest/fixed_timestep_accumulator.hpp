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
        static constexpr double default_time_scale = 1.0;

        explicit fixed_timestep_accumulator(chrono::duration<double> fixed_delta = default_fixed_delta,
                                            chrono::duration<double> max_frame_delta = default_max_frame_delta) noexcept
            : _fixed_delta(fixed_delta), _max_frame_delta(max_frame_delta)
        {
        }

        explicit fixed_timestep_accumulator(float fixed_delta, float max_frame_delta = 0.1F) noexcept
            : _fixed_delta(chrono::duration<double>{static_cast<double>(fixed_delta)}),
              _max_frame_delta(chrono::duration<double>{static_cast<double>(max_frame_delta)})
        {
        }

        auto accumulate(chrono::duration<double> delta) noexcept -> void
        {
            if (_time_scale <= 0.0 || delta.count() <= 0.0)
            {
                return;
            }

            const auto scaled_delta = delta * _time_scale;
            const auto clamped_delta = (scaled_delta < _max_frame_delta) ? scaled_delta : _max_frame_delta;
            _accumulated_time += clamped_delta;
        }

        auto accumulate(float delta_seconds) noexcept -> void
        {
            accumulate(chrono::duration<double>{static_cast<double>(delta_seconds)});
        }

        [[nodiscard]] auto has_pending_ticks() const noexcept -> bool
        {
            return _accumulated_time >= _fixed_delta;
        }

        auto consume_tick() noexcept -> void
        {
            if (_accumulated_time >= _fixed_delta)
            {
                _accumulated_time -= _fixed_delta;
                ++_total_ticks;
            }
        }

        [[nodiscard]] auto alpha() const noexcept -> float
        {
            if (_fixed_delta.count() <= 0.0)
            {
                return 0.0F;
            }

            const auto raw_alpha = static_cast<float>(_accumulated_time.count() / _fixed_delta.count());
            return tempest::clamp(raw_alpha, 0.0F, 1.0F);
        }

        auto reset() noexcept -> void
        {
            _accumulated_time = chrono::duration<double>::zero();
        }

        template <typename TickFn>
        auto step(chrono::duration<double> delta, TickFn&& tick_fn) -> void
        {
            accumulate(delta);
            while (has_pending_ticks())
            {
                if constexpr (requires { tick_fn(_fixed_delta); })
                {
                    tick_fn(_fixed_delta);
                }
                else
                {
                    tick_fn(static_cast<float>(_fixed_delta.count()));
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
            return _fixed_delta;
        }

        auto set_fixed_delta(chrono::duration<double> fixed_delta) noexcept -> void
        {
            _fixed_delta = fixed_delta;
        }

        auto set_fixed_delta(float fixed_delta) noexcept -> void
        {
            _fixed_delta = chrono::duration<double>{static_cast<double>(fixed_delta)};
        }

        [[nodiscard]] auto max_frame_delta() const noexcept -> chrono::duration<double>
        {
            return _max_frame_delta;
        }

        auto set_max_frame_delta(chrono::duration<double> max_delta) noexcept -> void
        {
            _max_frame_delta = max_delta;
        }

        auto set_max_frame_delta(float max_delta) noexcept -> void
        {
            _max_frame_delta = chrono::duration<double>{static_cast<double>(max_delta)};
        }

        [[nodiscard]] auto time_scale() const noexcept -> double
        {
            return _time_scale;
        }

        auto set_time_scale(double scale) noexcept -> void
        {
            _time_scale = scale;
        }

        [[nodiscard]] auto accumulated_time() const noexcept -> chrono::duration<double>
        {
            return _accumulated_time;
        }

        [[nodiscard]] auto total_ticks() const noexcept -> uint64_t
        {
            return _total_ticks;
        }

      private:
        chrono::duration<double> _fixed_delta = default_fixed_delta;
        chrono::duration<double> _max_frame_delta = default_max_frame_delta;
        double _time_scale = default_time_scale;
        chrono::duration<double> _accumulated_time = chrono::duration<double>::zero();
        uint64_t _total_ticks = 0;
    };
} // namespace tempest

#endif // tempest_engine_fixed_timestep_accumulator_hpp
