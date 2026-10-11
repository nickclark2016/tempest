#ifndef tempest_editor_core_editor_log_sink_hpp
#define tempest_editor_core_editor_log_sink_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/intrusive_stack.hpp>
#include <tempest/logger.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string_view.hpp>

namespace tempest::editor
{
    /// @brief Node representing an individual captured log record within the lock-free intrusive stack.
    struct TEMPEST_EDITOR_API log_entry_node : treiber_node<log_entry_node>
    {
        static constexpr log_level default_log_level = log_level::info;
        static constexpr uint64_t default_timestamp_ns = 0;
        static constexpr uint32_t default_thread_id = 0;
        static constexpr uint32_t default_message_length = 0;

        log_level level = default_log_level;
        uint64_t timestamp_ns = default_timestamp_ns;
        uint32_t thread_id = default_thread_id;
        source_location source{};
        uint32_t message_length = default_message_length;

        /// @brief Pointer to the inline message payload buffer allocated immediately following this node.
        [[nodiscard]] auto message_buffer() noexcept -> char*
        {
            return reinterpret_cast<char*>(this + 1);
        }

        /// @brief Const pointer to the inline message payload buffer.
        [[nodiscard]] auto message_buffer() const noexcept -> const char*
        {
            return reinterpret_cast<const char*>(this + 1);
        }

        /// @brief String view referencing the inline null-terminated message payload.
        [[nodiscard]] auto message() const noexcept -> string_view
        {
            return string_view{message_buffer(), message_length};
        }

        /// @brief Allocates and initializes a packed log entry node containing the record payload.
        /// @param record The source log record to capture.
        /// @return Pointer to the allocated node.
        static auto create(const log_record& record) -> log_entry_node*;

        /// @brief Destroys and deallocates a packed log entry node.
        /// @param node Pointer to the node to deallocate.
        static void destroy(log_entry_node* node) noexcept;
    };

    /// @brief Deleter functor for managed pointers to log_entry_node.
    struct log_entry_node_deleter
    {
        void operator()(log_entry_node* node) const noexcept
        {
            log_entry_node::destroy(node);
        }
    };

    /// @brief High-throughput, non-blocking MPSC log sink capturing engine logs for editor consumption.
    class TEMPEST_EDITOR_API editor_log_sink final : public log_sink
    {
      public:
        static constexpr uint64_t default_enqueued_count = 0;
        static constexpr logger* default_attached_logger = nullptr;

        editor_log_sink();
        editor_log_sink(log_level min_level, log_level max_level);
        explicit editor_log_sink(logger& log);
        editor_log_sink(logger& log, log_level min_level, log_level max_level);
        explicit editor_log_sink(logger* log);
        editor_log_sink(logger* log, log_level min_level, log_level max_level);

        ~editor_log_sink() override;

        editor_log_sink(const editor_log_sink&) = delete;
        editor_log_sink(editor_log_sink&&) noexcept = delete;
        auto operator=(const editor_log_sink&) -> editor_log_sink& = delete;
        auto operator=(editor_log_sink&&) noexcept -> editor_log_sink& = delete;

        /// @brief Attaches this sink to the target logger.
        auto attach(logger& log) -> void;

        /// @brief Detaches this sink from the currently attached logger, if any.
        auto detach() -> void;

        /// @brief Atomically drains all pending log nodes and returns them in chronological (FIFO) order.
        /// @return Pointer to head of the reversed singly linked list, or nullptr if empty.
        [[nodiscard]] auto drain_nodes() noexcept -> log_entry_node*;

        /// @brief Returns the total cumulative number of log entries enqueued to this sink.
        [[nodiscard]] auto enqueued_count() const noexcept -> uint64_t;

      protected:
        void do_log(const log_record& record) override;

      private:
        intrusive_mpsc_stack<log_entry_node> _pending_entries;
        atomic<uint64_t> _enqueued_count = default_enqueued_count;
        logger* _attached_logger = default_attached_logger;
    };
} // namespace tempest::editor

namespace tempest
{
    using editor::editor_log_sink;
    using editor::log_entry_node;
    using editor::log_entry_node_deleter;
} // namespace tempest

#endif // tempest_editor_core_editor_log_sink_hpp
