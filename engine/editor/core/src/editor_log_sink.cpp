#include <tempest/editor_log_sink.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/assert.hpp>
#include <tempest/checked.hpp>
#include <tempest/memory.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    auto log_entry_node::create(const log_record& record) -> log_entry_node*
    {
        const auto message_bytes = record.message.size();
        const auto total_bytes = sizeof(log_entry_node) + message_bytes + 1;
        auto* const memory_buffer = tempest::aligned_alloc(total_bytes, alignof(log_entry_node));
        TEMPEST_ASSERT(memory_buffer != nullptr);

        auto* const entry_node = tempest::construct_at(static_cast<log_entry_node*>(memory_buffer));
        entry_node->level = record.level;
        entry_node->timestamp_ns = record.timestamp_ns;
        entry_node->thread_id = record.thread_id;
        entry_node->source = record.source;
        entry_node->message_length = static_cast<uint32_t>(message_bytes);

        if (message_bytes > 0)
        {
            tempest::memcpy(entry_node->message_buffer(), record.message.data(), message_bytes);
        }
        entry_node->message_buffer()[message_bytes] = '\0';

        return entry_node;
    }

    void log_entry_node::destroy(log_entry_node* node) noexcept
    {
        if (node != nullptr)
        {
            tempest::destroy_at(node);
            tempest::aligned_free(node);
        }
    }

    editor_log_sink::editor_log_sink()
        : log_sink{log_level::trace, log_level::fatal}
    {
    }

    editor_log_sink::editor_log_sink(log_level min_level, log_level max_level)
        : log_sink{min_level, max_level}
    {
    }

    editor_log_sink::editor_log_sink(logger& log)
        : log_sink{log_level::trace, log_level::fatal}
    {
        attach(log);
    }

    editor_log_sink::editor_log_sink(logger& log, log_level min_level, log_level max_level)
        : log_sink{min_level, max_level}
    {
        attach(log);
    }

    editor_log_sink::editor_log_sink(logger* log)
        : log_sink{log_level::trace, log_level::fatal}
    {
        if (log != nullptr)
        {
            attach(*log);
        }
    }

    editor_log_sink::editor_log_sink(logger* log, log_level min_level, log_level max_level)
        : log_sink{min_level, max_level}
    {
        if (log != nullptr)
        {
            attach(*log);
        }
    }

    editor_log_sink::~editor_log_sink()
    {
        detach();

        auto* current_node = _pending_entries.drain();
        while (current_node != nullptr)
        {
            auto* const next_node = intrusive_mpsc_stack<log_entry_node>::next(current_node);
            log_entry_node::destroy(current_node);
            current_node = next_node;
        }
    }

    auto editor_log_sink::attach(logger& log) -> void
    {
        if (_attached_logger == &log)
        {
            return;
        }
        if (_attached_logger != nullptr)
        {
            _attached_logger->remove_sink(*this);
        }
        _attached_logger = &log;
        _attached_logger->add_sink(*this);
    }

    auto editor_log_sink::detach() -> void
    {
        if (_attached_logger != nullptr)
        {
            _attached_logger->remove_sink(*this);
            _attached_logger = nullptr;
        }
    }

    void editor_log_sink::do_log(const log_record& record)
    {
        auto* const entry_node = log_entry_node::create(record);
        _pending_entries.push(entry_node);
        _enqueued_count.fetch_add(1, memory_order::relaxed);
    }

    auto editor_log_sink::drain_nodes() noexcept -> log_entry_node*
    {
        auto* const head_node = _pending_entries.drain();
        return intrusive_mpsc_stack<log_entry_node>::reverse(head_node);
    }

    auto editor_log_sink::enqueued_count() const noexcept -> uint64_t
    {
        return _enqueued_count.load(memory_order::relaxed);
    }
} // namespace tempest::editor
