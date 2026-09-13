#ifndef tempest_core_bitset_hpp
#define tempest_core_bitset_hpp

#include <tempest/array.hpp>
#include <tempest/bit.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>

namespace tempest
{
    /// @brief Fixed-capacity, zero-allocation multi-word bitset container.
    /// @tparam N Total number of bits representable by the bitset.
    template <size_t N>
    class bitset
    {
      public:
        static constexpr size_t bits_per_word = 64;
        static constexpr size_t bit_count = N;
        static constexpr size_t word_count = (N == 0) ? 1 : ((N + bits_per_word - 1) / bits_per_word);

        constexpr bitset() noexcept = default;

        constexpr explicit bitset(uint64_t initial_val) noexcept
        {
            _words[0] = initial_val;
            _sanitize_tail();
        }

        [[nodiscard]] static constexpr auto size() noexcept -> size_t
        {
            return N;
        }

        [[nodiscard]] static constexpr auto num_words() noexcept -> size_t
        {
            return word_count;
        }

        [[nodiscard]] constexpr auto count() const noexcept -> size_t
        {
            if constexpr (N == 0)
            {
                return 0;
            }
            auto total = size_t{0};
            for (auto idx = size_t{0}; idx < word_count; ++idx)
            {
                total += static_cast<size_t>(tempest::popcount(_words[idx]));
            }
            return total;
        }

