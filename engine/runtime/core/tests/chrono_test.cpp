#include <gtest/gtest.h>

#include <tempest/chrono.hpp>
#include <tempest/ratio.hpp>

//==============================================================================
// Ratio Tests
//==============================================================================

/// @brief Tests compile-time canonical reduction and sign normalization of tempest::ratio.
TEST(ratio_test, canonical_reduction_and_signs)
{
    // 1. Setup & Assert: verify canonical gcd reduction
    static_assert(tempest::ratio<2, 4>::num == 1);
    static_assert(tempest::ratio<2, 4>::den == 2);
    static_assert(tempest::is_same_v<tempest::ratio<2, 4>::type, tempest::ratio<1, 2>>);

    // 2. Setup & Assert: verify negative denominator and negative numerator sign flips
    static_assert(tempest::ratio<1, -2>::num == -1);
    static_assert(tempest::ratio<1, -2>::den == 2);
    static_assert(tempest::ratio<-1, -2>::num == 1);
    static_assert(tempest::ratio<-1, -2>::den == 2);
    static_assert(tempest::ratio<-2, -4>::num == 1);
    static_assert(tempest::ratio<-2, -4>::den == 2);
    static_assert(tempest::ratio<2, -4>::num == -1);
    static_assert(tempest::ratio<2, -4>::den == 2);
    static_assert(tempest::ratio<-2, 4>::num == -1);
    static_assert(tempest::ratio<-2, 4>::den == 2);

    // 3. Setup & Assert: zero numerator with non-zero denominator
    constexpr auto zero_ratio_denom = 5;
    static_assert(tempest::ratio<0, zero_ratio_denom>::num == 0);
    static_assert(tempest::ratio<0, zero_ratio_denom>::den == 1);

    // 4. Runtime assertion verifying values match expectations
    constexpr auto test_numerator = 4;
    constexpr auto test_denominator = 8;
    EXPECT_EQ((tempest::ratio<test_numerator, test_denominator>::num), 1);
    EXPECT_EQ((tempest::ratio<test_numerator, test_denominator>::den), 2);
}

/// @brief Tests compile-time rational addition and subtraction.
TEST(ratio_test, addition_and_subtraction)
{
    // 1. Setup: Define ratio operand components
    constexpr auto denom_three = 3;
    constexpr auto denom_six = 6;
    using r1 = tempest::ratio<1, denom_three>;
    using r2 = tempest::ratio<1, denom_six>;

    // 2. Act & Assert: Addition 1/3 + 1/6 = 1/2
    using r_sum = tempest::ratio_add<r1, r2>;
    static_assert(r_sum::num == 1 && r_sum::den == 2);
    EXPECT_EQ(r_sum::num, 1);
    EXPECT_EQ(r_sum::den, 2);

    // 3. Act & Assert: Subtraction 1/3 - 1/6 = 1/6
    using r_diff = tempest::ratio_subtract<r1, r2>;
    static_assert(r_diff::num == 1 && r_diff::den == denom_six);
    EXPECT_EQ(r_diff::num, 1);
    EXPECT_EQ(r_diff::den, denom_six);
}

/// @brief Tests compile-time rational multiplication and division.
TEST(ratio_test, multiplication_and_division)
{
    // 1. Setup: Define ratio operand components
    constexpr auto denom_three = 3;
    constexpr auto denom_six = 6;
    constexpr auto denom_eighteen = 18;
    using r1 = tempest::ratio<1, denom_three>;
    using r2 = tempest::ratio<1, denom_six>;

    // 2. Act & Assert: Multiplication 1/3 * 1/6 = 1/18
    using r_prod = tempest::ratio_multiply<r1, r2>;
    static_assert(r_prod::num == 1 && r_prod::den == denom_eighteen);
    EXPECT_EQ(r_prod::num, 1);
    EXPECT_EQ(r_prod::den, denom_eighteen);

    // 3. Act & Assert: Division (1/3) / (1/6) = 2/1
    using r_quot = tempest::ratio_divide<r1, r2>;
    static_assert(r_quot::num == 2 && r_quot::den == 1);
    EXPECT_EQ(r_quot::num, 2);
    EXPECT_EQ(r_quot::den, 1);
}

