#ifndef tempest_core_chrono_hpp
#define tempest_core_chrono_hpp

#include <tempest/api.hpp>
#include <tempest/compare.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/ratio.hpp>
#include <tempest/type_traits.hpp>

namespace tempest::chrono
{
    template <typename Rep, typename Period = ratio<1>>
    class duration;

    template <typename Clock, typename Duration = Clock::duration>
    class time_point;

    template <typename ToDuration, typename Rep, typename Period>
    constexpr auto duration_cast(const duration<Rep, Period>& dur) noexcept -> ToDuration;

    namespace detail
    {
        template <typename T>
        struct is_duration : false_type
        {
        };

        template <typename Rep, typename Period>
        struct is_duration<duration<Rep, Period>> : true_type
        {
        };

        template <typename T>
        inline constexpr bool is_duration_v = is_duration<T>::value;

        constexpr intmax_t seconds_per_minute = 60LL;
        constexpr intmax_t seconds_per_hour = 3600LL;
        constexpr intmax_t seconds_per_day = 86400LL;
        constexpr intmax_t seconds_per_week = 604800LL;
        constexpr intmax_t seconds_per_year = 31556952LL;
    } // namespace detail
} // namespace tempest::chrono

namespace tempest
{
    namespace detail
    {
        template <typename Period1, typename Period2>
        struct common_period_helper
        {
            static constexpr intmax_t g_num = ratio_gcd(Period1::num, Period2::num);
            static constexpr intmax_t g_den = ratio_gcd(Period1::den, Period2::den);
            using type = ratio<g_num, (Period1::den / g_den) * Period2::den>;
        };
    } // namespace detail

    template <typename Rep1, typename Period1, typename Rep2, typename Period2>
    struct common_type<chrono::duration<Rep1, Period1>, chrono::duration<Rep2, Period2>>
    {
        using type =
            chrono::duration<common_type_t<Rep1, Rep2>, typename detail::common_period_helper<Period1, Period2>::type>;
    };

    template <typename Clock, typename Duration1, typename Duration2>
    struct common_type<chrono::time_point<Clock, Duration1>, chrono::time_point<Clock, Duration2>>
    {
        using type = chrono::time_point<Clock, common_type_t<Duration1, Duration2>>;
    };
} // namespace tempest

namespace tempest::chrono
{
    /// @brief Duration represents a span of time measured as a count of ticks of a time unit.
    /// @tparam Rep An arithmetic type representing the number of ticks.
    /// @tparam Period A ratio representing the tick period in seconds.
    template <typename Rep, typename Period>
    class duration
    {
      public:
        using rep = Rep;
        using period = Period::type;

        constexpr duration() = default;

        template <typename Rep2>
            requires(is_convertible_v<const Rep2&, rep> && (is_floating_point_v<rep> || !is_floating_point_v<Rep2>))
        constexpr explicit duration(const Rep2& ticks) noexcept : _ticks(static_cast<rep>(ticks))
        {
        }

        template <typename Rep2, typename Period2>
            requires(is_floating_point_v<rep> ||
                     (ratio_divide<Period2, period>::den == 1 && !is_floating_point_v<Rep2>))
        constexpr duration(const duration<Rep2, Period2>& other) noexcept
            : _ticks(duration_cast<duration>(other).count())
        {
        }

        [[nodiscard]] constexpr auto count() const noexcept -> rep
        {
            return _ticks;
        }

        static constexpr auto zero() noexcept -> duration
        {
            return duration(static_cast<rep>(0));
        }

        static constexpr auto min() noexcept -> duration
        {
            return duration(numeric_limits<rep>::lowest());
        }

        static constexpr auto max() noexcept -> duration
        {
            return duration(numeric_limits<rep>::max());
        }

        constexpr auto operator+() const noexcept -> duration
        {
            return *this;
        }

        constexpr auto operator-() const noexcept -> duration
        {
            return duration(-_ticks);
        }

        constexpr auto operator++() noexcept -> duration&
        {
            ++_ticks;
            return *this;
        }

        constexpr auto operator++(int) noexcept -> duration
        {
            return duration(_ticks++);
        }

