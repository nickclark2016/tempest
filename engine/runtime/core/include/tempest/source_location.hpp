#ifndef tempest_core_source_location_hpp
#define tempest_core_source_location_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>

// If (GNUC or CLANG) and not (_GLIBCXX_SRCLOC or _LIBCPP_SOURCE_LOCATION)
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_GLIBCXX_SRCLOC) && !defined(_LIBCPP_SOURCE_LOCATION)
// Known UB. If libstdc++ or libc++ changes the layout of std::source_location::__impl, this will break
// TODO: Investigate a robust way to test for this. Maybe C++26 static reflection will provide a solution.
namespace std
{
    class TEMPEST_API source_location
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

        [[nodiscard]] constexpr auto file_name() const noexcept -> const char*
        {
            return _impl->_M_file_name;
        }

        [[nodiscard]] constexpr auto function_name() const noexcept -> const char*
        {
            return _impl->_M_function_name;
        }

        [[nodiscard]] constexpr auto line() const noexcept -> unsigned
        {
            return _impl->_M_line;
        }

        [[nodiscard]] constexpr auto column() const noexcept -> unsigned
        {
            return _impl->_M_column;
        }

        static consteval auto current(decltype(__builtin_source_location()) ptr = __builtin_source_location()) noexcept
            -> source_location
        {
            source_location loc;
            loc._impl = static_cast<const struct __impl*>(ptr);
            return loc;
        }
    };
} // namespace std

#endif

namespace tempest
{
    struct TEMPEST_API source_location
    {
      public:
#ifdef _MSC_VER
        static consteval auto current(uint32_t line = __builtin_LINE(), uint32_t column = __builtin_COLUMN(),
                                      const char* file = __builtin_FILE(),
                                      const char* func = __builtin_FUNCSIG()) noexcept -> source_location;
#elif defined(__GNUC__)
        static consteval source_location current(
            decltype(__builtin_source_location()) ptr = __builtin_source_location()) noexcept;
#else
#error "Unsupported compiler."
#endif

        constexpr source_location() noexcept = default;

        [[nodiscard]] constexpr auto file_name() const noexcept -> const char*;
        [[nodiscard]] constexpr auto function_name() const noexcept -> const char*;

        [[nodiscard]] constexpr auto line() const noexcept -> size_t;
        [[nodiscard]] constexpr auto column() const noexcept -> size_t;

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

#ifdef _MSC_VER
    consteval auto source_location::current(const uint32_t line, const uint32_t column, const char* file,
                                            const char* func) noexcept -> source_location
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

    constexpr auto source_location::file_name() const noexcept -> const char*
    {
        return _impl._file;
    }

    constexpr auto source_location::function_name() const noexcept -> const char*
    {
        return _impl._function;
    }

    constexpr auto source_location::line() const noexcept -> size_t
    {
        return _impl._line;
    }

    constexpr auto source_location::column() const noexcept -> size_t
    {
        return _impl._column;
    }
} // namespace tempest

#endif // tempest_core_source_location_hpp