/// @brief Tests ratio compile-time comparison operators including negative operands.
TEST(ratio_test, comparisons)
{
    // 1. Setup: define comparison operands
    constexpr auto denom_three = 3;
    constexpr auto denom_four = 4;
    constexpr auto denom_eight = 8;
    using r1 = tempest::ratio<1, denom_four>;
    using r2 = tempest::ratio<2, denom_eight>;
    using r3 = tempest::ratio<1, 2>;
    using r_neg1 = tempest::ratio<-1, 2>;
    using r_neg2 = tempest::ratio<-1, denom_three>;

    // 2. Act & Assert: equality and inequality
    static_assert(tempest::ratio_equal_v<r1, r2>);
    static_assert(!tempest::ratio_not_equal_v<r1, r2>);
    static_assert(tempest::ratio_not_equal_v<r1, r3>);

    // 3. Act & Assert: ordered relations with positive numbers
    static_assert(tempest::ratio_less_v<r1, r3>);
    static_assert(tempest::ratio_less_equal_v<r1, r2>);
    static_assert(tempest::ratio_greater_v<r3, r1>);
    static_assert(tempest::ratio_greater_equal_v<r2, r1>);

    // 4. Act & Assert: ordered relations with negative numbers
    static_assert(tempest::ratio_less_v<r_neg1, r1>);
    static_assert(tempest::ratio_less_v<r_neg1, r_neg2>);
    static_assert(tempest::ratio_greater_v<r_neg2, r_neg1>);

    EXPECT_TRUE((tempest::ratio_equal_v<r1, r2>));
    EXPECT_TRUE((tempest::ratio_less_v<r1, r3>));
    EXPECT_TRUE((tempest::ratio_less_v<r_neg1, r_neg2>));
}

/// @brief Tests SI prefix ratio constants.
TEST(ratio_test, si_prefixes)
{
    // 1. Setup: named constants for expected standard SI ratios
    constexpr auto expected_atto_den = 1'000'000'000'000'000'000LL;
    constexpr auto expected_nano_den = 1'000'000'000LL;
    constexpr auto expected_micro_den = 1'000'000LL;
    constexpr auto expected_milli_den = 1'000LL;
    constexpr auto expected_centi_den = 100LL;
    constexpr auto expected_deci_den = 10LL;
    constexpr auto expected_deca_num = 10LL;
    constexpr auto expected_hecto_num = 100LL;
    constexpr auto expected_kilo_num = 1'000LL;
    constexpr auto expected_mega_num = 1'000'000LL;
    constexpr auto expected_giga_num = 1'000'000'000LL;
    constexpr auto expected_tera_num = 1'000'000'000'000LL;
    constexpr auto expected_exa_num = 1'000'000'000'000'000'000LL;

    // 2. Assert: verify standard SI ratio numerator and denominator
    static_assert(tempest::atto::num == 1 && tempest::atto::den == expected_atto_den);
    static_assert(tempest::nano::num == 1 && tempest::nano::den == expected_nano_den);
    static_assert(tempest::micro::num == 1 && tempest::micro::den == expected_micro_den);
    static_assert(tempest::milli::num == 1 && tempest::milli::den == expected_milli_den);
    static_assert(tempest::centi::num == 1 && tempest::centi::den == expected_centi_den);
    static_assert(tempest::deci::num == 1 && tempest::deci::den == expected_deci_den);
    static_assert(tempest::deca::num == expected_deca_num && tempest::deca::den == 1LL);
    static_assert(tempest::hecto::num == expected_hecto_num && tempest::hecto::den == 1LL);
    static_assert(tempest::kilo::num == expected_kilo_num && tempest::kilo::den == 1LL);
    static_assert(tempest::mega::num == expected_mega_num && tempest::mega::den == 1LL);
    static_assert(tempest::giga::num == expected_giga_num && tempest::giga::den == 1LL);
    static_assert(tempest::tera::num == expected_tera_num && tempest::tera::den == 1LL);
    static_assert(tempest::exa::num == expected_exa_num && tempest::exa::den == 1LL);

    EXPECT_EQ(tempest::milli::den, expected_milli_den);
    EXPECT_EQ(tempest::kilo::num, expected_kilo_num);
}

//==============================================================================
// Duration Tests
//==============================================================================

