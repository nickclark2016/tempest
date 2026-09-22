#ifndef tempest_job_idle_mask_hpp
#define tempest_job_idle_mask_hpp

#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/bit.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/thread.hpp>

namespace tempest::job
{
    template <size_t MaxWorkers = 256>
    class scalable_idle_mask
    {
        static_assert((MaxWorkers % 64) == 0 && MaxWorkers >= 64, "MaxWorkers must be a multiple of 64 and >= 64");
        static constexpr size_t BitsPerWord = 64;
        static constexpr size_t WordCount = MaxWorkers / BitsPerWord;

      public:
        scalable_idle_mask() noexcept
        {
            for (auto& word : _words)
            {
                word.store(0ULL, memory_order::relaxed);
            }
        }

        ~scalable_idle_mask() = default;
        scalable_idle_mask(const scalable_idle_mask&) = delete;
        scalable_idle_mask(scalable_idle_mask&&) noexcept = delete;
        auto operator=(const scalable_idle_mask&) -> scalable_idle_mask& = delete;
        auto operator=(scalable_idle_mask&&) noexcept -> scalable_idle_mask& = delete;

        /// @brief Marks the specified worker index as idle.
        auto mark_idle(size_t worker_index) noexcept -> void
        {
            if (worker_index < MaxWorkers)
            {
                const auto word_index = worker_index / BitsPerWord;
                const auto bit_index = worker_index % BitsPerWord;
                _words[word_index].fetch_or(1ULL << bit_index, memory_order::acq_rel);
            }
        }

        /// @brief Clears the idle status of the specified worker index.
        auto clear_idle(size_t worker_index) noexcept -> void
        {
            if (worker_index < MaxWorkers)
            {
                const auto word_index = worker_index / BitsPerWord;
                const auto bit_index = worker_index % BitsPerWord;
                _words[word_index].fetch_and(~(1ULL << bit_index), memory_order::acq_rel);
            }
        }

        /// @brief Checks whether the specified worker index is currently marked idle.
        [[nodiscard]] auto is_idle(size_t worker_index) const noexcept -> bool
        {
            if (worker_index >= MaxWorkers)
            {
                return false;
            }
            const auto word_index = worker_index / BitsPerWord;
            const auto bit_index = worker_index % BitsPerWord;
            return (_words[word_index].load(memory_order::relaxed) & (1ULL << bit_index)) != 0;
        }

        /// @brief Finds the first idle worker index within [start_index, start_index + count).
        [[nodiscard]] auto find_idle(size_t start_index, size_t count) const noexcept -> optional<size_t>
        {
            if (count == 0 || start_index >= MaxWorkers)
            {
                return nullopt;
            }

            const auto end_index = min(start_index + count, MaxWorkers);
            const auto first_word = start_index / BitsPerWord;
            const auto last_word = (end_index - 1) / BitsPerWord;

            for (auto word_idx = first_word; word_idx <= last_word; ++word_idx)
            {
                auto word = _words[word_idx].load(memory_order::relaxed);
                if (word == 0ULL)
                {
                    continue;
                }

                if (word_idx == first_word)
                {
                    const auto shift = start_index % BitsPerWord;
                    word &= (~0ULL << shift);
                }

                if (word_idx == last_word)
                {
                    const auto remainder = end_index % BitsPerWord;
                    if (remainder != 0)
                    {
                        word &= ((1ULL << remainder) - 1ULL);
                    }
                }

                if (word != 0ULL)
                {
                    const auto bit_offset = static_cast<size_t>(tempest::countr_zero(word));
                    const auto candidate_index = word_idx * BitsPerWord + bit_offset;
                    if (candidate_index < end_index)
                    {
                        return candidate_index;
                    }
                }
            }

            return nullopt;
        }

        /// @brief Atomically finds the first idle worker, clears its idle bit, and returns its index.
        [[nodiscard]] auto claim_first_idle(size_t worker_count) noexcept -> optional<size_t>
        {
            const auto limit = min(worker_count, MaxWorkers);
            const auto words_to_scan = (limit + BitsPerWord - 1) / BitsPerWord;

            for (auto word_idx = size_t{0}; word_idx < words_to_scan; ++word_idx)
            {
                auto word = _words[word_idx].load(memory_order::relaxed);
                while (word != 0ULL)
                {
                    const auto bit_offset = static_cast<size_t>(tempest::countr_zero(word));
                    const auto candidate_index = word_idx * BitsPerWord + bit_offset;
                    if (candidate_index >= limit)
                    {
                        break;
                    }

                    const auto mask = 1ULL << bit_offset;
                    if ((_words[word_idx].fetch_and(~mask, memory_order::acq_rel) & mask) != 0)
                    {
                        return candidate_index;
                    }

                    word = _words[word_idx].load(memory_order::relaxed);
                }
            }

            return nullopt;
        }

      private:
        alignas(hardware_destructive_interference_size) array<atomic<uint64_t>, WordCount> _words{};
    };
} // namespace tempest::job

#endif // tempest_job_idle_mask_hpp