        constexpr auto operator--() noexcept -> duration&
        {
            --_ticks;
            return *this;
        }

        constexpr auto operator--(int) noexcept -> duration
        {
            return duration(_ticks--);
        }

        constexpr auto operator+=(const duration& other) noexcept -> duration&
        {
            _ticks += other.count();
            return *this;
        }

        constexpr auto operator-=(const duration& other) noexcept -> duration&
        {
            _ticks -= other.count();
            return *this;
        }

        constexpr auto operator*=(const rep& rhs) noexcept -> duration&
        {
            _ticks *= rhs;
            return *this;
        }

        constexpr auto operator/=(const rep& rhs) noexcept -> duration&
        {
            _ticks /= rhs;
            return *this;
        }

        constexpr auto operator%=(const rep& rhs) noexcept -> duration&
        {
            _ticks %= rhs;
            return *this;
        }

        constexpr auto operator%=(const duration& rhs) noexcept -> duration&
        {
            _ticks %= rhs.count();
            return *this;
        }

      private:
        rep _ticks{};
    };

    // Duration type aliases
    using nanoseconds = duration<int64_t, nano>;
    using microseconds = duration<int64_t, micro>;
    using milliseconds = duration<int64_t, milli>;
    using seconds = duration<int64_t>;
    using minutes = duration<int64_t, ratio<detail::seconds_per_minute>>;
    using hours = duration<int64_t, ratio<detail::seconds_per_hour>>;
    using days = duration<int64_t, ratio<detail::seconds_per_day>>;
    using weeks = duration<int64_t, ratio<detail::seconds_per_week>>;
    using years = duration<int64_t, ratio<detail::seconds_per_year>>;

    // Duration cast
    template <typename ToDuration, typename Rep, typename Period>
    constexpr auto duration_cast(const duration<Rep, Period>& dur) noexcept -> ToDuration
    {
        using CF = ratio_divide<Period, typename ToDuration::period>;
        using CR = common_type_t<typename ToDuration::rep, Rep, intmax_t>;

        if constexpr (CF::num == 1 && CF::den == 1)
        {
            return ToDuration(static_cast<ToDuration::rep>(dur.count()));
        }
        else if constexpr (CF::num != 1 && CF::den == 1)
        {
            return ToDuration(static_cast<ToDuration::rep>(static_cast<CR>(dur.count()) * static_cast<CR>(CF::num)));
        }
        else if constexpr (CF::num == 1 && CF::den != 1)
        {
            return ToDuration(static_cast<ToDuration::rep>(static_cast<CR>(dur.count()) / static_cast<CR>(CF::den)));
        }
        else
        {
            return ToDuration(static_cast<ToDuration::rep>(static_cast<CR>(dur.count()) * static_cast<CR>(CF::num) /
                                                           static_cast<CR>(CF::den)));
        }
    }

    // Non-member duration comparisons
    template <typename Rep1, typename Period1, typename Rep2, typename Period2>
    constexpr auto operator==(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) noexcept -> bool
    {
        using CD = common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>;
        return CD(lhs).count() == CD(rhs).count();
    }

    template <typename Rep1, typename Period1, typename Rep2, typename Period2>
    constexpr auto operator<=>(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) noexcept
    {
        using CD = common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>;
        return CD(lhs).count() <=> CD(rhs).count();
    }

    // Non-member duration arithmetic
    template <typename Rep1, typename Period1, typename Rep2, typename Period2>
    constexpr auto operator+(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) noexcept
        -> common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>
    {
        using CD = common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>;
        return CD(CD(lhs).count() + CD(rhs).count());
    }

    template <typename Rep1, typename Period1, typename Rep2, typename Period2>
    constexpr auto operator-(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) noexcept
        -> common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>
    {
        using CD = common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>;
        return CD(CD(lhs).count() - CD(rhs).count());
    }