/// @brief Tests duration construction, count(), and boundary limits.
TEST(chrono_duration_test, construction_and_limits)
{
    // 1. Setup: create duration instances
    constexpr auto initial_five_seconds = 5;
    auto dur_five = tempest::chrono::seconds(initial_five_seconds);
    auto dur_zero = tempest::chrono::seconds::zero();
    auto dur_min = tempest::chrono::seconds::min();
    auto dur_max = tempest::chrono::seconds::max();

    // 2. Assert: count and limit values
    EXPECT_EQ(dur_five.count(), initial_five_seconds);
    EXPECT_EQ(dur_zero.count(), 0);
    EXPECT_EQ(dur_min.count(), tempest::numeric_limits<tempest::int64_t>::lowest());
    EXPECT_EQ(dur_max.count(), tempest::numeric_limits<tempest::int64_t>::max());
}

/// @brief Tests duration arithmetic operators including +, -, *, /, %, ++, --, +=, -=.
TEST(chrono_duration_test, arithmetic_operators)
{
    // 1. Setup: operands and named constants
    constexpr auto ten_seconds = 10;
    constexpr auto three_seconds = 3;
    constexpr auto expected_sum = 13;
    constexpr auto expected_diff = 7;
    constexpr auto expected_neg = -10;
    constexpr auto expected_mul = 20;
    constexpr auto expected_mul_lhs = 30;
    constexpr auto expected_div = 5;
    constexpr auto expected_add_result = 6;
    constexpr auto expected_sub_result = 4;
    constexpr auto expected_mul_result = 12;

    auto dur_a = tempest::chrono::seconds(ten_seconds);
    auto dur_b = tempest::chrono::seconds(three_seconds);

    // 2. Act: basic addition and subtraction
    auto sum = dur_a + dur_b;
    auto diff = dur_a - dur_b;
    auto neg = -dur_a;

    // 3. Assert: arithmetic values
    EXPECT_EQ(sum.count(), expected_sum);
    EXPECT_EQ(diff.count(), expected_diff);
    EXPECT_EQ(neg.count(), expected_neg);

    // 4. Act & Assert: scalar multiplication and division
    auto mul = dur_a * 2;
    auto mul_lhs = 3 * dur_a;
    auto div = dur_a / 2;
    auto div_dur = dur_a / dur_b;
    auto rem = dur_a % dur_b;

    EXPECT_EQ(mul.count(), expected_mul);
    EXPECT_EQ(mul_lhs.count(), expected_mul_lhs);
    EXPECT_EQ(div.count(), expected_div);
    EXPECT_EQ(div_dur, three_seconds);
    EXPECT_EQ(rem.count(), 1);

    // 5. Act & Assert: increment / decrement
    auto dur_c = tempest::chrono::seconds(1);
    EXPECT_EQ((++dur_c).count(), 2);
    EXPECT_EQ((dur_c++).count(), 2);
    EXPECT_EQ(dur_c.count(), 3);
    EXPECT_EQ((--dur_c).count(), 2);
    EXPECT_EQ((dur_c--).count(), 2);
    EXPECT_EQ(dur_c.count(), 1);

    // 6. Act & Assert: compound assignments
    constexpr auto five_seconds = 5;
    dur_c += tempest::chrono::seconds(five_seconds);
    EXPECT_EQ(dur_c.count(), expected_add_result);
    dur_c -= tempest::chrono::seconds(2);
    EXPECT_EQ(dur_c.count(), expected_sub_result);
    dur_c *= 3;
    EXPECT_EQ(dur_c.count(), expected_mul_result);
    dur_c /= 2;
    EXPECT_EQ(dur_c.count(), expected_add_result);
    dur_c %= expected_sub_result;
    EXPECT_EQ(dur_c.count(), 2);
}

/// @brief Tests duration comparisons: ==, !=, <, <=, >, >=.
TEST(chrono_duration_test, comparisons)
{
    // 1. Setup: durations with different periods
    constexpr auto thousand_ms = 1000;
    constexpr auto five_hundred_ms = 500;
    auto sec1 = tempest::chrono::seconds(1);
    auto ms1000 = tempest::chrono::milliseconds(thousand_ms);
    auto ms500 = tempest::chrono::milliseconds(five_hundred_ms);
    auto sec2 = tempest::chrono::seconds(2);

    // 2. Assert: cross-period equality
    EXPECT_TRUE(sec1 == ms1000);
    EXPECT_FALSE(sec1 != ms1000);
    EXPECT_FALSE(sec1 == ms500);

    // 3. Assert: ordered comparisons
    EXPECT_TRUE(ms500 < sec1);
    EXPECT_TRUE(ms500 <= sec1);
    EXPECT_TRUE(sec2 > sec1);
    EXPECT_TRUE(sec2 >= sec1);
    EXPECT_TRUE(sec1 >= ms1000);
    EXPECT_TRUE(sec1 <= ms1000);
}

