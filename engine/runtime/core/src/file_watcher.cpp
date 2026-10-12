#include <tempest/file_watcher.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/atomic.hpp>
#include <tempest/chrono.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/thread.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <poll.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace tempest
{
    class file_watcher_impl
    {
      public:
        explicit file_watcher_impl(file_watcher_options options);
        ~file_watcher_impl() noexcept;

        file_watcher_impl(const file_watcher_impl&) = delete;
        file_watcher_impl(file_watcher_impl&&) noexcept = delete;
        auto operator=(const file_watcher_impl&) -> file_watcher_impl& = delete;
        auto operator=(file_watcher_impl&&) noexcept -> file_watcher_impl& = delete;

        auto poll_events(vector<file_watch_event>& out_events) -> size_t;
        [[nodiscard]] auto poll_events() -> vector<file_watch_event>;

        [[nodiscard]] auto events() noexcept -> mpsc_queue<file_watch_event>&
        {
            return _events;
        }

        [[nodiscard]] auto events() const noexcept -> const mpsc_queue<file_watch_event>&
        {
            return _events;
        }

        [[nodiscard]] auto is_running() const noexcept -> bool
        {
            return _running.load(memory_order::acquire);
        }

        [[nodiscard]] auto watch_path() const noexcept -> const filesystem::path&
        {
            return _options.watch_path;
        }

        void stop() noexcept;

      private:
        struct pending_event_entry
        {
            filesystem::path path;
            filesystem::path old_path;
            file_watch_event_type type;
            chrono::steady_clock::time_point deadline;
        };

        void _worker_thread_func();
        void _add_or_update_pending(file_watch_event_type type, filesystem::path path, filesystem::path old_path);
        void _flush_expired_pending(chrono::steady_clock::time_point now);

        file_watcher_options _options;
        mpsc_queue<file_watch_event> _events;
        atomic<bool> _running{false};
        thread _worker_thread;

        // Thread-confined to _worker_thread
        vector<pending_event_entry> _pending_events;

#if defined(_WIN32)
        HANDLE _dir_handle{INVALID_HANDLE_VALUE};
        HANDLE _stop_event{nullptr};
        OVERLAPPED _overlapped{};
        vector<uint8_t> _buffer;
#elif defined(__linux__)
        int _inotify_fd{-1};
        int _stop_pipe[2]{-1, -1};
#endif
    };

    file_watcher_impl::file_watcher_impl(file_watcher_options options)
        : _options{tempest::move(options)}
    {
        if (_options.watch_path.empty() || !filesystem::exists(_options.watch_path) || !filesystem::is_directory(_options.watch_path))
        {
            return;
        }

#if defined(_WIN32)
        _dir_handle = CreateFileW(
            _options.watch_path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (_dir_handle == INVALID_HANDLE_VALUE)
        {
            return;
        }

        _stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        _overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        if (_stop_event == nullptr || _overlapped.hEvent == nullptr)
        {
            if (_stop_event != nullptr)
            {
                CloseHandle(_stop_event);
                _stop_event = nullptr;
            }
            if (_overlapped.hEvent != nullptr)
            {
                CloseHandle(_overlapped.hEvent);
                _overlapped.hEvent = nullptr;
            }
            CloseHandle(_dir_handle);
            _dir_handle = INVALID_HANDLE_VALUE;
            return;
        }

        static constexpr size_t read_buffer_size = 64 * 1024;
        _buffer.resize(read_buffer_size);
        _pending_events.reserve(256);
#elif defined(__linux__)
        _inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (_inotify_fd == -1)
        {
            return;
        }
        if (pipe2(_stop_pipe, O_NONBLOCK | O_CLOEXEC) != 0)
        {
            close(_inotify_fd);
            _inotify_fd = -1;
            return;
        }
        _pending_events.reserve(256);
#endif

        _running.store(true, memory_order::release);
        _worker_thread = thread{[this]() { _worker_thread_func(); }};
    }

    file_watcher_impl::~file_watcher_impl() noexcept
    {
        stop();
    }

    void file_watcher_impl::stop() noexcept
    {
        if (!_running.exchange(false, memory_order::acq_rel))
        {
            return;
        }

#if defined(_WIN32)
        if (_stop_event != nullptr)
        {
            SetEvent(_stop_event);
        }
        if (_dir_handle != INVALID_HANDLE_VALUE && _overlapped.hEvent != nullptr)
        {
            CancelIoEx(_dir_handle, &_overlapped);
        }
#elif defined(__linux__)
        if (_stop_pipe[1] != -1)
        {
            const char stop_byte = 1;
            write(_stop_pipe[1], &stop_byte, 1);
        }
#endif

        if (_worker_thread.joinable())
        {
            _worker_thread.join();
        }

#if defined(_WIN32)
        if (_overlapped.hEvent != nullptr)
        {
            CloseHandle(_overlapped.hEvent);
            _overlapped.hEvent = nullptr;
        }
        if (_stop_event != nullptr)
        {
            CloseHandle(_stop_event);
            _stop_event = nullptr;
        }
        if (_dir_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(_dir_handle);
            _dir_handle = INVALID_HANDLE_VALUE;
        }
#elif defined(__linux__)
        if (_stop_pipe[0] != -1)
        {
            close(_stop_pipe[0]);
            _stop_pipe[0] = -1;
        }
        if (_stop_pipe[1] != -1)
        {
            close(_stop_pipe[1]);
            _stop_pipe[1] = -1;
        }
        if (_inotify_fd != -1)
        {
            close(_inotify_fd);
            _inotify_fd = -1;
        }
#endif
    }

    auto file_watcher_impl::poll_events(vector<file_watch_event>& out_events) -> size_t
    {
        return _events.drain(out_events);
    }

    auto file_watcher_impl::poll_events() -> vector<file_watch_event>
    {
        return _events.drain();
    }

    void file_watcher_impl::_add_or_update_pending(
        file_watch_event_type type,
        filesystem::path path,
        filesystem::path old_path)
    {
        const auto now = chrono::steady_clock::now();
        const auto deadline = now + _options.debounce_interval;

        for (auto pending_it = _pending_events.begin(); pending_it != _pending_events.end(); ++pending_it)
        {
            if (pending_it->path == path)
            {
                pending_it->deadline = deadline;
                if (pending_it->type == file_watch_event_type::added)
                {
                    if (type == file_watch_event_type::removed)
                    {
                        _pending_events.erase(pending_it);
                        return;
                    }
                    // If modified, remains added
                }
                else if (pending_it->type == file_watch_event_type::modified)
                {
                    if (type == file_watch_event_type::removed)
                    {
                        pending_it->type = file_watch_event_type::removed;
                    }
                    // If modified, remains modified
                }
                else if (pending_it->type == file_watch_event_type::renamed)
                {
                    if (type == file_watch_event_type::removed)
                    {
                        pending_it->type = file_watch_event_type::removed;
                    }
                    // If modified, remains renamed
                }
                else if (pending_it->type == file_watch_event_type::removed)
                {
                    if (type == file_watch_event_type::added)
                    {
                        pending_it->type = file_watch_event_type::modified;
                    }
                }
                return;
            }
        }

        if (type == file_watch_event_type::renamed && !old_path.empty())
        {
            for (auto old_pending_it = _pending_events.begin(); old_pending_it != _pending_events.end(); ++old_pending_it)
            {
                if (old_pending_it->path == old_path)
                {
                    if (old_pending_it->type == file_watch_event_type::added)
                    {
                        old_pending_it->path = tempest::move(path);
                        old_pending_it->deadline = deadline;
                        return;
                    }
                    _pending_events.erase(old_pending_it);
                    break;
                }
            }
        }

        _pending_events.push_back(pending_event_entry{
            .path = tempest::move(path),
            .old_path = tempest::move(old_path),
            .type = type,
            .deadline = deadline,
        });
    }

    void file_watcher_impl::_flush_expired_pending(chrono::steady_clock::time_point now)
    {
        for (size_t event_idx = 0; event_idx < _pending_events.size();)
        {
            if (now >= _pending_events[event_idx].deadline)
            {
                _events.push(file_watch_event{
                    .type = _pending_events[event_idx].type,
                    .path = tempest::move(_pending_events[event_idx].path),
                    .old_path = tempest::move(_pending_events[event_idx].old_path),
                });
                _pending_events.erase(_pending_events.begin() + event_idx);
            }
            else
            {
                ++event_idx;
            }
        }
    }

#if defined(_WIN32)
    void file_watcher_impl::_worker_thread_func()
    {
        static constexpr DWORD notify_filter =
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_ATTRIBUTES |
            FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_CREATION;

        auto issue_read = [this]() -> bool {
            ResetEvent(_overlapped.hEvent);
            _overlapped.Internal = 0;
            _overlapped.InternalHigh = 0;
            _overlapped.Offset = 0;
            _overlapped.OffsetHigh = 0;

            DWORD bytes_returned = 0;
            const auto ok = ReadDirectoryChangesW(
                _dir_handle,
                _buffer.data(),
                static_cast<DWORD>(_buffer.size()),
                _options.recursive ? TRUE : FALSE,
                notify_filter,
                &bytes_returned,
                &_overlapped,
                nullptr
            );
            if (!ok && GetLastError() != ERROR_IO_PENDING)
            {
                return false;
            }
            return true;
        };

        if (!issue_read())
        {
            _running.store(false, memory_order::release);
            return;
        }

        HANDLE wait_handles[2] = {_stop_event, _overlapped.hEvent};
        optional<filesystem::path> pending_rename_old;

        while (_running.load(memory_order::acquire))
        {
            auto now = chrono::steady_clock::now();
            DWORD timeout_ms = INFINITE;

            if (!_pending_events.empty())
            {
                auto earliest = _pending_events.front().deadline;
                for (const auto& entry : _pending_events)
                {
                    if (entry.deadline < earliest)
                    {
                        earliest = entry.deadline;
                    }
                }

                if (earliest <= now)
                {
                    timeout_ms = 0;
                }
                else
                {
                    const auto diff = chrono::duration_cast<chrono::milliseconds>(earliest - now).count();
                    timeout_ms = static_cast<DWORD>(diff > 0 ? diff : 0);
                }
            }

            const auto wait_res = WaitForMultipleObjects(2, wait_handles, FALSE, timeout_ms);

            if (!_running.load(memory_order::acquire) || wait_res == WAIT_OBJECT_0)
            {
                break;
            }

            if (wait_res == WAIT_OBJECT_0 + 1)
            {
                DWORD bytes_transferred = 0;
                const auto result = GetOverlappedResult(_dir_handle, &_overlapped, &bytes_transferred, FALSE);

                if (!_running.load(memory_order::acquire))
                {
                    break;
                }

                if (result && bytes_transferred > 0)
                {
                    auto* record = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(_buffer.data());
                    for (;;)
                    {
                        const auto len = record->FileNameLength / sizeof(WCHAR);
                        const auto is_dot = (len == 1 && record->FileName[0] == L'.');
                        const auto is_dotdot = (len == 2 && record->FileName[0] == L'.' && record->FileName[1] == L'.');

                        if (len > 0 && !is_dot && !is_dotdot)
                        {
                            const auto rel_view = basic_string_view<wchar_t>(record->FileName, len);
                            auto full_path = _options.watch_path / filesystem::path(rel_view);

                            switch (record->Action)
                            {
                            case FILE_ACTION_ADDED:
                                _add_or_update_pending(file_watch_event_type::added, tempest::move(full_path), {});
                                break;
                            case FILE_ACTION_REMOVED:
                                _add_or_update_pending(file_watch_event_type::removed, tempest::move(full_path), {});
                                break;
                            case FILE_ACTION_MODIFIED:
                                _add_or_update_pending(file_watch_event_type::modified, tempest::move(full_path), {});
                                break;
                            case FILE_ACTION_RENAMED_OLD_NAME:
                                pending_rename_old = tempest::move(full_path);
                                break;
                            case FILE_ACTION_RENAMED_NEW_NAME:
                                if (pending_rename_old.has_value())
                                {
                                    _add_or_update_pending(file_watch_event_type::renamed, tempest::move(full_path), tempest::move(*pending_rename_old));
                                    pending_rename_old.reset();
                                }
                                else
                                {
                                    _add_or_update_pending(file_watch_event_type::added, tempest::move(full_path), {});
                                }
                                break;
                            default:
                                break;
                            }
                        }

                        if (record->NextEntryOffset == 0)
                        {
                            break;
                        }
                        record = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                            reinterpret_cast<const uint8_t*>(record) + record->NextEntryOffset);
                    }
                }

                if (!issue_read())
                {
                    break;
                }
            }
            else if (wait_res == WAIT_TIMEOUT)
            {
                // Timeout expired, will flush below
            }
            else
            {
                break;
            }

            _flush_expired_pending(chrono::steady_clock::now());
        }
    }
#elif defined(__linux__)
    void file_watcher_impl::_worker_thread_func()
    {
        const auto watch_mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO;
        const auto wd = inotify_add_watch(_inotify_fd, _options.watch_path.c_str(), watch_mask);
        if (wd == -1)
        {
            _running.store(false, memory_order::release);
            return;
        }

        struct pollfd fds[2];
        fds[0].fd = _stop_pipe[0];
        fds[0].events = POLLIN;
        fds[1].fd = _inotify_fd;
        fds[1].events = POLLIN;

        alignas(struct inotify_event) char buffer[4096];
        optional<filesystem::path> pending_rename_old;
        uint32_t pending_rename_cookie = 0;

        while (_running.load(memory_order::acquire))
        {
            auto now = chrono::steady_clock::now();
            int timeout_ms = -1;

            if (!_pending_events.empty())
            {
                auto earliest = _pending_events.front().deadline;
                for (const auto& entry : _pending_events)
                {
                    if (entry.deadline < earliest)
                    {
                        earliest = entry.deadline;
                    }
                }

                if (earliest <= now)
                {
                    timeout_ms = 0;
                }
                else
                {
                    const auto diff = chrono::duration_cast<chrono::milliseconds>(earliest - now).count();
                    timeout_ms = static_cast<int>(diff > 0 ? diff : 0);
                }
            }

            const auto ret = poll(fds, 2, timeout_ms);
            if (!_running.load(memory_order::acquire) || (fds[0].revents & POLLIN))
            {
                break;
            }

            if (ret > 0 && (fds[1].revents & POLLIN))
            {
                const auto bytes_read = read(_inotify_fd, buffer, sizeof(buffer));
                if (bytes_read > 0)
                {
                    ssize_t offset = 0;
                    while (offset < bytes_read)
                    {
                        auto* event = reinterpret_cast<struct inotify_event*>(&buffer[offset]);
                        if (event->len > 0)
                        {
                            auto full_path = _options.watch_path / filesystem::path(event->name);
                            if (event->mask & IN_CREATE)
                            {
                                _add_or_update_pending(file_watch_event_type::added, tempest::move(full_path), {});
                            }
                            else if (event->mask & IN_DELETE)
                            {
                                _add_or_update_pending(file_watch_event_type::removed, tempest::move(full_path), {});
                            }
                            else if (event->mask & IN_MODIFY)
                            {
                                _add_or_update_pending(file_watch_event_type::modified, tempest::move(full_path), {});
                            }
                            else if (event->mask & IN_MOVED_FROM)
                            {
                                pending_rename_old = tempest::move(full_path);
                                pending_rename_cookie = event->cookie;
                            }
                            else if (event->mask & IN_MOVED_TO)
                            {
                                if (pending_rename_old.has_value() && pending_rename_cookie == event->cookie)
                                {
                                    _add_or_update_pending(file_watch_event_type::renamed, tempest::move(full_path), tempest::move(*pending_rename_old));
                                    pending_rename_old.reset();
                                }
                                else
                                {
                                    _add_or_update_pending(file_watch_event_type::added, tempest::move(full_path), {});
                                }
                            }
                        }
                        offset += sizeof(struct inotify_event) + event->len;
                    }
                }
            }

            _flush_expired_pending(chrono::steady_clock::now());
        }
    }
#endif

    file_watcher::file_watcher(file_watcher_options options)
        : _impl{make_unique<file_watcher_impl>(tempest::move(options))}
    {
    }

    file_watcher::file_watcher(filesystem::path watch_path)
        : _impl{make_unique<file_watcher_impl>(file_watcher_options{
              .watch_path = tempest::move(watch_path),
          })}
    {
    }

    file_watcher::file_watcher(filesystem::path watch_path, bool recursive)
        : _impl{make_unique<file_watcher_impl>(file_watcher_options{
              .watch_path = tempest::move(watch_path),
              .recursive = recursive,
          })}
    {
    }

    file_watcher::file_watcher(file_watcher&& other) noexcept = default;

    auto file_watcher::operator=(file_watcher&& other) noexcept -> file_watcher& = default;

    file_watcher::~file_watcher() noexcept = default;

    auto file_watcher::poll_events(vector<file_watch_event>& out_events) -> size_t
    {
        return _impl ? _impl->poll_events(out_events) : 0;
    }

    auto file_watcher::poll_events() -> vector<file_watch_event>
    {
        return _impl ? _impl->poll_events() : vector<file_watch_event>{};
    }

    auto file_watcher::events() noexcept -> mpsc_queue<file_watch_event>&
    {
        return _impl->events();
    }

    auto file_watcher::events() const noexcept -> const mpsc_queue<file_watch_event>&
    {
        return _impl->events();
    }

    auto file_watcher::is_running() const noexcept -> bool
    {
        return _impl ? _impl->is_running() : false;
    }

    auto file_watcher::watch_path() const noexcept -> const filesystem::path&
    {
        static const filesystem::path empty_path{};
        return _impl ? _impl->watch_path() : empty_path;
    }
} // namespace tempest