    template <typename Rep1, typename Period, typename Rep2>
        requires is_convertible_v<const Rep2&, common_type_t<Rep1, Rep2>>
    constexpr auto operator*(const duration<Rep1, Period>& dur, const Rep2& scalar) noexcept
        -> duration<common_type_t<Rep1, Rep2>, Period>
    {
        using CR = common_type_t<Rep1, Rep2>;
        return duration<CR, Period>(static_cast<CR>(dur.count()) * static_cast<CR>(scalar));
    }

    template <typename Rep1, typename Rep2, typename Period>
        requires is_convertible_v<const Rep1&, common_type_t<Rep1, Rep2>>
    constexpr auto operator*(const Rep1& scalar, const duration<Rep2, Period>& dur) noexcept
        -> duration<common_type_t<Rep1, Rep2>, Period>
    {
        return dur * scalar;
    }

    template <typename Rep1, typename Period, typename Rep2>
        requires(!detail::is_duration_v<Rep2> && is_convertible_v<const Rep2&, common_type_t<Rep1, Rep2>>)
    constexpr auto operator/(const duration<Rep1, Period>& dur, const Rep2& scalar) noexcept
        -> duration<common_type_t<Rep1, Rep2>, Period>
    {
        using CR = common_type_t<Rep1, Rep2>;
        return duration<CR, Period>(static_cast<CR>(dur.count()) / static_cast<CR>(scalar));
    }

    template <typename Rep1, typename Period1, typename Rep2, typename Period2>
    constexpr auto operator/(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) noexcept
        -> common_type_t<Rep1, Rep2>
    {
        using CD = common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>;
        return CD(lhs).count() / CD(rhs).count();
    }

    template <typename Rep1, typename Period, typename Rep2>
        requires(!detail::is_duration_v<Rep2> && is_convertible_v<const Rep2&, common_type_t<Rep1, Rep2>>)
    constexpr auto operator%(const duration<Rep1, Period>& dur, const Rep2& scalar) noexcept
        -> duration<common_type_t<Rep1, Rep2>, Period>
    {
        using CR = common_type_t<Rep1, Rep2>;
        return duration<CR, Period>(static_cast<CR>(dur.count()) % static_cast<CR>(scalar));
    }

    template <typename Rep1, typename Period1, typename Rep2, typename Period2>
    constexpr auto operator%(const duration<Rep1, Period1>& lhs, const duration<Rep2, Period2>& rhs) noexcept
        -> common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>
    {
        using CD = common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>;
        return CD(CD(lhs).count() % CD(rhs).count());
    }

    // Duration utilities: floor, ceil, round, abs
    template <typename ToDuration, typename Rep, typename Period>
    constexpr auto floor(const duration<Rep, Period>& dur) noexcept -> ToDuration
    {
        auto res = duration_cast<ToDuration>(dur);
        if (res > dur)
        {
            res = res - ToDuration(1);
        }
        return res;
    }

    template <typename ToDuration, typename Rep, typename Period>
    constexpr auto ceil(const duration<Rep, Period>& dur) noexcept -> ToDuration
    {
        auto res = duration_cast<ToDuration>(dur);
        if (res < dur)
        {
            res = res + ToDuration(1);
        }
        return res;
    }

    template <typename ToDuration, typename Rep, typename Period>
    constexpr auto round(const duration<Rep, Period>& dur) noexcept -> ToDuration
    {
        auto lower = floor<ToDuration>(dur);
        auto upper = lower + ToDuration(1);
        auto diff0 = dur - lower;
        auto diff1 = upper - dur;
        if (diff0 < diff1)
        {
            return lower;
        }
        if (diff1 < diff0)
        {
            return upper;
        }
        if constexpr (is_integral_v<typename ToDuration::rep>)
        {
            return (lower.count() % 2 != 0) ? upper : lower;
        }
        else
        {
            return lower;
        }
    }

    template <typename Rep, typename Period>
    constexpr auto abs(duration<Rep, Period> dur) noexcept -> duration<Rep, Period>
    {
        return dur >= dur.zero() ? dur : -dur;
    }

    /// @brief Represents a specific point in time tied to a Clock.
    template <typename Clock, typename Duration>
    class time_point
    {
      public:
        using clock = Clock;
        using duration = Duration;
        using rep = Duration::rep;
        using period = Duration::period;

