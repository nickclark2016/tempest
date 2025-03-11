#ifndef tempest_core_source_location_hpp
#define tempest_core_source_location_hpp

#include <tempest/int.hpp>

// Check for GLIBCXX or LIBCPP
#if ((defined(__GLIBCXX__) && !defined(_GLIBCXX_SRCLOC)) ||                                                            \
     (defined(_LIBCPP_VERSION) && !defined(_LIBCPP_SOURCE_LOCATION))) &&                                               \
    (defined(__GNUC__) || defined(__clang__))

// Known UB. If libstdc++ or libc++ changes the layout of std::source_location::__impl, this will break
// TODO: Investigate a robust way to test for this. Maybe C++26 static reflection will provide a solution.
namespace std
{
    class source_location
    {
        struct __impl
        {
            const char* _M_file_name = nullptr;
            const char* _M_function_name = nullptr;
            unsigned _M_line = 0;
            unsigned _M_column = 0;
        };

      public:
        const __impl* _impl;

        constexpr source_location() noexcept = default;

        constexpr const char* file_name() const noexcept
        {
            return _impl->_M_file_name;
        }

        constexpr const char* function_name() const noexcept
        {
            return _impl->_M_function_name;
        }

        constexpr size_t line() const noexcept
        {
            return _impl->_M_line;
        }

        constexpr size_t column() const noexcept
        {
            return _impl->_M_column;
        }

        static consteval source_location current(
            decltype(__builtin_source_location()) ptr = __builtin_source_location()) noexcept
        {
            source_location loc;
            loc._impl = static_cast<const __impl*>(ptr);
            return loc;
        }
    };
} // namespace std

#endif

namespace tempest
{
    struct source_location
    {
      public:
#if defined(_MSC_VER)
        static consteval source_location current(const uint32_t line = __builtin_LINE(),
                                                 const uint32_t column = __builtin_COLUMN(),
                                                 const char* file = __builtin_FILE(),
                                                 const char* func = __builtin_FUNCSIG()) noexcept;
#elif defined(__GNUC__)
        static consteval source_location current(
            decltype(__builtin_source_location()) ptr = __builtin_source_location()) noexcept;
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

        impl _impl;
    };

#if defined(_MSC_VER)
    inline consteval source_location source_location::current(const uint32_t line, const uint32_t column,
                                                              const char* file, const char* func) noexcept
    {
        source_location loc;
        loc._impl._file = file;
        loc._impl._function = func;
        loc._impl._line = line;
        loc._impl._column = column;
        return loc;
    }
#else
    inline consteval source_location source_location::current(decltype(__builtin_source_location()) ptr) noexcept
    {
        auto loc = std::source_location::current(ptr);
        source_location result;
        result._impl._file = loc.file_name();
        result._impl._function = loc.function_name();
        result._impl._line = loc.line();
        result._impl._column = loc.column();
        return result;
    }
#endif

    inline constexpr const char* source_location::file_name() const noexcept
    {
        return _impl._file;
    }

    inline constexpr const char* source_location::function_name() const noexcept
    {
        return _impl._function;
    }

    inline constexpr size_t source_location::line() const noexcept
    {
        return _impl._line;
    }

    inline constexpr size_t source_location::column() const noexcept
    {
        return _impl._column;
    }
} // namespace tempest

#endif // tempest_core_source_location_hpp
