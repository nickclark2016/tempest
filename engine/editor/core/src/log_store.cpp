#include <tempest/log_store.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/intrusive_stack.hpp>

namespace tempest::editor
{
    namespace
    {
        [[nodiscard]] constexpr auto to_lower_ascii(char character) noexcept -> char
        {
            return (character >= 'A' && character <= 'Z') ? static_cast<char>(character + ('a' - 'A')) : character;
        }

        [[nodiscard]] auto case_insensitive_contains(string_view haystack, string_view needle) noexcept -> bool
        {
            if (needle.empty())
            {
                return true;
            }

            if (haystack.size() < needle.size())
            {
                return false;
            }

            const auto max_offset = haystack.size() - needle.size();
            for (size_t offset = 0; offset <= max_offset; ++offset)
            {
                auto matches = true;
                for (size_t character_idx = 0; character_idx < needle.size(); ++character_idx)
                {
                    if (to_lower_ascii(haystack[offset + character_idx]) != to_lower_ascii(needle[character_idx]))
                    {
                        matches = false;
                        break;
                    }
                }

                if (matches)
                {
                    return true;
                }
            }

            return false;
        }
    } // namespace

    auto log_filter_state::set_level_enabled(log_level level, bool enabled) noexcept -> void
    {
        const auto bit_index = static_cast<uint32_t>(level);
        if (enabled)
        {
            level_mask |= (1u << bit_index);
        }
        else
        {
            level_mask &= ~(1u << bit_index);
        }
    }

    auto log_filter_state::is_level_enabled(log_level level) const noexcept -> bool
    {
        const auto bit_index = static_cast<uint32_t>(level);
        return (level_mask & (1u << bit_index)) != 0;
    }

    auto log_filter_state::matches(const log_record_entry& entry) const -> bool
    {
        if (!is_level_enabled(entry.level()))
        {
            return false;
        }

        if (search_query.empty())
        {
            return true;
        }

        const auto query_view = string_view{search_query.data(), search_query.size()};
        if (case_insensitive_contains(entry.message(), query_view))
        {
            return true;
        }

        const auto* const file_name = entry.source().file_name();
        if (file_name != nullptr && case_insensitive_contains(string_view{file_name}, query_view))
        {
            return true;
        }

        return false;
    }

    auto log_filter_state::counter_for_level(log_level level) noexcept -> size_t*
    {
        switch (level)
        {
        case log_level::trace:
            return &trace_count;
        case log_level::debug:
            return &debug_count;
        case log_level::info:
            return &info_count;
        case log_level::warn:
            return &warn_count;
        case log_level::error:
            return &error_count;
        case log_level::critical:
            return &critical_count;
        case log_level::fatal:
            return &fatal_count;
        }
        return nullptr;
    }

    auto log_filter_state::counter_for_level(log_level level) const noexcept -> const size_t*
    {
        switch (level)
        {
        case log_level::trace:
            return &trace_count;
        case log_level::debug:
            return &debug_count;
        case log_level::info:
            return &info_count;
        case log_level::warn:
            return &warn_count;
        case log_level::error:
            return &error_count;
        case log_level::critical:
            return &critical_count;
        case log_level::fatal:
            return &fatal_count;
        }
        return nullptr;
    }

    auto log_filter_state::evaluate_and_append(const log_record_entry& entry) -> void
    {
        ++total_count;

        if (auto* const counter = counter_for_level(entry.level()); counter != nullptr)
        {
            ++(*counter);
        }

        if (matches(entry))
        {
            matched_sequences.push_back(entry.sequence_number);
        }
    }

    auto log_filter_state::decrement_counter(log_level level) noexcept -> void
    {
        if (total_count > 0)
        {
            --total_count;
        }

        if (auto* const counter = counter_for_level(level); counter != nullptr && *counter > 0)
        {
            --(*counter);
        }
    }

    auto log_filter_state::trim_evicted(uint64_t oldest_active_sequence) -> void
    {
        if (matched_sequences.empty())
        {
            return;
        }

        const auto bound_it =
            tempest::lower_bound(matched_sequences.begin(), matched_sequences.end(), oldest_active_sequence);
        if (bound_it != matched_sequences.begin())
        {
            matched_sequences.erase(matched_sequences.begin(), bound_it);
        }
    }

    auto log_filter_state::reset_matches_and_counters() -> void
    {
        matched_sequences.clear();
        total_count = default_count;
        trace_count = default_count;
        debug_count = default_count;
        info_count = default_count;
        warn_count = default_count;
        error_count = default_count;
        critical_count = default_count;
        fatal_count = default_count;
    }

    log_store::log_store(size_t capacity)
        : _capacity(tempest::max(size_t{1}, capacity))
    {
        _entries.resize(_capacity);
    }

    auto log_store::drain(editor_log_sink& sink) -> size_t
    {
        auto* current_node = sink.drain_nodes();
        if (current_node == nullptr)
        {
            return 0;
        }

        auto drained_count = size_t{0};

        while (current_node != nullptr)
        {
            auto* const next_node = intrusive_mpsc_stack<log_entry_node>::next(current_node);

            const auto seq = _next_sequence++;
            const auto slot = seq % _capacity;

            if (_entries[slot].node != nullptr)
            {
                if (_entries[slot].sequence_number >= _cleared_sequence_watermark)
                {
                    _filter.decrement_counter(_entries[slot].level());
                }
                log_entry_node::destroy(_entries[slot].node);
                _entries[slot].node = nullptr;
                ++_dropped_count;
                _oldest_sequence = seq - _capacity + 1;
            }

            _entries[slot].node = current_node;
            _entries[slot].sequence_number = seq;

            if (seq >= _cleared_sequence_watermark)
            {
                _filter.evaluate_and_append(_entries[slot]);
            }

            current_node = next_node;
            ++drained_count;
        }

        if (_oldest_sequence > 0)
        {
            _filter.trim_evicted(tempest::max(_oldest_sequence, _cleared_sequence_watermark));
        }

        return drained_count;
    }

    auto log_store::clear() -> void
    {
        _cleared_sequence_watermark = _next_sequence;
        _filter.reset_matches_and_counters();
    }

    auto log_store::set_filter_level(log_level level, bool enabled) -> void
    {
        _filter.set_level_enabled(level, enabled);
        rebuild_filter();
    }

    auto log_store::set_filter_search(string_view query) -> void
    {
        _filter.search_query.assign(query.data(), query.size());
        rebuild_filter();
    }

    auto log_store::rebuild_filter() -> void
    {
        _filter.reset_matches_and_counters();
        const auto start_seq = tempest::max(_oldest_sequence, _cleared_sequence_watermark);
        for (auto seq = start_seq; seq < _next_sequence; ++seq)
        {
            const auto slot = seq % _capacity;
            _filter.evaluate_and_append(_entries[slot]);
        }
    }

    auto log_store::get_entry(uint64_t sequence) const noexcept -> const log_record_entry*
    {
        if (sequence < _oldest_sequence || sequence >= _next_sequence || sequence < _cleared_sequence_watermark)
        {
            return nullptr;
        }

        return &_entries[sequence % _capacity];
    }
} // namespace tempest::editor