/// @brief Tests negative duration arithmetic, comparisons, and modulo edge cases.
TEST(chrono_duration_test, negative_durations)
{
    using namespace tempest::chrono;

    // 1. Setup: negative operands and constants
    constexpr auto neg_five_hundred_ms = -500;
    constexpr auto pos_five_hundred_ms = 500;
    constexpr auto neg_seven_seconds = -7;
    constexpr auto pos_three_seconds = 3;
    constexpr auto neg_three_seconds = -3;
    constexpr auto pos_seven_seconds = 7;

    auto dur_neg500ms = milliseconds(neg_five_hundred_ms);
    auto dur_pos1s = seconds(1);

    // 2. Act & Assert: addition with negative
    auto res = dur_neg500ms + dur_pos1s;
    EXPECT_EQ(res.count(), pos_five_hundred_ms);

    // 3. Act & Assert: negative comparisons
    EXPECT_TRUE(dur_neg500ms < milliseconds::zero());
    EXPECT_TRUE(seconds(-2) < seconds(-1));
    EXPECT_TRUE(seconds(-1) > seconds(-2));

    // 4. Act & Assert: modulo with negative durations
    auto dur_neg7s = seconds(neg_seven_seconds);
    auto dur_pos3s = seconds(pos_three_seconds);
    auto dur_neg3s = seconds(neg_three_seconds);

    EXPECT_EQ((dur_neg7s % dur_pos3s).count(), -1);
    EXPECT_EQ((dur_neg7s % dur_neg3s).count(), -1);
    EXPECT_EQ((seconds(pos_seven_seconds) % dur_neg3s).count(), 1);
}

/// @brief Tests duration_cast precision across integer ratios and floating-point conversions.
TEST(chrono_duration_test, duration_cast)
{
    using namespace tempest::chrono;

    // 1. Setup & Act: nanoseconds to milliseconds conversion (1,000,000ns == 1ms)
    constexpr auto one_million = 1'000'000;
    constexpr auto two_billion = 2'000'000'000LL;
    constexpr auto fifteen_hundred = 1500;
    constexpr auto neg_fifteen_hundred = -1500;
    constexpr auto float_one_point_five = 1.5F;
    constexpr auto float_two_point_five = 2.5F;
    constexpr auto double_twenty_five_hundred = 2500.0;

    auto nanos = nanoseconds(one_million);
    auto millis = duration_cast<milliseconds>(nanos);

    // 2. Assert: exact integer ratio conversion
    EXPECT_EQ(millis.count(), 1);

    // 3. Act & Assert: seconds to nanoseconds
    auto secs = seconds(2);
    auto ns_from_s = duration_cast<nanoseconds>(secs);
    EXPECT_EQ(ns_from_s.count(), two_billion);

    // 4. Act & Assert: integer truncation towards zero
    auto ms_trunc = duration_cast<seconds>(milliseconds(fifteen_hundred));
    EXPECT_EQ(ms_trunc.count(), 1);

    auto neg_ms_trunc = duration_cast<seconds>(milliseconds(neg_fifteen_hundred));
    EXPECT_EQ(neg_ms_trunc.count(), -1);

    // 5. Act & Assert: floating point duration cast
    auto float_sec = duration_cast<duration<float>>(milliseconds(fifteen_hundred));
    EXPECT_FLOAT_EQ(float_sec.count(), float_one_point_five);

    auto float_ms = duration_cast<duration<double, tempest::milli>>(duration<float>(float_two_point_five));
    EXPECT_DOUBLE_EQ(float_ms.count(), double_twenty_five_hundred);
}

