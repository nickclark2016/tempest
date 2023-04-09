#ifndef tempest_logger_hpp
#define tempest_logger_hpp

#include <memory>
#include <string_view>
#include <format>

namespace tempest::logger
{
    class ilogger
    {
      public:
        virtual ~ilogger() = default;

        virtual void close() = 0;

        template <typename... Args> 
        void info(std::string_view msg, Args&&... args)
        {
            info_impl(std::vformat(msg, std::make_format_args(args...)));
        }

        template <typename... Args> 
        void warn(std::string_view msg, Args&&... args)
        {
            warn_impl(std::vformat(msg, std::make_format_args(args...)));
        }

        template <typename... Args> 
        void debug(std::string_view msg, Args&&... args)
        {
            debug_impl(std::vformat(msg, std::make_format_args(args...)));
        }

        template <typename... Args> 
        void error(std::string_view msg, Args&&... args)
        {
            error_impl(std::vformat(msg, std::make_format_args(args...)));
        }

        template <typename... Args> 
        void critical(std::string_view msg, Args&&... args)
        {
            critical_impl(std::vformat(msg, std::make_format_args(args...)));
        }

      protected:
        virtual void info_impl(std::string_view msg) = 0;
        virtual void warn_impl(std::string_view msg) = 0;
        virtual void debug_impl(std::string_view msg) = 0;
        virtual void error_impl(std::string_view msg) = 0;
        virtual void critical_impl(std::string_view msg) = 0;
    };

    class logger_factory
    {
      public:
        struct create_info
        {
            std::string prefix;
        };

        static std::unique_ptr<ilogger> create(const create_info& info);
    };
} // namespace tempest::logger

#endif // tempest_logger_hpp