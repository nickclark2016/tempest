#ifndef tempest_core_source_location_hpp
#define tempest_core_source_location_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>

namespace tempest
{
    struct TEMPEST_API source_location
    {
      public:
#if defined(_MSC_VER)
        static consteval auto current(uint32_t line = __builtin_LINE(), uint32_t column = __builtin_COLUMN(),
                                      const char* file = __builtin_FILE(),
                                      const char* func = __builtin_FUNCSIG()) noexcept -> source_location;
#elif defined(__GNUC__) || defined(__clang__)
        static consteval auto current(decltype(__builtin_source_location()) ptr = __builtin_source_location()) noexcept
            -> source_location;
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

#if defined(_MSC_VER)
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
#elif defined(__GNUC__) || defined(__clang__)
    consteval auto source_location::current(decltype(__builtin_source_location()) ptr) noexcept -> source_location
    {
        struct __builtin_source_location_layout
        {
            const char* _file_name;
            const char* _function_name;
            unsigned int _line;
            unsigned int _column;
        };

        const auto* data = static_cast<const __builtin_source_location_layout*>(ptr);
        source_location loc;
        loc._impl._file = data->_file_name;
        loc._impl._function = data->_function_name;
        loc._impl._line = data->_line;
        loc._impl._column = data->_column;
        return loc;
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