        constexpr time_point() = default;

        constexpr explicit time_point(const duration& dur) noexcept : _dur(dur)
        {
        }

        template <typename Duration2>
            requires is_convertible_v<Duration2, duration>
        constexpr time_point(const time_point<Clock, Duration2>& other) noexcept : _dur(other.time_since_epoch())
        {
        }

        [[nodiscard]] constexpr auto time_since_epoch() const noexcept -> duration
        {
            return _dur;
        }

        static constexpr auto min() noexcept -> time_point
        {
            return time_point(duration::min());
        }

        static constexpr auto max() noexcept -> time_point
        {
            return time_point(duration::max());
        }

        constexpr auto operator+=(const duration& dur) noexcept -> time_point&
        {
            _dur += dur;
            return *this;
        }

        constexpr auto operator-=(const duration& dur) noexcept -> time_point&
        {
            _dur -= dur;
            return *this;
        }

      private:
        duration _dur{};
    };

    // time_point_cast
    template <typename ToDuration, typename Clock, typename Duration>
    constexpr auto time_point_cast(const time_point<Clock, Duration>& timepoint) noexcept
        -> time_point<Clock, ToDuration>
    {
        return time_point<Clock, ToDuration>(duration_cast<ToDuration>(timepoint.time_since_epoch()));
    }

    template <typename ToDuration, typename Clock, typename Duration>
    constexpr auto floor(const time_point<Clock, Duration>& timepoint) noexcept -> time_point<Clock, ToDuration>
    {
        return time_point<Clock, ToDuration>(floor<ToDuration>(timepoint.time_since_epoch()));
    }

    template <typename ToDuration, typename Clock, typename Duration>
    constexpr auto ceil(const time_point<Clock, Duration>& timepoint) noexcept -> time_point<Clock, ToDuration>
    {
        return time_point<Clock, ToDuration>(ceil<ToDuration>(timepoint.time_since_epoch()));
    }

    template <typename ToDuration, typename Clock, typename Duration>
    constexpr auto round(const time_point<Clock, Duration>& timepoint) noexcept -> time_point<Clock, ToDuration>
    {
        return time_point<Clock, ToDuration>(round<ToDuration>(timepoint.time_since_epoch()));
    }

    // time_point comparisons
    template <typename Clock, typename Duration1, typename Duration2>
    constexpr auto operator==(const time_point<Clock, Duration1>& lhs, const time_point<Clock, Duration2>& rhs) noexcept
        -> bool
    {
        return lhs.time_since_epoch() == rhs.time_since_epoch();
    }

    template <typename Clock, typename Duration1, typename Duration2>
    constexpr auto operator<=>(const time_point<Clock, Duration1>& lhs,
                               const time_point<Clock, Duration2>& rhs) noexcept
    {
        return lhs.time_since_epoch() <=> rhs.time_since_epoch();
    }

    // time_point arithmetic
    template <typename Clock, typename Duration1, typename Rep2, typename Period2>
    constexpr auto operator+(const time_point<Clock, Duration1>& timepoint, const duration<Rep2, Period2>& dur) noexcept
        -> time_point<Clock, common_type_t<Duration1, duration<Rep2, Period2>>>
    {
        using CD = common_type_t<Duration1, duration<Rep2, Period2>>;
        return time_point<Clock, CD>(timepoint.time_since_epoch() + dur);
    }

    template <typename Rep1, typename Period1, typename Clock, typename Duration2>
    constexpr auto operator+(const duration<Rep1, Period1>& dur, const time_point<Clock, Duration2>& timepoint) noexcept
        -> time_point<Clock, common_type_t<duration<Rep1, Period1>, Duration2>>
    {
        return timepoint + dur;
    }

    template <typename Clock, typename Duration1, typename Rep2, typename Period2>
    constexpr auto operator-(const time_point<Clock, Duration1>& timepoint, const duration<Rep2, Period2>& dur) noexcept
        -> time_point<Clock, common_type_t<Duration1, duration<Rep2, Period2>>>
    {
        using CD = common_type_t<Duration1, duration<Rep2, Period2>>;
        return time_point<Clock, CD>(timepoint.time_since_epoch() - dur);
    }

