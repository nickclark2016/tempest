#ifndef tempest_editor_core_log_store_hpp
#define tempest_editor_core_log_store_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/editor_log_sink.hpp>
#include <tempest/int.hpp>
#include <tempest/logger.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor
{
    /// @brief Storage node for an individual log entry within the circular ring buffer.
    struct TEMPEST_EDITOR_API log_record_entry
    {
        static constexpr uint64_t default_sequence_number = 0;
        static constexpr uint64_t default_timestamp_ns = 0;
        static constexpr uint32_t default_thread_id = 0;

        log_entry_node* node = nullptr;
        uint64_t sequence_number = default_sequence_number;

        log_record_entry() = default;

        ~log_record_entry()
        {
            if (node != nullptr)
            {
                log_entry_node::destroy(node);
                node = nullptr;
            }
        }

        log_record_entry(const log_record_entry&) = delete;
        auto operator=(const log_record_entry&) -> log_record_entry& = delete;

        log_record_entry(log_record_entry&& other) noexcept
            : node(tempest::exchange(other.node, nullptr))
            , sequence_number(tempest::exchange(other.sequence_number, default_sequence_number))
        {
        }

        auto operator=(log_record_entry&& other) noexcept -> log_record_entry&
        {
            if (this == &other)
            {
                return *this;
            }

            if (node != nullptr)
            {
                log_entry_node::destroy(node);
            }
            node = tempest::exchange(other.node, nullptr);
            sequence_number = tempest::exchange(other.sequence_number, default_sequence_number);
            return *this;
        }

        [[nodiscard]] auto level() const noexcept -> log_level
        {
            return node != nullptr ? node->level : log_level::info;
        }

        [[nodiscard]] auto timestamp_ns() const noexcept -> uint64_t
        {
            return node != nullptr ? node->timestamp_ns : default_timestamp_ns;
        }

        [[nodiscard]] auto thread_id() const noexcept -> uint32_t
        {
            return node != nullptr ? node->thread_id : default_thread_id;
        }

        [[nodiscard]] auto message() const noexcept -> string_view
        {
            return node != nullptr ? node->message() : string_view{};
        }

        [[nodiscard]] auto source() const noexcept -> source_location
        {
            return node != nullptr ? node->source : source_location{};
        }
    };

    /// @brief Incremental search and level filter state maintaining pre-filtered sequence indices and counters.
    struct TEMPEST_EDITOR_API log_filter_state
    {
        static constexpr uint32_t default_level_mask = 0x7F;
        static constexpr size_t default_count = 0;

        uint32_t level_mask = default_level_mask;
        string search_query{};
        vector<uint64_t> matched_sequences{};

        size_t total_count = default_count;
        size_t trace_count = default_count;
        size_t debug_count = default_count;
        size_t info_count = default_count;
        size_t warn_count = default_count;
        size_t error_count = default_count;
        size_t critical_count = default_count;
        size_t fatal_count = default_count;

        auto set_level_enabled(log_level level, bool enabled) noexcept -> void;
        [[nodiscard]] auto is_level_enabled(log_level level) const noexcept -> bool;
        [[nodiscard]] auto matches(const log_record_entry& entry) const -> bool;
        auto evaluate_and_append(const log_record_entry& entry) -> void;
        auto decrement_counter(log_level level) noexcept -> void;
        auto trim_evicted(uint64_t oldest_active_sequence) -> void;
        auto reset_matches_and_counters() -> void;
        [[nodiscard]] auto counter_for_level(log_level level) noexcept -> size_t*;
        [[nodiscard]] auto counter_for_level(log_level level) const noexcept -> const size_t*;
    };

    /// @brief Fixed-capacity circular ring buffer draining log records from editor_log_sink with incremental filtering.
    class TEMPEST_EDITOR_API log_store
    {
      public:
        static constexpr size_t default_capacity = 100'000;
        static constexpr uint64_t default_sequence = 0;
        static constexpr uint64_t default_dropped_count = 0;

        explicit log_store(size_t capacity = default_capacity);
        ~log_store() = default;

        log_store(const log_store&) = delete;
        auto operator=(const log_store&) -> log_store& = delete;

        log_store(log_store&&) noexcept = default;
        auto operator=(log_store&&) noexcept -> log_store& = default;

        /// @brief Drains all pending nodes from the log sink into the ring buffer, updating monotonic sequences and filters.
        /// @param sink The editor log sink to drain.
        /// @return Number of nodes drained.
        auto drain(editor_log_sink& sink) -> size_t;

        /// @brief Soft-clears the log store by advancing the clear watermark and resetting active filter matches.
        auto clear() -> void;

        /// @brief Enables or disables a specific log level in the filter, rebuilding cached matches.
        auto set_filter_level(log_level level, bool enabled) -> void;

        /// @brief Sets the case-insensitive search query in the filter, rebuilding cached matches.
        auto set_filter_search(string_view query) -> void;

        /// @brief Rebuilds the entire filter cache across all currently visible log entries.
        auto rebuild_filter() -> void;

        /// @brief Retrieves a pointer to the log record entry with the given sequence number, or nullptr if evicted/cleared.
        [[nodiscard]] auto get_entry(uint64_t sequence) const noexcept -> const log_record_entry*;

        /// @brief Returns the fixed ring buffer capacity.
        [[nodiscard]] auto capacity() const noexcept -> size_t
        {
            return _capacity;
        }

        /// @brief Returns the total number of retained log entries currently held in the ring buffer.
        [[nodiscard]] auto size() const noexcept -> size_t
        {
            return tempest::min(static_cast<size_t>(_next_sequence - _oldest_sequence), _capacity);
        }

        /// @brief Returns the number of visible log entries not obscured by clear watermarks.
        [[nodiscard]] auto visible_count() const noexcept -> size_t
        {
            return _next_sequence <= _cleared_sequence_watermark
                       ? 0
                       : static_cast<size_t>(_next_sequence -
                                             tempest::max(_oldest_sequence, _cleared_sequence_watermark));
        }

        /// @brief Returns the total cumulative count of dropped (overwritten) log entries.
        [[nodiscard]] auto dropped_count() const noexcept -> uint64_t
        {
            return _dropped_count;
        }

        /// @brief Returns the monotonic sequence number of the oldest retained entry.
        [[nodiscard]] auto oldest_sequence() const noexcept -> uint64_t
        {
            return _oldest_sequence;
        }

        /// @brief Returns the next sequence number to be assigned.
        [[nodiscard]] auto next_sequence() const noexcept -> uint64_t
        {
            return _next_sequence;
        }

        /// @brief Returns the current soft-cleared watermark sequence.
        [[nodiscard]] auto cleared_sequence() const noexcept -> uint64_t
        {
            return _cleared_sequence_watermark;
        }

        /// @brief Const reference to the current filter state and cached matches.
        [[nodiscard]] auto filter() const noexcept -> const log_filter_state&
        {
            return _filter;
        }

        /// @brief Mutable reference to the current filter state and cached matches.
        [[nodiscard]] auto filter() noexcept -> log_filter_state&
        {
            return _filter;
        }

      private:
        size_t _capacity = default_capacity;
        uint64_t _next_sequence = default_sequence;
        uint64_t _oldest_sequence = default_sequence;
        uint64_t _cleared_sequence_watermark = default_sequence;
        uint64_t _dropped_count = default_dropped_count;
        vector<log_record_entry> _entries{};
        log_filter_state _filter{};
    };
} // namespace tempest::editor

namespace tempest
{
    using editor::log_filter_state;
    using editor::log_record_entry;
    using editor::log_store;
} // namespace tempest

#endif // tempest_editor_core_log_store_hpp
