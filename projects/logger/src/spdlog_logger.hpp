#ifndef tempest_spdlog_logger_hpp__
#define tempest_spdlog_logger_hpp__

#include <tempest/logger.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace tempest::logger::spd
{
	class spdlog_logger final : public ilogger
    {
      public:
        spdlog_logger(const logger_factory::create_info& info);
        ~spdlog_logger() override;
        void close() override;

      protected:

        void info_impl(std::string_view msg) override;
        void warn_impl(std::string_view msg) override;
        void debug_impl(std::string_view msg) override;
        void error_impl(std::string_view msg) override;
        void critical_impl(std::string_view msg) override;

      private:
        std::shared_ptr<spdlog::logger> _logger;
        std::string _prefix;
    };
} // namespace tempest::logger::spdlog

#endif // tempest_spdlog_logger_hpp__