/// @brief Tests duration utility functions: abs, floor, ceil, round.
TEST(chrono_duration_test, utilities_floor_ceil_round_abs)
{
    using namespace tempest::chrono;

    constexpr auto neg_five_hundred = -500;
    constexpr auto pos_five_hundred = 500;
    constexpr auto pos_three_hundred = 300;
    constexpr auto val_nineteen_hundred = 1900;
    constexpr auto val_neg_nineteen_hundred = -1900;
    constexpr auto val_eleven_hundred = 1100;
    constexpr auto val_neg_eleven_hundred = -1100;
    constexpr auto val_fourteen_hundred = 1400;
    constexpr auto val_sixteen_hundred = 1600;
    constexpr auto val_fifteen_hundred = 1500;
    constexpr auto val_twenty_five_hundred = 2500;
    constexpr auto val_neg_fifteen_hundred = -1500;
    constexpr auto float_one_point_five = 1.5F;

    // 1. Act & Assert: abs()
    auto neg = milliseconds(neg_five_hundred);
    EXPECT_EQ(abs(neg).count(), pos_five_hundred);
    EXPECT_EQ(abs(milliseconds(pos_three_hundred)).count(), pos_three_hundred);

    // 2. Act & Assert: floor() truncates downwards
    EXPECT_EQ(floor<seconds>(milliseconds(val_nineteen_hundred)).count(), 1);
    EXPECT_EQ(floor<seconds>(milliseconds(val_neg_eleven_hundred)).count(), -2);

    // 3. Act & Assert: ceil() rounds upwards
    EXPECT_EQ(ceil<seconds>(milliseconds(val_eleven_hundred)).count(), 2);
    EXPECT_EQ(ceil<seconds>(milliseconds(val_neg_nineteen_hundred)).count(), -1);

    // 4. Act & Assert: round() rounds to nearest, tie to even
    EXPECT_EQ(round<seconds>(milliseconds(val_fourteen_hundred)).count(), 1);
    EXPECT_EQ(round<seconds>(milliseconds(val_sixteen_hundred)).count(), 2);
    EXPECT_EQ(round<seconds>(milliseconds(val_fifteen_hundred)).count(), 2);      // 2 is even
    EXPECT_EQ(round<seconds>(milliseconds(val_twenty_five_hundred)).count(), 2);  // 2 is even
    EXPECT_EQ(round<seconds>(milliseconds(val_neg_fifteen_hundred)).count(), -2); // -2 is even

    // 5. Act & Assert: round() with floating-point duration rep
    auto f_rounded = round<duration<float>>(milliseconds(val_fifteen_hundred));
    EXPECT_FLOAT_EQ(f_rounded.count(), float_one_point_five);
}

/// @brief Tests mixed arithmetic between floating-point and integral durations.
TEST(chrono_duration_test, mixed_floating_point_arithmetic)
{
    using namespace tempest::chrono;

    // 1. Setup: float and integral durations
    constexpr auto float_half = 0.5F;
    constexpr auto val_five_hundred = 500;
    constexpr auto float_thousand = 1000.0F;

    auto f_sec = duration<float>(float_half);
    auto millis = milliseconds(val_five_hundred);

    // 2. Act: addition
    auto sum = f_sec + millis;

    // 3. Assert: common duration period is milliseconds, count is 1000ms, and value equals 1 second
    EXPECT_FLOAT_EQ(sum.count(), float_thousand);
    EXPECT_EQ(sum, seconds(1));
    EXPECT_FLOAT_EQ(duration_cast<duration<float>>(sum).count(), 1.0F);
}

//==============================================================================
// Time Point Tests
//==============================================================================

/// @brief Tests time_point epoch, arithmetic with durations, and differences.
TEST(chrono_time_point_test, arithmetic_and_difference)
{
    using clock = tempest::chrono::steady_clock;
    using time_point = clock::time_point;
    using namespace tempest::chrono;

    constexpr auto sec_ten = 10;
    constexpr auto sec_five = 5;
    constexpr auto sec_fifteen = 15;
    constexpr auto sec_three = 3;
    constexpr auto sec_twelve = 12;
    constexpr auto sec_eight = 8;
    constexpr auto sec_two = 2;
    constexpr auto sec_four = 4;

    // 1. Setup: default time_point at epoch (0)
    auto tp0 = time_point();
    EXPECT_EQ(tp0.time_since_epoch().count(), 0);

    // 2. Act: time_point + duration and duration + time_point
    auto tp1 = tp0 + seconds(sec_ten);
    auto tp2 = seconds(sec_five) + tp1;

    // 3. Assert: time since epoch advances
    EXPECT_EQ(duration_cast<seconds>(tp1.time_since_epoch()).count(), sec_ten);
    EXPECT_EQ(duration_cast<seconds>(tp2.time_since_epoch()).count(), sec_fifteen);

    // 4. Act: time_point - duration
    auto tp3 = tp2 - seconds(sec_three);
    EXPECT_EQ(duration_cast<seconds>(tp3.time_since_epoch()).count(), sec_twelve);

    // 5. Act: time_point - time_point
    auto dur = tp2 - tp1;
    EXPECT_EQ(duration_cast<seconds>(dur).count(), sec_five);

    // 6. Act: compound operators
    tp1 += seconds(sec_two);
    EXPECT_EQ(duration_cast<seconds>(tp1.time_since_epoch()).count(), sec_twelve);
    tp1 -= seconds(sec_four);
    EXPECT_EQ(duration_cast<seconds>(tp1.time_since_epoch()).count(), sec_eight);

    // 7. Assert: time_point comparisons
    EXPECT_TRUE(tp1 < tp2);
    EXPECT_TRUE(tp2 > tp1);
    EXPECT_TRUE(tp1 == (tp0 + seconds(sec_eight)));
}

