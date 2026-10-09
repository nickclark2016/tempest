#include <tempest/logger.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/chrono.hpp>
#include <tempest/format.hpp>
#include <tempest/print.hpp>
#include <tempest/thread.hpp>

namespace tempest
{
    namespace
    {
        // Convert log_level to string
        [[nodiscard]] auto log_level_to_string(log_level level) -> string_view
        {
            switch (level)
            {
            case log_level::trace:
                return "TRACE";
            case log_level::debug:
                return "DEBUG";
            case log_level::info:
                return "INFO";
            case log_level::warn:
                return "WARN";
            case log_level::error:
                return "ERROR";
            case log_level::critical:
                return "CRITICAL";
            case log_level::fatal:
                return "FATAL";
            default:
                return "UNKNOWN";
            }
        }

        [[nodiscard]] auto trim_source_path(string_view file_name) noexcept -> string_view
        {
            for (size_t i = 0; i < file_name.size(); ++i)
            {
                if (file_name[i] != '.' && file_name[i] != '/' && file_name[i] != '\\')
                {
                    return substr(file_name, i, file_name.size() - i);
                }
            }
            return file_name;
        }
    } // namespace

    namespace detail
    {
        auto capture_wall_clock_base() noexcept -> int64_t
        {
            return chrono::duration_cast<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count();
        }
    } // namespace detail

    log_sink::log_sink(log_level min_level, log_level max_level) // NOLINT
        : _min_level(min_level), _max_level(max_level)
    {
    }

    void log_sink::log(const log_record& record)
    {
        if (record.level < _min_level || record.level > _max_level)
        {
            return;
        }

        do_log(record);
    }

    void log_sink::log(log_level level, string_view message, source_location location)
    {
        const auto record = log_record{
            .level = level,
            .timestamp_ns = static_cast<uint64_t>(chrono::steady_clock::now().time_since_epoch().count()),
            .thread_id = static_cast<uint32_t>(this_thread::get_id().to_uint64()),
            .message = message,
            .source = location,
        };

        log(record);
    }

    void stdout_log_sink::do_log(const log_record& record)
    {
        const auto level_str = log_level_to_string(record.level);
        const auto file_name = trim_source_path(record.source.file_name());
        auto line = format("[{}]: {} ({}:{})\n", level_str, record.message, file_name, record.source.line());
        write_stdout(line);
    }

    void mt_stdout_log_sink::do_log(const log_record& record)
    {
        unique_lock lock(_mutex);
        stdout_log_sink::do_log(record);
    }

    logger::logger() : _wall_clock_base{detail::capture_wall_clock_base()}
    {
    }

    logger::logger(logger&& other) noexcept
    {
        // NOLINTBEGIN
        // other's lock must be acquired before moving its resources, preventing the usage of member initializer list
        unique_lock lock(other._mutex);
        _sinks = tempest::move(other._sinks);
        _wall_clock_base = other._wall_clock_base;
        // NOLINTEND
    }

    auto logger::operator=(logger&& other) noexcept -> logger&
    {
        if (this == &other)
        {
            return *this;
        }

        unique_lock lock_this(_mutex, defer_lock);
        unique_lock lock_other(other._mutex, defer_lock);
        if (this < &other)
        {
            lock_this.lock();
            lock_other.lock();
        }
        else
        {
            lock_other.lock();
            lock_this.lock();
        }

        _sinks = tempest::move(other._sinks);
        _wall_clock_base = other._wall_clock_base;
        return *this;
    }

    logger::logger(span<log_sink*> sinks) : _wall_clock_base{detail::capture_wall_clock_base()}
    {
        for (auto* sink : sinks)
        {
            if (sink != nullptr)
            {
                _sinks.push_back(sink);
            }
        }
    }

    void logger::add_sink(log_sink& sink)
    {
        unique_lock lock(_mutex);
        auto *const iter = tempest::find(_sinks.begin(), _sinks.end(), &sink);
        if (iter == _sinks.end())
        {
            _sinks.push_back(&sink);
        }
    }

    void logger::remove_sink(log_sink& sink)
    {
        unique_lock lock(_mutex);
        auto *const iter = tempest::find(_sinks.begin(), _sinks.end(), &sink);
        if (iter != _sinks.end())
        {
            _sinks.erase(iter);
        }
    }

    auto logger::wall_clock_base() const noexcept -> int64_t
    {
        return _wall_clock_base;
    }

    void logger::trace(string_view message, source_location location)
    {
        do_log(log_level::trace, message, location);
    }

    void logger::debug(string_view message, source_location location)
    {
        do_log(log_level::debug, message, location);
    }

    void logger::info(string_view message, source_location location)
    {
        do_log(log_level::info, message, location);
    }

    void logger::warn(string_view message, source_location location)
    {
        do_log(log_level::warn, message, location);
    }

    void logger::error(string_view message, source_location location)
    {
        do_log(log_level::error, message, location);
    }

    void logger::critical(string_view message, source_location location)
    {
        do_log(log_level::critical, message, location);
    }

    void logger::fatal(string_view message, source_location location)
    {
        do_log(log_level::fatal, message, location);
    }

    void logger::do_log(log_level level, string_view message, source_location location)
    {
        const auto record = log_record{
            .level = level,
            .timestamp_ns = static_cast<uint64_t>(chrono::steady_clock::now().time_since_epoch().count()),
            .thread_id = static_cast<uint32_t>(this_thread::get_id().to_uint64()),
            .message = message,
            .source = location,
        };

        shared_lock lock(_mutex);
        for (auto* sink : _sinks)
        {
            if (sink != nullptr)
            {
                sink->log(record);
            }
        }
    }
} // namespace tempest