        [[nodiscard]] constexpr auto any() const noexcept -> bool
        {
            if constexpr (N == 0)
            {
                return false;
            }
            for (auto idx = size_t{0}; idx < word_count; ++idx)
            {
                if (_words[idx] != 0ULL)
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] constexpr auto none() const noexcept -> bool
        {
            return !any();
        }

        [[nodiscard]] constexpr auto all() const noexcept -> bool
        {
            if constexpr (N == 0)
            {
                return true;
            }
            for (auto idx = size_t{0}; idx < word_count - 1; ++idx)
            {
                if (_words[idx] != ~0ULL)
                {
                    return false;
                }
            }
            return _words[word_count - 1] == _tail_mask();
        }

        [[nodiscard]] constexpr auto test(size_t pos) const noexcept -> bool
        {
            if (pos >= N)
            {
                return false;
            }
            return (_words[pos / bits_per_word] & (1ULL << (pos % bits_per_word))) != 0;
        }

        [[nodiscard]] constexpr auto operator[](size_t pos) const noexcept -> bool
        {
            return test(pos);
        }

        constexpr auto set(size_t pos, bool value = true) noexcept -> bitset&
        {
            if (pos < N)
            {
                if (value)
                {
                    _words[pos / bits_per_word] |= (1ULL << (pos % bits_per_word));
                }
                else
                {
                    _words[pos / bits_per_word] &= ~(1ULL << (pos % bits_per_word));
                }
            }
            return *this;
        }

        constexpr auto set() noexcept -> bitset&
        {
            for (auto idx = size_t{0}; idx < word_count; ++idx)
            {
                _words[idx] = ~0ULL;
            }
            _sanitize_tail();
            return *this;
        }

        constexpr auto reset(size_t pos) noexcept -> bitset&
        {
            if (pos < N)
            {
                _words[pos / bits_per_word] &= ~(1ULL << (pos % bits_per_word));
            }
            return *this;
        }

        constexpr auto reset() noexcept -> bitset&
        {
            for (auto idx = size_t{0}; idx < word_count; ++idx)
            {
                _words[idx] = 0ULL;
            }
            return *this;
        }

        constexpr auto flip(size_t pos) noexcept -> bitset&
        {
            if (pos < N)
            {
                _words[pos / bits_per_word] ^= (1ULL << (pos % bits_per_word));
            }
            return *this;
        }

        constexpr auto flip() noexcept -> bitset&
        {
            for (auto idx = size_t{0}; idx < word_count; ++idx)
            {
                _words[idx] = ~_words[idx];
            }
            _sanitize_tail();
            return *this;
        }

        [[nodiscard]] auto find_first() const noexcept -> optional<size_t>
        {
            if constexpr (N == 0)
            {
                return nullopt;
            }
            for (auto word_idx = size_t{0}; word_idx < word_count; ++word_idx)
            {
                if (_words[word_idx] != 0ULL)
                {
                    const auto bit_idx = static_cast<size_t>(tempest::countr_zero(_words[word_idx]));
                    const auto total_idx = (word_idx * bits_per_word) + bit_idx;
                    if (total_idx < N)
                    {
                        return total_idx;
                    }
                }
            }
            return nullopt;
        }

        [[nodiscard]] auto find_next(size_t prev_pos) const noexcept -> optional<size_t>
        {
            const auto next_pos = prev_pos + 1;
            if (next_pos >= N)
            {
                return nullopt;
            }

            auto word_idx = next_pos / bits_per_word;
            const auto bit_offset = next_pos % bits_per_word;

            const auto mask = ~0ULL << bit_offset;
            const auto first_val = _words[word_idx] & mask;
            if (first_val != 0ULL)
            {
                const auto bit_idx = static_cast<size_t>(tempest::countr_zero(first_val));
                const auto total_idx = (word_idx * bits_per_word) + bit_idx;
                if (total_idx < N)
                {
                    return total_idx;
                }
                return nullopt;
            }

            for (word_idx = word_idx + 1; word_idx < word_count; ++word_idx)
            {
                if (_words[word_idx] != 0ULL)
                {
                    auto bit_idx = static_cast<size_t>(tempest::countr_zero(_words[word_idx]));
                    auto total_idx = (word_idx * bits_per_word) + bit_idx;
                    if (total_idx < N)
                    {
                        return total_idx;
                    }
                    return nullopt;
                }
            }
            return nullopt;
        }

        [[nodiscard]] constexpr auto operator~() const noexcept -> bitset
        {
            auto result = *this;
            result.flip();
            return result;
        }

        constexpr auto operator&=(const bitset& other) noexcept -> bitset&
        {
            for (auto idx = size_t{0}; idx < word_count; ++idx)
            {
                _words[idx] &= other._words[idx];
            }
            return *this;
        }

        constexpr auto operator|=(const bitset& other) noexcept -> bitset&
        {
            for (auto idx = size_t{0}; idx < word_count; ++idx)
            {
                _words[idx] |= other._words[idx];
            }
            return *this;
        }

        constexpr auto operator^=(const bitset& other) noexcept -> bitset&
        {
            for (auto idx = size_t{0}; idx < word_count; ++idx)
            {
                _words[idx] ^= other._words[idx];
            }
            _sanitize_tail();
            return *this;
        }

        friend constexpr auto operator&(const bitset& lhs, const bitset& rhs) noexcept -> bitset
        {
            auto result = lhs;
            result &= rhs;
            return result;
        }

        friend constexpr auto operator|(const bitset& lhs, const bitset& rhs) noexcept -> bitset
        {
            auto result = lhs;
            result |= rhs;
            return result;
        }

        friend constexpr auto operator^(const bitset& lhs, const bitset& rhs) noexcept -> bitset
        {
            auto result = lhs;
            result ^= rhs;
            return result;
        }

        friend constexpr auto operator==(const bitset& lhs, const bitset& rhs) noexcept -> bool
        {
            for (auto idx = size_t{0}; idx < word_count; ++idx)
            {
                if (lhs._words[idx] != rhs._words[idx])
                {
                    return false;
                }
            }
            return true;
        }

        friend constexpr auto operator!=(const bitset& lhs, const bitset& rhs) noexcept -> bool
        {
            return !(lhs == rhs);
        }

        [[nodiscard]] constexpr auto word(size_t index) const noexcept -> uint64_t
        {
            if (index < word_count)
            {
                return _words[index];
            }
            return 0ULL;
        }

        constexpr void set_word(size_t index, uint64_t value) noexcept
        {
            if (index < word_count)
            {
                _words[index] = value;
                if (index == word_count - 1)
                {
                    _sanitize_tail();
                }
            }
        }

        [[nodiscard]] constexpr auto words() const noexcept -> const array<uint64_t, word_count>&
        {
            return _words;
        }

        [[nodiscard]] constexpr auto words() noexcept -> array<uint64_t, word_count>&
        {
            return _words;
        }

        [[nodiscard]] constexpr auto data() noexcept -> uint64_t*
        {
            return _words.data();
        }

        [[nodiscard]] constexpr auto data() const noexcept -> const uint64_t*
        {
            return _words.data();
        }

        [[nodiscard]] constexpr auto to_uint64() const noexcept -> uint64_t
        {
            return _words[0];
        }

      private:
        static constexpr auto _tail_mask() noexcept -> uint64_t
        {
            if constexpr (N == 0)
            {
                return 0ULL;
            }
            else if constexpr (N % bits_per_word == 0)
            {
                return ~0ULL;
            }
            else
            {
                return (1ULL << (N % bits_per_word)) - 1ULL;
            }
        }

        constexpr void _sanitize_tail() noexcept
        {
            if constexpr (N == 0)
            {
                _words[0] = 0ULL;
            }
            else if constexpr (N % bits_per_word != 0)
            {
                _words[word_count - 1] &= _tail_mask();
            }
        }

        array<uint64_t, word_count> _words{};
    };
} // namespace tempest

#endif // tempest_core_bitset_hpp
