#ifndef tempest_core_random_hpp
#define tempest_core_random_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/concepts.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    /// @brief PCG-XSH-RR 32-bit pseudo-random number generator.
    /// Fast 32-bit generator with 64-bit state and configurable streams.
    class pcg32
    {
      public:
        using result_type = uint32_t;

        static constexpr uint64_t default_seed = 0x853c49e6748fea9bULL;
        static constexpr uint64_t default_stream = 0xda3e39cb94b95bdbULL;
        static constexpr uint64_t multiplier = 6364136223846793005ULL;

        constexpr pcg32() noexcept = default;

        explicit constexpr pcg32(uint64_t seed_value, uint64_t stream = default_stream) noexcept
        {
            seed(seed_value, stream);
        }

        constexpr auto seed(uint64_t seed_value, // NOLINT(bugprone-easily-swappable-parameters)
                            uint64_t stream = default_stream) noexcept -> void
        {
            _state = 0U;
            _inc = (stream << 1U) | 1U;
            operator()();
            _state += seed_value;
            operator()();
        }

        constexpr auto operator()() noexcept -> result_type
        {
            constexpr auto xor_shift_offset = 18U;
            constexpr auto xor_shift_amount = 27U;
            constexpr auto rot_shift_amount = 59U;
            constexpr auto rot_mask = 31U;
            constexpr auto word_bits = 32U;

            const auto oldstate = _state;
            _state = (oldstate * multiplier) + _inc;
            const auto xorshifted =
                static_cast<uint32_t>(((oldstate >> xor_shift_offset) ^ oldstate) >> xor_shift_amount);
            const auto rot = static_cast<uint32_t>(oldstate >> rot_shift_amount);
            return (xorshifted >> rot) | (xorshifted << ((word_bits - rot) & rot_mask));
        }

        constexpr auto discard(uint64_t count) noexcept -> void
        {
            auto cur_mult = multiplier;
            auto cur_plus = _inc;
            auto acc_mult = static_cast<uint64_t>(1U);
            auto acc_plus = static_cast<uint64_t>(0U);

            while (count > 0)
            {
                if ((count & 1U) != 0U)
                {
                    acc_mult *= cur_mult;
                    acc_plus = (acc_plus * cur_mult) + cur_plus;
                }
                cur_plus = (cur_mult + 1U) * cur_plus;
                cur_mult *= cur_mult;
                count /= 2U;
            }
            _state = (acc_mult * _state) + acc_plus;
        }

        [[nodiscard]] static constexpr auto min() noexcept -> result_type
        {
            return 0U;
        }

        [[nodiscard]] static constexpr auto max() noexcept -> result_type
        {
            constexpr auto max_val = 0xFFFF'FFFFU;
            return max_val;
        }

        [[nodiscard]] friend constexpr auto operator==(const pcg32& lhs, const pcg32& rhs) noexcept -> bool
        {
            return lhs._state == rhs._state && lhs._inc == rhs._inc;
        }

        [[nodiscard]] friend constexpr auto operator!=(const pcg32& lhs, const pcg32& rhs) noexcept -> bool
        {
            return !(lhs == rhs);
        }

      private:
        uint64_t _state{default_seed};
        uint64_t _inc{default_stream};
    };

    /// @brief xoshiro256** pseudo-random number generator.
    /// Fast 64-bit generator with 256-bit state, jump capabilities, and excellent statistical properties.
    class xoshiro256starstar
    {
      public:
        using result_type = uint64_t;

        static constexpr uint64_t default_seed = 0x853c49e6748fea9bULL;

        constexpr xoshiro256starstar() noexcept
        {
            seed(default_seed);
        }

        explicit constexpr xoshiro256starstar(uint64_t seed_value) noexcept
        {
            seed(seed_value);
        }

        constexpr xoshiro256starstar(uint64_t seed0, uint64_t seed1, uint64_t seed2, uint64_t seed3) noexcept
            : _state{seed0, seed1, seed2, seed3}
        {
            if (_state[0] == 0 && _state[1] == 0 && _state[2] == 0 && _state[3] == 0)
            {
                seed(default_seed);
            }
        }

        constexpr auto seed(uint64_t seed_value) noexcept -> void
        {
            auto sm_state = seed_value;
            _state[0] = splitmix64(sm_state);
            _state[1] = splitmix64(sm_state);
            _state[2] = splitmix64(sm_state);
            _state[3] = splitmix64(sm_state);
        }

        constexpr auto seed(uint64_t seed0, uint64_t seed1, uint64_t seed2, uint64_t seed3) noexcept -> void
        {
            _state[0] = seed0;
            _state[1] = seed1;
            _state[2] = seed2;
            _state[3] = seed3;
            if (_state[0] == 0 && _state[1] == 0 && _state[2] == 0 && _state[3] == 0)
            {
                seed(default_seed);
            }
        }

        constexpr auto operator()() noexcept -> result_type
        {
            constexpr auto mult1 = 5ULL;
            constexpr auto rot1 = 7;
            constexpr auto mult2 = 9ULL;
            constexpr auto shift_amount = 17;
            constexpr auto rot2 = 45;

            const auto result = rotl(_state[1] * mult1, rot1) * mult2;
            const auto shifted_state = _state[1] << shift_amount;

            _state[2] ^= _state[0];
            _state[3] ^= _state[1];
            _state[1] ^= _state[2];
            _state[0] ^= _state[3];

            _state[2] ^= shifted_state;
            _state[3] = rotl(_state[3], rot2);

            return result;
        }

        /// @brief Advances state by 2^128 steps, equivalent to 2^128 calls to operator()().
        constexpr auto jump() noexcept -> void
        {
            constexpr auto jump_table = array<uint64_t, 4>{0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL,
                                                           0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL};
            constexpr auto bits_in_entry = 64U;

            auto accum0 = static_cast<uint64_t>(0);
            auto accum1 = static_cast<uint64_t>(0);
            auto accum2 = static_cast<uint64_t>(0);
            auto accum3 = static_cast<uint64_t>(0);

            for (const auto entry : jump_table)
            {
                for (auto bit_pos = 0U; bit_pos < bits_in_entry; ++bit_pos)
                {
                    if ((entry & (1ULL << bit_pos)) != 0U)
                    {
                        accum0 ^= _state[0];
                        accum1 ^= _state[1];
                        accum2 ^= _state[2];
                        accum3 ^= _state[3];
                    }
                    operator()();
                }
            }

            _state[0] = accum0;
            _state[1] = accum1;
            _state[2] = accum2;
            _state[3] = accum3;
        }

        /// @brief Advances state by 2^192 steps, equivalent to 2^192 calls to operator()().
        constexpr auto long_jump() noexcept -> void
        {
            constexpr auto long_jump_table = array<uint64_t, 4>{0x76e15d3efefdcbbfULL, 0xc5004e441c522fb3ULL,
                                                                0x77710069854ee241ULL, 0x39109bb02acbe635ULL};
            constexpr auto bits_in_entry = 64U;

            auto accum0 = static_cast<uint64_t>(0);
            auto accum1 = static_cast<uint64_t>(0);
            auto accum2 = static_cast<uint64_t>(0);
            auto accum3 = static_cast<uint64_t>(0);

            for (const auto entry : long_jump_table)
            {
                for (auto bit_pos = 0U; bit_pos < bits_in_entry; ++bit_pos)
                {
                    if ((entry & (1ULL << bit_pos)) != 0U)
                    {
                        accum0 ^= _state[0];
                        accum1 ^= _state[1];
                        accum2 ^= _state[2];
                        accum3 ^= _state[3];
                    }
                    operator()();
                }
            }

            _state[0] = accum0;
            _state[1] = accum1;
            _state[2] = accum2;
            _state[3] = accum3;
        }

        constexpr auto discard(uint64_t count) noexcept -> void
        {
            for (auto i = 0ULL; i < count; ++i)
            {
                operator()();
            }
        }

        [[nodiscard]] static constexpr auto min() noexcept -> result_type
        {
            return 0ULL;
        }

        [[nodiscard]] static constexpr auto max() noexcept -> result_type
        {
            constexpr auto max_val = 0xFFFF'FFFF'FFFF'FFFFULL;
            return max_val;
        }

        [[nodiscard]] friend constexpr auto operator==(const xoshiro256starstar& lhs,
                                                       const xoshiro256starstar& rhs) noexcept -> bool
        {
            return lhs._state == rhs._state;
        }

        [[nodiscard]] friend constexpr auto operator!=(const xoshiro256starstar& lhs,
                                                       const xoshiro256starstar& rhs) noexcept -> bool
        {
            return !(lhs == rhs);
        }

      private:
        [[nodiscard]] static constexpr auto rotl(uint64_t val, int shift) noexcept -> uint64_t
        {
            constexpr auto total_bits = 64;
            return (val << shift) | (val >> (total_bits - shift));
        }

        [[nodiscard]] static constexpr auto splitmix64(uint64_t& state_var) noexcept -> uint64_t
        {
            constexpr auto inc = 0x9e3779b97f4a7c15ULL;
            constexpr auto mult1 = 0xbf58476d1ce4e5b9ULL;
            constexpr auto mult2 = 0x94d049bb133111ebULL;
            constexpr auto shift1 = 30U;
            constexpr auto shift2 = 27U;
            constexpr auto shift3 = 31U;

            auto intermediate_val = (state_var += inc);
            intermediate_val = (intermediate_val ^ (intermediate_val >> shift1)) * mult1;
            intermediate_val = (intermediate_val ^ (intermediate_val >> shift2)) * mult2;
            return intermediate_val ^ (intermediate_val >> shift3);
        }

        array<uint64_t, 4> _state{};
    };

    /// @brief Default random engine type alias.
    using default_random_engine = pcg32;

    /// @brief Hardware / OS entropy source.
    class TEMPEST_API random_device
    {
      public:
        using result_type = uint32_t;

        random_device() = default;
        ~random_device() = default;

        random_device(const random_device&) = delete;
        auto operator=(const random_device&) -> random_device& = delete;
        random_device(random_device&&) noexcept = default;
        auto operator=(random_device&&) noexcept -> random_device& = default;

        auto operator()() -> result_type;
        auto generate(void* buffer, size_t size) -> void;

        [[nodiscard]] static constexpr auto min() noexcept -> result_type
        {
            return 0U;
        }

        [[nodiscard]] static constexpr auto max() noexcept -> result_type
        {
            constexpr auto max_val = 0xFFFF'FFFFU;
            return max_val;
        }
    };

    /// @brief Uniform integer distribution generating integers uniformly on closed interval [a, b].
    template <integral IntType = int>
    class uniform_int_distribution
    {
      public:
        using result_type = IntType;

        struct param_type
        {
            using distribution_type = uniform_int_distribution<IntType>;

            constexpr param_type() noexcept : _max_val{numeric_limits<IntType>::max()}
            {
            }

            constexpr param_type(IntType min_bound, // NOLINT(bugprone-easily-swappable-parameters)
                                 IntType max_bound = numeric_limits<IntType>::max()) noexcept
                : _min_val{min_bound}, _max_val{max_bound}
            {
            }

            [[nodiscard]] constexpr auto a() const noexcept -> IntType
            {
                return _min_val;
            }

            [[nodiscard]] constexpr auto b() const noexcept -> IntType
            {
                return _max_val;
            }

            [[nodiscard]] friend constexpr auto operator==(const param_type& lhs, const param_type& rhs) noexcept
                -> bool
            {
                return lhs._min_val == rhs._min_val && lhs._max_val == rhs._max_val;
            }

            [[nodiscard]] friend constexpr auto operator!=(const param_type& lhs, const param_type& rhs) noexcept
                -> bool
            {
                return !(lhs == rhs);
            }

          private:
            IntType _min_val{0};
            IntType _max_val{numeric_limits<IntType>::max()};
        };

        constexpr uniform_int_distribution() noexcept : _param{0, numeric_limits<IntType>::max()}
        {
        }

        constexpr uniform_int_distribution(IntType min_bound, // NOLINT(bugprone-easily-swappable-parameters)
                                           IntType max_bound = numeric_limits<IntType>::max()) noexcept
            : _param{min_bound, max_bound}
        {
        }

        explicit constexpr uniform_int_distribution(const param_type& parm) noexcept : _param{parm}
        {
        }

        constexpr auto reset() noexcept -> void
        {
        }

        [[nodiscard]] constexpr auto a() const noexcept -> IntType
        {
            return _param.a();
        }

        [[nodiscard]] constexpr auto b() const noexcept -> IntType
        {
            return _param.b();
        }

        [[nodiscard]] constexpr auto param() const noexcept -> param_type
        {
            return _param;
        }

        constexpr auto param(const param_type& parm) noexcept -> void
        {
            _param = parm;
        }

        [[nodiscard]] constexpr auto min() const noexcept -> result_type
        {
            return a();
        }

        [[nodiscard]] constexpr auto max() const noexcept -> result_type
        {
            return b();
        }

        template <typename Engine>
        [[nodiscard]] auto operator()(Engine& urng) -> result_type
        {
            return (*this)(urng, _param);
        }

        template <typename Engine>
        [[nodiscard]] auto operator()(Engine& urng, const param_type& parm) -> result_type
        {
            using unsigned_type = make_unsigned_t<IntType>;

            if (parm.a() >= parm.b())
            {
                return parm.a();
            }

            const auto u_a = static_cast<unsigned_type>(parm.a());
            const auto u_b = static_cast<unsigned_type>(parm.b());
            const auto range = static_cast<unsigned_type>(u_b - u_a);

            if constexpr (sizeof(IntType) <= sizeof(uint32_t))
            {
                constexpr auto full_32_bit_range = 0xFFFF'FFFFU;
                constexpr auto word_shift = 32U;

                if (range == full_32_bit_range)
                {
                    const auto raw_val = static_cast<uint32_t>(urng());
                    return static_cast<IntType>(u_a + raw_val);
                }

                // Lemire's nearly-divisionless algorithm
                const auto range_size = static_cast<uint32_t>(range + 1U);
                auto raw_val = static_cast<uint32_t>(urng());
                auto product = static_cast<uint64_t>(raw_val) * static_cast<uint64_t>(range_size);
                auto lower_bits = static_cast<uint32_t>(product);

                if (lower_bits < range_size)
                {
                    const auto threshold = (static_cast<uint32_t>(0U) - range_size) % range_size;
                    while (lower_bits < threshold)
                    {
                        raw_val = static_cast<uint32_t>(urng());
                        product = static_cast<uint64_t>(raw_val) * static_cast<uint64_t>(range_size);
                        lower_bits = static_cast<uint32_t>(product);
                    }
                }

                return static_cast<IntType>(u_a + static_cast<unsigned_type>(product >> word_shift));
            }
            else
            {
                constexpr auto full_64_bit_range = 0xFFFF'FFFF'FFFF'FFFFULL;
                constexpr auto word_shift = 32U;

                auto sample_64 = [&urng]() -> uint64_t {
                    if constexpr (sizeof(typename Engine::result_type) >= sizeof(uint64_t))
                    {
                        return static_cast<uint64_t>(urng());
                    }
                    else
                    {
                        return (static_cast<uint64_t>(urng()) << word_shift) | static_cast<uint64_t>(urng());
                    }
                };

                if (range == full_64_bit_range)
                {
                    return static_cast<IntType>(u_a + static_cast<unsigned_type>(sample_64()));
                }

                const auto range_size = static_cast<uint64_t>(range + 1U);
                const auto limit = full_64_bit_range - (full_64_bit_range % range_size);

                auto raw_val = static_cast<uint64_t>(0);
                do
                {
                    raw_val = sample_64();
                } while (raw_val >= limit);

                return static_cast<IntType>(u_a + static_cast<unsigned_type>(raw_val % range_size));
            }
        }

        [[nodiscard]] friend constexpr auto operator==(const uniform_int_distribution& lhs,
                                                       const uniform_int_distribution& rhs) noexcept -> bool
        {
            return lhs._param == rhs._param;
        }

        [[nodiscard]] friend constexpr auto operator!=(const uniform_int_distribution& lhs,
                                                       const uniform_int_distribution& rhs) noexcept -> bool
        {
            return !(lhs == rhs);
        }

      private:
        param_type _param;
    };

    /// @brief Uniform real distribution generating floating-point values uniformly on half-open interval [a, b).
    template <floating_point RealType = double>
    class uniform_real_distribution
    {
      public:
        using result_type = RealType;

        struct param_type
        {
            using distribution_type = uniform_real_distribution<RealType>;

            constexpr param_type() noexcept = default;

            constexpr param_type(RealType min_bound, // NOLINT(bugprone-easily-swappable-parameters)
                                 RealType max_bound = static_cast<RealType>(1.0)) noexcept
                : _min_val{min_bound}, _max_val{max_bound}
            {
            }

            [[nodiscard]] constexpr auto a() const noexcept -> RealType
            {
                return _min_val;
            }

            [[nodiscard]] constexpr auto b() const noexcept -> RealType
            {
                return _max_val;
            }

            [[nodiscard]] friend constexpr auto operator==(const param_type& lhs, const param_type& rhs) noexcept
                -> bool
            {
                return lhs._min_val == rhs._min_val && lhs._max_val == rhs._max_val;
            }

            [[nodiscard]] friend constexpr auto operator!=(const param_type& lhs, const param_type& rhs) noexcept
                -> bool
            {
                return !(lhs == rhs);
            }

          private:
            RealType _min_val{static_cast<RealType>(0.0)};
            RealType _max_val{static_cast<RealType>(1.0)};
        };

        constexpr uniform_real_distribution() noexcept : _param{static_cast<RealType>(0.0), static_cast<RealType>(1.0)}
        {
        }

        constexpr uniform_real_distribution(RealType min_bound, // NOLINT(bugprone-easily-swappable-parameters)
                                            RealType max_bound = static_cast<RealType>(1.0)) noexcept
            : _param{min_bound, max_bound}
        {
        }

        explicit constexpr uniform_real_distribution(const param_type& parm) noexcept : _param{parm}
        {
        }

        constexpr auto reset() noexcept -> void
        {
        }

        [[nodiscard]] constexpr auto a() const noexcept -> RealType
        {
            return _param.a();
        }

        [[nodiscard]] constexpr auto b() const noexcept -> RealType
        {
            return _param.b();
        }

        [[nodiscard]] constexpr auto param() const noexcept -> param_type
        {
            return _param;
        }

        constexpr auto param(const param_type& parm) noexcept -> void
        {
            _param = parm;
        }

        [[nodiscard]] constexpr auto min() const noexcept -> result_type
        {
            return a();
        }

        [[nodiscard]] constexpr auto max() const noexcept -> result_type
        {
            return b();
        }

        template <typename Engine>
        [[nodiscard]] auto operator()(Engine& urng) -> result_type
        {
            return (*this)(urng, _param);
        }

        template <typename Engine>
        [[nodiscard]] auto operator()(Engine& urng, const param_type& parm) -> result_type
        {
            if (parm.a() >= parm.b())
            {
                return parm.a();
            }

            if constexpr (is_same_v<RealType, float>)
            {
                constexpr auto factor = 1.0F / 16777216.0F; // 2^-24
                constexpr auto shift_amount = 8U;

                auto result = static_cast<RealType>(0);
                do
                {
                    const auto raw_bits = static_cast<uint32_t>(urng()) >> shift_amount;
                    const auto unit_float = static_cast<float>(raw_bits) * factor;
                    result = parm.a() + (unit_float * (parm.b() - parm.a()));
                } while (result >= parm.b());

                return result;
            }
            else
            {
                constexpr auto factor = 1.0 / 9007199254740992.0; // 2^-53
                constexpr auto shift_amount = 11U;
                constexpr auto word_shift = 32U;

                auto sample_64 = [&urng]() -> uint64_t {
                    if constexpr (sizeof(typename Engine::result_type) >= sizeof(uint64_t))
                    {
                        return static_cast<uint64_t>(urng());
                    }
                    else
                    {
                        return (static_cast<uint64_t>(urng()) << word_shift) | static_cast<uint64_t>(urng());
                    }
                };

                auto result = static_cast<RealType>(0);
                do
                {
                    const auto raw_bits = sample_64() >> shift_amount;
                    const auto unit_double = static_cast<double>(raw_bits) * factor;
                    result = parm.a() + (unit_double * (parm.b() - parm.a()));
                } while (result >= parm.b());

                return result;
            }
        }

        [[nodiscard]] friend constexpr auto operator==(const uniform_real_distribution& lhs,
                                                       const uniform_real_distribution& rhs) noexcept -> bool
        {
            return lhs._param == rhs._param;
        }

        [[nodiscard]] friend constexpr auto operator!=(const uniform_real_distribution& lhs,
                                                       const uniform_real_distribution& rhs) noexcept -> bool
        {
            return !(lhs == rhs);
        }

      private:
        param_type _param;
    };
} // namespace tempest

#endif // tempest_core_random_hpp
