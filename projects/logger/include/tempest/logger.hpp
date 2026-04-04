#ifndef tempest_logger_logger_hpp
#define tempest_logger_logger_hpp

#include <tempest/int.hpp>

namespace tempest
{
    enum class log_level : uint8_t
    {
        trace,
        debug,
        info,
        warn,
        error,
        critical,
        fatal,
    };

    class logger
    {
        public:
        private:
            logger* _parent{nullptr};
    };
}

#endif // tempest_logger_logger_hpp