/// @brief Tests time_point_cast, floor, ceil, and round on time points.
TEST(chrono_time_point_test, time_point_cast_and_rounding)
{
    using clock = tempest::chrono::steady_clock;
    using namespace tempest::chrono;

    // 1. Setup: time_point in nanoseconds
    constexpr auto test_timestamp_ns = 3'500'000'000LL;
    constexpr auto expected_sec_three = 3;
    constexpr auto expected_sec_four = 4;
    auto tp_ns = clock::time_point(nanoseconds(test_timestamp_ns));

    // 2. Act & Assert: time_point_cast truncated to 3 seconds
    auto tp_s = time_point_cast<seconds>(tp_ns);
    EXPECT_EQ(tp_s.time_since_epoch().count(), expected_sec_three);

    // 3. Act & Assert: floor, ceil, and round for time points
    auto tp_floor = floor<seconds>(tp_ns);
    EXPECT_EQ(tp_floor.time_since_epoch().count(), expected_sec_three);

    auto tp_ceil = ceil<seconds>(tp_ns);
    EXPECT_EQ(tp_ceil.time_since_epoch().count(), expected_sec_four);

    auto tp_round = round<seconds>(tp_ns);
    EXPECT_EQ(tp_round.time_since_epoch().count(), expected_sec_four); // 3.5s ties to 4 (even)
}

/// @brief Tests time_point representation prior to epoch (negative time points).
TEST(chrono_time_point_test, negative_time_points)
{
    using clock = tempest::chrono::steady_clock;
    using namespace tempest::chrono;

    // 1. Setup: time_point 5 seconds before epoch
    constexpr auto sec_five = 5;
    constexpr auto sec_neg_five = -5;
    auto tp_epoch = clock::time_point();
    auto tp_before = tp_epoch - seconds(sec_five);

    // 2. Assert: comparisons and arithmetic
    EXPECT_LT(tp_before, tp_epoch);
    EXPECT_EQ((tp_epoch - tp_before).count(), nanoseconds(seconds(sec_five)).count());
    EXPECT_EQ(tp_before.time_since_epoch().count(), nanoseconds(seconds(sec_neg_five)).count());
}

//==============================================================================
// Clock Tests
//==============================================================================

/// @brief Tests steady_clock monotonicity: successive readings never decrease.
TEST(chrono_clock_test, steady_clock_monotonicity)
{
    using clock = tempest::chrono::steady_clock;

    // 1. Act: sample clock multiple times in loop
    constexpr auto iteration_count = 1000;
    auto prev = clock::now();
    for (auto i = 0; i < iteration_count; ++i)
    {
        auto curr = clock::now();

        // 2. Assert: monotonic advancement (curr >= prev)
        EXPECT_GE(curr.time_since_epoch().count(), prev.time_since_epoch().count());
        prev = curr;
    }
}

/// @brief Tests system_clock real-time timestamp and time_t conversions.
TEST(chrono_clock_test, system_clock_conversions)
{
    using clock = tempest::chrono::system_clock;

    // 1. Act: Query system clock
    constexpr auto jan_1_2026_epoch = 1'767'225'600LL;
    auto now = clock::now();
    auto time_val = clock::to_time_t(now);

    // 2. Assert: verify timestamp is after Jan 1, 2026 (approx 1767225600 seconds)
    EXPECT_GT(time_val, jan_1_2026_epoch);

    // 3. Act & Assert: round-trip through from_time_t
    auto reconstructed = clock::from_time_t(time_val);
    auto reconstructed_time_val = clock::to_time_t(reconstructed);
    EXPECT_EQ(time_val, reconstructed_time_val);
}