    template <typename Clock, typename Duration1, typename Duration2>
    constexpr auto operator-(const time_point<Clock, Duration1>& lhs, const time_point<Clock, Duration2>& rhs) noexcept
        -> common_type_t<Duration1, Duration2>
    {
        return lhs.time_since_epoch() - rhs.time_since_epoch();
    }

    // Clocks

    /// @brief Monotonic clock whose value cannot decrease as physical time moves forward.
    struct steady_clock
    {
        using rep = int64_t;
        using period = nano;
        using duration = nanoseconds;
        using time_point = chrono::time_point<steady_clock, duration>;
        static constexpr bool is_steady = true;

        static TEMPEST_API auto now() noexcept -> time_point;
    };

    /// @brief System-wide real time wall clock.
    struct system_clock
    {
        using rep = int64_t;
        using period = nano;
        using duration = nanoseconds;
        using time_point = chrono::time_point<system_clock, duration>;
        static constexpr bool is_steady = false;

        using time_t = int64_t;

        static TEMPEST_API auto now() noexcept -> time_point;

        static constexpr auto to_time_t(const time_point& timepoint) noexcept -> time_t
        {
            return duration_cast<seconds>(timepoint.time_since_epoch()).count();
        }

        static constexpr auto from_time_t(time_t time_sec) noexcept -> time_point
        {
            return time_point(duration_cast<duration>(seconds(time_sec)));
        }
    };

    using high_resolution_clock = steady_clock;

} // namespace tempest::chrono

namespace tempest
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuser-defined-literals"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wliteral-suffix"
#endif

    inline namespace chrono_literals
    {
        constexpr auto operator""ns(unsigned long long val) noexcept -> chrono::nanoseconds
        {
            return chrono::nanoseconds(static_cast<int64_t>(val));
        }

        constexpr auto operator""us(unsigned long long val) noexcept -> chrono::microseconds
        {
            return chrono::microseconds(static_cast<int64_t>(val));
        }

        constexpr auto operator""ms(unsigned long long val) noexcept -> chrono::milliseconds
        {
            return chrono::milliseconds(static_cast<int64_t>(val));
        }

        constexpr auto operator""s(unsigned long long val) noexcept -> chrono::seconds
        {
            return chrono::seconds(static_cast<int64_t>(val));
        }

        constexpr auto operator""min(unsigned long long val) noexcept -> chrono::minutes
        {
            return chrono::minutes(static_cast<int64_t>(val));
        }

        constexpr auto operator""h(unsigned long long val) noexcept -> chrono::hours
        {
            return chrono::hours(static_cast<int64_t>(val));
        }

        constexpr auto operator""ns(long double val) noexcept -> chrono::duration<long double, nano>
        {
            return chrono::duration<long double, nano>(val);
        }

        constexpr auto operator""us(long double val) noexcept -> chrono::duration<long double, micro>
        {
            return chrono::duration<long double, micro>(val);
        }

        constexpr auto operator""ms(long double val) noexcept -> chrono::duration<long double, milli>
        {
            return chrono::duration<long double, milli>(val);
        }

        constexpr auto operator""s(long double val) noexcept -> chrono::duration<long double>
        {
            return chrono::duration<long double>(val);
        }

        constexpr auto operator""min(long double val) noexcept
            -> chrono::duration<long double, ratio<chrono::detail::seconds_per_minute>>
        {
            return chrono::duration<long double, ratio<chrono::detail::seconds_per_minute>>(val);
        }

        constexpr auto operator""h(long double val) noexcept
            -> chrono::duration<long double, ratio<chrono::detail::seconds_per_hour>>
        {
            return chrono::duration<long double, ratio<chrono::detail::seconds_per_hour>>(val);
        }
    } // namespace chrono_literals

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    namespace literals
    {
        using namespace chrono_literals;
    } // namespace literals

    namespace chrono
    {
        using namespace chrono_literals;
    } // namespace chrono
} // namespace tempest

#endif // tempest_core_chrono_hpp
