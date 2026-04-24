#ifndef tempest_logger_logger_hpp
#define tempest_logger_logger_hpp

#include <tempest/api.hpp>
#include <tempest/concepts.hpp>
#include <tempest/int.hpp>
#include <tempest/mutex.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

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

    class TEMPEST_API log_sink
    {
      public:
        explicit log_sink(log_level min_level = log_level::trace, log_level max_level = log_level::fatal);
        log_sink(const log_sink&) = delete;
        log_sink(log_sink&&) noexcept = delete;
        virtual ~log_sink() = default;

        log_sink& operator=(const log_sink&) = delete;
        log_sink& operator=(log_sink&&) noexcept = delete;

        void log(log_level level, string_view message, source_location location);

      protected:
        virtual void do_log(log_level level, string_view message, source_location location) = 0;

      private:
        log_level _min_level;
        log_level _max_level;
    };

    class TEMPEST_API stdout_log_sink : public log_sink
    {
      public:
        using log_sink::log_sink;

      protected:
        void do_log(log_level level, string_view message, source_location location) override;
    };

    class TEMPEST_API mt_stdout_log_sink final : public stdout_log_sink
    {
      public:
        using stdout_log_sink::stdout_log_sink;

      protected:
        void do_log(log_level level, string_view message, source_location location) override;

      private:
        mutex _mutex;
    };

    class TEMPEST_API logger
    {
      public:
        template <typename... Sinks>
            requires(derived_from<remove_cvref_t<Sinks>, log_sink> && ...)
        explicit logger(Sinks&&... sinks);

        explicit logger(span<log_sink*> sinks);

        void trace(string_view message, source_location location = source_location::current());
        void debug(string_view message, source_location location = source_location::current());
        void info(string_view message, source_location location = source_location::current());
        void warn(string_view message, source_location location = source_location::current());
        void error(string_view message, source_location location = source_location::current());
        void critical(string_view message, source_location location = source_location::current());
        void fatal(string_view message, source_location location = source_location::current());

      private:
        void do_log(log_level level, string_view message, source_location location);

        vector<log_sink*> _sinks;
    };

    template <typename... Sinks>
        requires(derived_from<remove_cvref_t<Sinks>, log_sink> && ...)
    logger::logger(Sinks&&... sinks)
    {
        // Store pointers to the sinks
        (_sinks.push_back(&sinks), ...);
    }
} // namespace tempest

#endif // tempest_logger_logger_hpp
