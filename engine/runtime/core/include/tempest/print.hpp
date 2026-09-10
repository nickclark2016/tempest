#ifndef tempest_core_print_hpp
#define tempest_core_print_hpp

#include <tempest/api.hpp>
#include <tempest/concepts.hpp>
#include <tempest/format.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    struct stdout_stream_t
    {
    };

    struct stderr_stream_t
    {
    };

    inline constexpr stdout_stream_t stdout_stream{};
    inline constexpr stderr_stream_t stderr_stream{};

    TEMPEST_API auto write_stdout(string_view text) -> void;
    TEMPEST_API auto write_stderr(string_view text) -> void;

    template <typename T>
    concept writable_target = requires(T& target, string_view text) { target.write(text); };

    template <typename... Args>
    auto print(format_string<type_identity_t<Args>...> fmt, Args&&... args) -> void
    {
        auto str = format(fmt, tempest::forward<Args>(args)...);
        write_stdout(string_view(str));
    }

    template <typename... Args>
    auto println(format_string<type_identity_t<Args>...> fmt, Args&&... args) -> void
    {
        auto str = format(fmt, tempest::forward<Args>(args)...);
        str.push_back('\n');
        write_stdout(string_view(str));
    }

    inline auto println() -> void
    {
        write_stdout("\n");
    }

    // --- print_to overloads ---
    template <typename... Args>
    auto print_to(stdout_stream_t, format_string<type_identity_t<Args>...> fmt, Args&&... args) -> void
    {
        auto str = format(fmt, tempest::forward<Args>(args)...);
        write_stdout(string_view(str));
    }

    template <typename... Args>
    auto print_to(stderr_stream_t, format_string<type_identity_t<Args>...> fmt, Args&&... args) -> void
    {
        auto str = format(fmt, tempest::forward<Args>(args)...);
        write_stderr(string_view(str));
    }

    template <writable_target Target, typename... Args>
    auto print_to(Target& dest, format_string<type_identity_t<Args>...> fmt, Args&&... args) -> void
    {
        auto str = format(fmt, tempest::forward<Args>(args)...);
        dest.write(string_view(str));
    }

    // --- println_to overloads with format string ---
    template <typename... Args>
    auto println_to(stdout_stream_t, format_string<type_identity_t<Args>...> fmt, Args&&... args) -> void
    {
        auto str = format(fmt, tempest::forward<Args>(args)...);
        str.push_back('\n');
        write_stdout(string_view(str));
    }

    template <typename... Args>
    auto println_to(stderr_stream_t, format_string<type_identity_t<Args>...> fmt, Args&&... args) -> void
    {
        auto str = format(fmt, tempest::forward<Args>(args)...);
        str.push_back('\n');
        write_stderr(string_view(str));
    }

    template <writable_target Target, typename... Args>
    auto println_to(Target& dest, format_string<type_identity_t<Args>...> fmt, Args&&... args) -> void
    {
        auto str = format(fmt, tempest::forward<Args>(args)...);
        str.push_back('\n');
        dest.write(string_view(str));
    }

    // --- println_to newline-only overloads ---
    inline auto println_to(stdout_stream_t) -> void
    {
        write_stdout("\n");
    }

    inline auto println_to(stderr_stream_t) -> void
    {
        write_stderr("\n");
    }

    template <writable_target Target>
    auto println_to(Target& dest) -> void
    {
        dest.write("\n");
    }
} // namespace tempest

#endif // tempest_core_print_hpp
