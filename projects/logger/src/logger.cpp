#include <tempest/logger.hpp>
#include "spdlog_logger.hpp"

namespace tempest::logger
{
    std::unique_ptr<ilogger> logger_factory::create(const create_info& info)
    {
        return std::make_unique<spd::spdlog_logger>(info);
    }
} // namespace tempest::logger