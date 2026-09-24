#ifndef tempest_engine_fixed_timestep_accumulator_hpp
#define tempest_engine_fixed_timestep_accumulator_hpp

#include <tempest/algorithm.hpp>
#include <tempest/int.hpp>

namespace tempest
{
    class fixed_timestep_accumulator
    {
      public:
        explicit fixed_timestep_accumulator(float fixed_delta = 1.0F / 60.0F, float max_frame_delta = 0.1F) noexcept
            : _fixed_delta{fixed_delta}, _max_frame_delta{max_frame_delta}
        {
        }

        auto accumulate(float delta_seconds) noexcept -> void
        {
            if (_time_scale <= 0.0F || delta_seconds <= 0.0F)
            {
                return;
            }

            const auto scaled_delta = delta_seconds * _time_scale;
            const auto clamped_delta = tempest::min(scaled_delta, _max_frame_delta);
            _accumulated_time += clamped_delta;
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
            if (_fixed_delta <= 0.0F)
            {
                return 0.0F;
            }

            const auto raw_alpha = _accumulated_time / _fixed_delta;
            return tempest::clamp(raw_alpha, 0.0F, 1.0F);
        }

        auto reset() noexcept -> void
        {
            _accumulated_time = 0.0F;
        }

        template <typename TickFn>
        auto step(float delta_seconds, TickFn&& tick_fn) -> void
        {
            accumulate(delta_seconds);
            while (has_pending_ticks())
            {
                tick_fn(_fixed_delta);
                consume_tick();
            }
        }

        [[nodiscard]] auto fixed_delta() const noexcept -> float
        {
            return _fixed_delta;
        }

        auto set_fixed_delta(float fixed_delta) noexcept -> void
        {
            _fixed_delta = fixed_delta;
        }

        [[nodiscard]] auto max_frame_delta() const noexcept -> float
        {
            return _max_frame_delta;
        }

        auto set_max_frame_delta(float max_delta) noexcept -> void
        {
            _max_frame_delta = max_delta;
        }

        [[nodiscard]] auto time_scale() const noexcept -> float
        {
            return _time_scale;
        }

        auto set_time_scale(float scale) noexcept -> void
        {
            _time_scale = scale;
        }

        [[nodiscard]] auto accumulated_time() const noexcept -> float
        {
            return _accumulated_time;
        }

        [[nodiscard]] auto total_ticks() const noexcept -> uint64_t
        {
            return _total_ticks;
        }

      private:
        float _fixed_delta{1.0F / 60.0F};
        float _max_frame_delta{0.1F};
        float _time_scale{1.0F};
        float _accumulated_time{0.0F};
        uint64_t _total_ticks{0};
    };
} // namespace tempest

#endif // tempest_engine_fixed_timestep_accumulator_hpp
