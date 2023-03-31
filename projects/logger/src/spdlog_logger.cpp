#include "spdlog_logger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <ctime>

namespace tempest::logger::spd
{
    spdlog_logger::spdlog_logger(const logger_factory::create_info& info)
    {
        _prefix = info.prefix;
        _logger = spdlog::stdout_color_mt(_prefix);

        std::time_t t = std::time(0);
        struct tm now;
        gmtime_s(&now, &t);

        std::ostringstream oss;
        oss << std::put_time(&now, "%y%m%d-%H%M%S");
        auto date = oss.str();

        _logger->set_level(spdlog::level::trace);
        _logger->flush_on(spdlog::level::trace);

        _logger->sinks().push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/log-" + date + ".txt"));

        // 0 == console, 1 == file
#if defined(_DEBUG)
        _logger->sinks()[0]->set_level(spdlog::level::info);
        _logger->sinks()[1]->set_level(spdlog::level::debug);
#else
        _logger->sinks()[0]->set_level(spdlog::level::critical);
        _logger->sinks()[1]->set_level(spdlog::level::warn);
#endif
    }

    spdlog_logger::~spdlog_logger()
    {
        close();
    }

    void spdlog_logger::info_impl(std::string_view msg)
    {
        _logger->info(msg);
    }

    void spdlog_logger::warn_impl(std::string_view msg)
    {
        _logger->warn(msg);
    }

    void spdlog_logger::debug_impl(std::string_view msg)
    {
        _logger->debug(msg);
    }

    void spdlog_logger::error_impl(std::string_view msg)
    {
        _logger->error(msg);
    }

    void spdlog_logger::critical_impl(std::string_view msg)
    {
        _logger->critical(msg);
    }

    void spdlog_logger::close()
    {
        _logger->flush();
    }
} // namespace tempest::logger::spd