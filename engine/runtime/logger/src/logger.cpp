#include <tempest/logger.hpp>

#include <iostream>

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
    } // namespace

    log_sink::log_sink(log_level min_level, log_level max_level) // NOLINT
        : _min_level(min_level), _max_level(max_level)
    {
    }

    void log_sink::log(log_level level, string_view message, source_location location)
    {
        if (level < _min_level || level > _max_level)
        {
            return;
        }

        do_log(level, message, location);
    }

    void stdout_log_sink::do_log(log_level level, string_view message, source_location location)
    {
        const auto level_str = log_level_to_string(level);
        std::cout << "[";
        std::cout.write(level_str.data(), static_cast<std::streamsize>(level_str.size()));
        std::cout << "]: ";
        std::cout.write(message.data(), static_cast<std::streamsize>(message.size()));

        // In the file location, ignore leading leading dots and slashes to improve readability
        auto file_name = string_view(location.file_name());
        for (size_t i = 0; i < file_name.size(); ++i)
        {
            if (file_name[i] != '.' && file_name[i] != '/' && file_name[i] != '\\')
            {
                file_name = substr(file_name, i, file_name.size() - i);
                break;
            }
        }

        std::cout << " (" << file_name.data() << ":" << location.line() << ")\n";
    }

    void mt_stdout_log_sink::do_log(log_level level, string_view message, source_location location)
    {
        unique_lock lock(_mutex);
        stdout_log_sink::do_log(level, message, location);
    }

    logger::logger(span<log_sink*> sinks)
    {
        for (auto* sink : sinks)
        {
            if (sink != nullptr)
            {
                _sinks.push_back(sink);
            }
        }
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
        for (auto* sink : _sinks)
        {
            sink->log(level, message, location);
        }
    }
} // namespace tempest
