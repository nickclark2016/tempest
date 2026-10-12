#ifndef tempest_core_file_watcher_hpp
#define tempest_core_file_watcher_hpp

#include <tempest/api.hpp>
#include <tempest/chrono.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/mutex.hpp>
#include <tempest/vector.hpp>

namespace tempest
{
    /// @brief Categorized filesystem watch event types.
    enum class file_watch_event_type : uint8_t
    {
        added,
        modified,
        removed,
        renamed,
    };

    /// @brief Payload describing a filesystem mutation event.
    struct file_watch_event
    {
        static constexpr file_watch_event_type default_type = file_watch_event_type::modified;

        file_watch_event_type type = default_type;
        filesystem::path path{};
        filesystem::path old_path{};
    };

    /// @brief Configuration options for file_watcher initialization.
    struct file_watcher_options
    {
        static constexpr chrono::milliseconds default_debounce_interval{250};
        static constexpr bool default_recursive = true;

        filesystem::path watch_path{};
        chrono::milliseconds debounce_interval = default_debounce_interval;
        bool recursive = default_recursive;
    };

    /// @brief Thread-safe multi-producer, single-consumer event queue.
    template <typename T>
    class mpsc_queue
    {
      public:
        mpsc_queue() = default;
        ~mpsc_queue() = default;

        mpsc_queue(const mpsc_queue&) = delete;
        auto operator=(const mpsc_queue&) -> mpsc_queue& = delete;

        mpsc_queue(mpsc_queue&& other) noexcept
        {
            lock_guard<mutex> lock{other._mutex};
            _items = tempest::move(other._items);
        }

        auto operator=(mpsc_queue&& other) noexcept -> mpsc_queue&
        {
            if (this != &other)
            {
                lock_guard<mutex> lock_this{_mutex};
                lock_guard<mutex> lock_other{other._mutex};
                _items = tempest::move(other._items);
            }
            return *this;
        }

        /// @brief Pushes a value to the back of the queue.
        /// @param item The value to append.
        void push(T item)
        {
            lock_guard<mutex> lock{_mutex};
            _items.push_back(tempest::move(item));
        }

        /// @brief Atomically drains all queued items into the destination vector.
        /// @param out Destination vector to append items into.
        /// @return Number of items drained.
        auto drain(vector<T>& out) -> size_t
        {
            lock_guard<mutex> lock{_mutex};
            if (_items.empty())
            {
                return 0;
            }
            const auto count = _items.size();
            for (auto& item : _items)
            {
                out.push_back(tempest::move(item));
            }
            _items.clear();
            return count;
        }

        /// @brief Atomically drains and returns all queued items.
        /// @return Vector containing all drained items.
        [[nodiscard]] auto drain() -> vector<T>
        {
            auto result = vector<T>{};
            drain(result);
            return result;
        }

        /// @brief Checks whether the queue is empty.
        [[nodiscard]] auto empty() const -> bool
        {
            lock_guard<mutex> lock{_mutex};
            return _items.empty();
        }

        /// @brief Returns the current count of items in the queue.
        [[nodiscard]] auto size() const -> size_t
        {
            lock_guard<mutex> lock{_mutex};
            return _items.size();
        }

      private:
        mutable mutex _mutex;
        vector<T> _items;
    };

    class file_watcher_impl;

    /// @brief Cross-platform RAII background directory watcher with debounced event coalescing.
    class TEMPEST_API file_watcher
    {
      public:
        explicit file_watcher(file_watcher_options options);
        explicit file_watcher(filesystem::path watch_path);
        file_watcher(filesystem::path watch_path, bool recursive);

        file_watcher(const file_watcher&) = delete;
        file_watcher(file_watcher&& other) noexcept;

        auto operator=(const file_watcher&) -> file_watcher& = delete;
        auto operator=(file_watcher&& other) noexcept -> file_watcher&;

        ~file_watcher() noexcept;

        /// @brief Polls and drains debounced file events into the provided output vector.
        /// @param out_events Output vector to receive drained events.
        /// @return Number of events drained.
        auto poll_events(vector<file_watch_event>& out_events) -> size_t;

        /// @brief Polls and returns all pending debounced file events.
        /// @return Vector of events drained.
        [[nodiscard]] auto poll_events() -> vector<file_watch_event>;

        /// @brief Direct reference to the internal thread-safe event queue.
        [[nodiscard]] auto events() noexcept -> mpsc_queue<file_watch_event>&;

        /// @brief Const reference to the internal thread-safe event queue.
        [[nodiscard]] auto events() const noexcept -> const mpsc_queue<file_watch_event>&;

        /// @brief Checks whether the background watcher thread is active and monitoring.
        [[nodiscard]] auto is_running() const noexcept -> bool;

        /// @brief Returns the root directory path being watched.
        [[nodiscard]] auto watch_path() const noexcept -> const filesystem::path&;

      private:
        unique_ptr<file_watcher_impl> _impl;
    };
} // namespace tempest

#endif // tempest_core_file_watcher_hpp