/// @brief Tests that high_resolution_clock is an alias of steady_clock.
TEST(chrono_clock_test, high_resolution_clock_alias)
{
    static_assert(tempest::is_same_v<tempest::chrono::high_resolution_clock, tempest::chrono::steady_clock>);
    EXPECT_TRUE((tempest::is_same_v<tempest::chrono::high_resolution_clock, tempest::chrono::steady_clock>));
}

//==============================================================================
// User-Defined Literals Tests
//==============================================================================

/// @brief Tests user-defined literals for integer and floating-point durations.
TEST(chrono_literals_test, duration_literals)
{
    using namespace tempest::chrono_literals;

    // 1. Setup & Act: integer duration literals with named constants
    constexpr auto zero_s = 0_s;
    constexpr auto zero_ms = 0_ms;
    constexpr auto nanos = 500_ns;
    constexpr auto micros = 400_us;
    constexpr auto millis = 300_ms;
    constexpr auto secs = 2_s;
    constexpr auto mins = 5_min;
    constexpr auto hours = 1_h;

    constexpr auto expected_nanos_count = 500;
    constexpr auto expected_micros_count = 400;
    constexpr auto expected_millis_count = 300;
    constexpr auto expected_mins_count = 5;

    // 2. Assert: counts and types
    EXPECT_EQ(zero_s.count(), 0);
    EXPECT_EQ(zero_ms.count(), 0);
    EXPECT_EQ(nanos.count(), expected_nanos_count);
    EXPECT_EQ(micros.count(), expected_micros_count);
    EXPECT_EQ(millis.count(), expected_millis_count);
    EXPECT_EQ(secs.count(), 2);
    EXPECT_EQ(mins.count(), expected_mins_count);
    EXPECT_EQ(hours.count(), 1);

    // 3. Setup & Act: floating-point duration literals
    constexpr auto f_secs = 1.5_s;
    constexpr auto f_millis = 2.5_ms;
    constexpr auto f_micros = 3.5_us;
    constexpr auto f_nanos = 4.5_ns;

    constexpr auto expected_f_secs_val = 1.5;
    constexpr auto expected_f_millis_val = 2.5;
    constexpr auto expected_f_micros_val = 3.5;
    constexpr auto expected_f_nanos_val = 4.5;

    // 4. Assert: floating point values
    EXPECT_DOUBLE_EQ(static_cast<double>(f_secs.count()), expected_f_secs_val);
    EXPECT_DOUBLE_EQ(static_cast<double>(f_millis.count()), expected_f_millis_val);
    EXPECT_DOUBLE_EQ(static_cast<double>(f_micros.count()), expected_f_micros_val);
    EXPECT_DOUBLE_EQ(static_cast<double>(f_nanos.count()), expected_f_nanos_val);

    // 5. Assert: literal arithmetic and equivalence
    constexpr auto thousand_ms = 1000_ms;
    constexpr auto sixty_secs = 60_s;
    constexpr auto sixty_mins = 60_min;
    constexpr auto twenty_four_hours = 24_h;
    constexpr auto fourteen_forty_mins = 1440_min;
    constexpr auto eighty_six_four_hundred_secs = 86400_s;
    constexpr auto one_million_nanos = 1000000_ns;

    EXPECT_EQ(thousand_ms, 1_s);
    EXPECT_EQ(sixty_secs, 1_min);
    EXPECT_EQ(sixty_mins, 1_h);
    EXPECT_EQ(twenty_four_hours, fourteen_forty_mins);
    EXPECT_EQ(fourteen_forty_mins, eighty_six_four_hundred_secs);
    EXPECT_EQ(one_million_nanos, 1_ms);
}

/// @brief Tests that literals can be resolved through tempest::literals and tempest::chrono.
TEST(chrono_literals_test, namespace_resolution)
{
    // 1. Act & Assert: tempest::literals
    {
        using namespace tempest::literals;
        constexpr auto dur_hundred_ms = 100_ms;
        constexpr auto expected_hundred = 100;
        auto dur = dur_hundred_ms;
        EXPECT_EQ(dur.count(), expected_hundred);
    }

    // 2. Act & Assert: tempest::chrono
    {
        using namespace tempest::chrono;
        constexpr auto dur_fifty_s = 50_s;
        constexpr auto expected_fifty = 50;
        auto dur = dur_fifty_s;
        EXPECT_EQ(dur.count(), expected_fifty);
    }
}
