#ifndef tempest_core_source_location_hpp
#define tempest_core_source_location_hpp

#include <tempest/int.hpp>

namespace tempest
{
    struct source_location
    {
      private:
#if defined(__GNUC__)
        using builtin_ret_type = decltype(__builtin_source_location());
#endif
      public:
#if defined(_MSC_VER)
        static consteval source_location current(const uint32_t line = __builtin_LINE(),
                                                 const uint32_t column = __builtin_COLUMN(),
                                                 const char* file = __builtin_FILE(),
                                                 const char* func = __builtin_FUNCSIG()) noexcept;
#elif defined(__GNUC__)
        static consteval source_location current(builtin_ret_type loc = __builtin_source_location()) noexcept;
#else
#error "Unsupported compiler."
#endif

        constexpr source_location() noexcept = default;

        constexpr const char* file_name() const noexcept;
        constexpr const char* function_name() const noexcept;

        constexpr size_t line() const noexcept;
        constexpr size_t column() const noexcept;

      private:
        struct impl
        {
            const char* _file = nullptr;
            const char* _function = nullptr;
            uint32_t _line = 0;
            uint32_t _column = 0;
        };

#if defined(_MSC_VER)
        impl _impl;
#elif defined(__GNUC__)
        const impl* _impl;
#endif
    };

#if defined(_MSC_VER)
    source_location source_location::current(const uint32_t line, const uint32_t column, const char* file,
                                             const char* func) noexcept
    {
        source_location loc;
        loc._impl._file = file;
        loc._impl._function = func;
        loc._impl._line = line;
        loc._impl._column = column;
        return loc;
    }

    constexpr const char* source_location::file_name() const noexcept
    {
        return _impl._file;
    }

    constexpr const char* source_location::function_name() const noexcept
    {
        return _impl._function;
    }

    constexpr size_t source_location::line() const noexcept
    {
        return _impl._line;
    }

    constexpr size_t source_location::column() const noexcept
    {
        return _impl._column;
    }

#elif defined(__GNUC__)
    source_location source_location::current(builtin_ret_type loc) noexcept
    {
        source_location sl;
        sl._impl = static_cast<const impl*>(loc);
        return sl;
    }

    constexpr const char* source_location::file_name() const noexcept
    {
        return _impl->_file;
    }

    constexpr const char* source_location::function_name() const noexcept
    {
        return _impl->_function;
    }

    constexpr size_t source_location::line() const noexcept
    {
        return _impl->_line;
    }

    constexpr size_t source_location::column() const noexcept
    {
        return _impl->_column;
    }

#else
#error "Unsupported compiler."
#endif
} // namespace tempest

#endif // tempest_core_source_location_hpp
