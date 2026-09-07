#ifndef tempest_core_format_hpp
#define tempest_core_format_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/charconv.hpp>
#include <tempest/cstring_view.hpp>
#include <tempest/expected.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    inline constexpr size_t default_format_stack_buffer_size = 256;
    inline constexpr size_t numeric_buffer_size = 64;
    inline constexpr size_t guid_buffer_size = 37;
    inline constexpr size_t guid_string_length = 36;
    inline constexpr size_t single_char_buffer_size = 1;

    inline constexpr int binary_base = 2;
    inline constexpr int octal_base = 8;
    inline constexpr int hex_base = 16;

    inline constexpr size_t guid_hyphen_pos_1 = 4;
    inline constexpr size_t guid_hyphen_pos_2 = 6;
    inline constexpr size_t guid_hyphen_pos_3 = 8;
    inline constexpr size_t guid_hyphen_pos_4 = 10;
    inline constexpr uint8_t guid_hex_nibble_mask = 0x0F;

    /// @brief Errors that can occur during format string parsing or formatting.
    enum class format_error : uint8_t
    {
        none = 0,
        invalid_format_string,
        unmatched_brace,
        argument_index_out_of_range,
        invalid_type_specifier,
        buffer_overflow,
    };

    /// @brief Converts a format_error code to a human-readable string view.
    TEMPEST_API auto to_string_view(format_error err) noexcept -> string_view;

    /// @brief Text alignment options.
    enum class format_align : uint8_t
    {
        none = 0,
        left,
        right,
        center
    };

    /// @brief Number sign options.
    enum class format_sign : uint8_t
    {
        none = 0,
        plus,
        minus,
        space
    };

    /// @brief Parsed standard format specifiers.
    struct format_specs
    {
        char fill{' '};
        format_align align{format_align::none};
        format_sign sign{format_sign::none};
        bool alternate_form{false}; // '#'
        bool zero_pad{false};       // '0'
        int width{0};
        int precision{-1};
        char type{'\0'};
    };

    namespace detail
    {
        /// @brief Sink for accumulating formatted output into a fixed buffer or string.
        struct format_sink
        {
            char* buffer_curr{nullptr};
            char* buffer_end{nullptr};
            size_t total_written{0};

            constexpr format_sink(char* start, char* end) noexcept : buffer_curr(start), buffer_end(end)
            {
            }

            TEMPEST_API void append(const char* data, size_t count) noexcept;
            TEMPEST_API void append(char character) noexcept;
            TEMPEST_API void append_fill(char fill_character, size_t count) noexcept;
        };
    } // namespace detail

    /// @brief Base format context provided to formatter specializations.
    class format_context_base
    {
      public:
        format_context_base(detail::format_sink& sink, const format_specs& specs, string_view raw_specs) noexcept
            : _sink(&sink), _specs(&specs), _raw_specs(raw_specs)
        {
        }

        [[nodiscard]] auto specs() const noexcept -> const format_specs&
        {
            return *_specs;
        }

        [[nodiscard]] auto raw_specs() const noexcept -> string_view
        {
            return _raw_specs;
        }

        TEMPEST_API void write(string_view text) noexcept;
        TEMPEST_API void write_padded(string_view text) noexcept;

        auto sink() noexcept -> detail::format_sink&
        {
            return *_sink;
        }

      private:
        detail::format_sink* _sink{nullptr};
        const format_specs* _specs{nullptr};
        string_view _raw_specs;
    };

    /// @brief Templated format context for iterator output.
    template <typename OutputIt, typename CharT = char>
    class basic_format_context : public format_context_base
    {
      public:
        using format_context_base::format_context_base;
    };

    template <typename OutputIt>
    using format_context = basic_format_context<OutputIt, char>;

    template <typename OutputIt>
    using wformat_context = basic_format_context<OutputIt, wchar_t>;

    /// @brief User customization point for formatting custom types.
    template <typename T, typename CharT = char>
    struct formatter;

    /// @brief Concept for types that have a valid formatter.
    template <typename T, typename CharT = char>
    concept formattable =
        requires(const T& val, format_context_base& ctx) { formatter<remove_cvref_t<T>, CharT>::format(val, ctx); };

    namespace detail
    {
        constexpr auto find_char(string_view text, char target) noexcept -> size_t
        {
            for (size_t char_idx = 0; char_idx < text.size(); ++char_idx)
            {
                if (text[char_idx] == target)
                {
                    return char_idx;
                }
            }
            return static_cast<size_t>(-1);
        }

        constexpr auto slice(string_view text, size_t pos, // NOLINT(bugprone-easily-swappable-parameters)
                             size_t count = static_cast<size_t>(-1)) noexcept -> string_view
        {
            if (pos >= text.size())
            {
                return {};
            }
            auto available = text.size() - pos;
            auto len = (count < available) ? count : available;
            return {text.data() + pos, len};
        }

        enum class arg_type : uint8_t
        {
            none = 0,
            bool_type,
            char_type,
            int_type,
            uint_type,
            int64_type,
            uint64_type,
            float_type,
            double_type,
            string_view_type,
            pointer_type,
            custom_type
        };

        struct string_view_repr
        {
            const char* data;
            size_t size;
        };

        struct format_arg
        {
            arg_type type{arg_type::none};
            union {
                bool b;
                char c;
                int32_t i32;
                uint32_t u32;
                int64_t i64;
                uint64_t u64;
                float f32;
                double f64;
                string_view_repr str_repr;
                const void* ptr;
                struct
                {
                    const void* value;
                    void (*format_fn)(const void* value, format_context_base& ctx);
                } custom;
            };

            constexpr format_arg() noexcept : ptr(nullptr)
            {
            }
        };

        template <typename T>
        auto make_format_arg(const T& val) noexcept -> format_arg
        {
            using Decayed = remove_cvref_t<T>;
            if constexpr (is_same_v<Decayed, bool>)
            {
                format_arg arg;
                arg.type = arg_type::bool_type;
                arg.b = val;
                return arg;
            }
            else if constexpr (is_same_v<Decayed, char>)
            {
                format_arg arg;
                arg.type = arg_type::char_type;
                arg.c = val;
                return arg;
            }
            else if constexpr (is_same_v<Decayed, float>)
            {
                format_arg arg;
                arg.type = arg_type::float_type;
                arg.f32 = val;
                return arg;
            }
            else if constexpr (is_same_v<Decayed, double>)
            {
                format_arg arg;
                arg.type = arg_type::double_type;
                arg.f64 = val;
                return arg;
            }
            else if constexpr (is_integral_v<Decayed> && is_signed_v<Decayed> && sizeof(Decayed) <= sizeof(int32_t))
            {
                format_arg arg;
                arg.type = arg_type::int_type;
                arg.i32 = static_cast<int32_t>(val);
                return arg;
            }
            else if constexpr (is_integral_v<Decayed> && is_unsigned_v<Decayed> && sizeof(Decayed) <= sizeof(uint32_t))
            {
                format_arg arg;
                arg.type = arg_type::uint_type;
                arg.u32 = static_cast<uint32_t>(val);
                return arg;
            }
            else if constexpr (is_integral_v<Decayed> && is_signed_v<Decayed> && sizeof(Decayed) == sizeof(int64_t))
            {
                format_arg arg;
                arg.type = arg_type::int64_type;
                arg.i64 = static_cast<int64_t>(val);
                return arg;
            }
            else if constexpr (is_integral_v<Decayed> && is_unsigned_v<Decayed> && sizeof(Decayed) == sizeof(uint64_t))
            {
                format_arg arg;
                arg.type = arg_type::uint64_type;
                arg.u64 = static_cast<uint64_t>(val);
                return arg;
            }
            else if constexpr (is_convertible_v<const T&, string_view>)
            {
                format_arg arg;
                arg.type = arg_type::string_view_type;
                auto sv_val = string_view(val);
                arg.str_repr.data = sv_val.data();
                arg.str_repr.size = sv_val.size();
                return arg;
            }
            else if constexpr (is_pointer_v<Decayed>)
            {
                format_arg arg;
                arg.type = arg_type::pointer_type;
                arg.ptr = reinterpret_cast<const void*>(val);
                return arg;
            }
            else
            {
                format_arg arg;
                arg.type = arg_type::custom_type;
                arg.custom.value = static_cast<const void*>(&val);
                arg.custom.format_fn = [](const void* ptr, format_context_base& ctx) -> auto {
                    formatter<Decayed>::format(*static_cast<const Decayed*>(ptr), ctx);
                };
                return arg;
            }
        }

        TEMPEST_API auto vformat_to(detail::format_sink& sink, string_view fmt, span<const format_arg> args) noexcept
            -> format_error;

        constexpr auto count_placeholders(string_view fmt) noexcept // NOLINT(readability-function-cognitive-complexity)
            -> expected<size_t, format_error>
        {
            size_t count = 0;
            int max_explicit_index = -1;
            bool has_automatic = false;
            bool has_explicit = false;
            size_t char_idx = 0;
            size_t len = fmt.size();

            while (char_idx < len)
            {
                if (fmt[char_idx] == '{')
                {
                    if (char_idx + 1 < len && fmt[char_idx + 1] == '{')
                    {
                        char_idx += 2;
                        continue;
                    }
                    size_t start = char_idx + 1;
                    size_t end = start;
                    while (end < len && fmt[end] != '}')
                    {
                        end++;
                    }
                    if (end >= len)
                    {
                        return unexpected<format_error>{format_error::unmatched_brace};
                    }

                    string_view inside = slice(fmt, start, end - start);
                    size_t colon_pos = find_char(inside, ':');
                    string_view id_str = (tempest::cmp_equal(colon_pos, -1)) ? inside : slice(inside, 0, colon_pos);

                    if (id_str.empty())
                    {
                        if (has_explicit)
                        {
                            return unexpected<format_error>{format_error::invalid_format_string};
                        }
                        has_automatic = true;
                        count++;
                    }
                    else
                    {
                        if (has_automatic)
                        {
                            return unexpected<format_error>{format_error::invalid_format_string};
                        }
                        has_explicit = true;
                        size_t parsed_idx = 0;
                        for (char digit_char : id_str)
                        {
                            if (digit_char < '0' || digit_char > '9')
                            {
                                return unexpected<format_error>{format_error::invalid_format_string};
                            }
                            parsed_idx = (parsed_idx * static_cast<size_t>(default_base)) +
                                         static_cast<size_t>(digit_char - '0');
                        }
                        max_explicit_index = tempest::max(static_cast<int>(parsed_idx), max_explicit_index);
                    }
                    char_idx = end + 1;
                }
                else if (fmt[char_idx] == '}')
                {
                    if (char_idx + 1 < len && fmt[char_idx + 1] == '}')
                    {
                        char_idx += 2;
                        continue;
                    }
                    return unexpected<format_error>{format_error::unmatched_brace};
                }
                else
                {
                    char_idx++;
                }
            }

            if (has_explicit)
            {
                return static_cast<size_t>(max_explicit_index + 1);
            }
            return count;
        }

        template <size_t ExpectedArgs>
        consteval auto validate_format_string(string_view fmt) noexcept -> format_error
        {
            auto res = count_placeholders(fmt);
            if (!res)
            {
                return res.error();
            }
            if (*res != ExpectedArgs)
            {
                return format_error::argument_index_out_of_range;
            }
            return format_error::none;
        }

        [[noreturn]] inline void report_format_error([[maybe_unused]] format_error err)
        {
            tempest::abort();
        }
    } // namespace detail

    /// @brief Compile-time validated format string wrapper.
    template <typename... Args>
    struct format_string
    {
        string_view str;

        template <typename T>
            requires(is_convertible_v<const T&, string_view>)
        consteval format_string(const T& fmt_str) noexcept : str(fmt_str)
        {
            auto err = detail::validate_format_string<sizeof...(Args)>(str);
            if (err != format_error::none)
            {
                detail::report_format_error(err);
            }
        }
    };

    /// @brief Fixed-capacity string literal helper for NTTP concept testing.
    template <size_t N>
    struct fixed_string
    {
        char buf[N]{}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

        constexpr fixed_string(
            const char (&str_literal)[N]) noexcept // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
        {
            for (size_t elem_idx = 0; elem_idx < N; ++elem_idx)
            {
                buf[elem_idx] = str_literal[elem_idx];
            }
        }

        [[nodiscard]] constexpr auto data() const noexcept -> const char*
        {
            return buf;
        }

        [[nodiscard]] constexpr auto size() const noexcept -> size_t
        {
            return N > 0 ? N - 1 : 0;
        }

        [[nodiscard]] constexpr auto to_string_view() const noexcept -> string_view
        {
            return string_view(buf, size());
        }

        constexpr operator string_view() const noexcept
        {
            return to_string_view();
        }
    };

    /// @brief Concept for verifying that a compile-time string literal has valid placeholders for Args.
    template <fixed_string Fmt, typename... Args>
    concept valid_format_literal =
        (detail::validate_format_string<sizeof...(Args)>(Fmt.to_string_view()) == format_error::none);

    /// @brief Runtime format string wrapper.
    struct runtime_format_string
    {
        tempest::string_view str;
    };

    /// @brief Creates a runtime format string for dynamic or user-specified format strings.
    inline auto runtime_format(tempest::string_view str) noexcept -> runtime_format_string
    {
        return runtime_format_string{str};
    }

    //=========================================================================
    // Built-in Formatter Specializations
    //=========================================================================

    template <>
    struct tempest::formatter<bool>
    {
        template <typename FormatContext>
        static auto format(bool val, FormatContext& ctx) -> void
        {
            if (ctx.specs().type == 'd' || ctx.specs().type == 'u')
            {
                ctx.write_padded(val ? "1" : "0");
            }
            else
            {
                ctx.write_padded(val ? "true" : "false");
            }
        }
    };

    template <>
    struct formatter<char>
    {
        template <typename FormatContext>
        static auto format(char val, FormatContext& ctx) -> void
        {
            char char_buf[single_char_buffer_size] = {val}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            ctx.write_padded(string_view(char_buf, single_char_buffer_size));
        }
    };

    template <typename T>
        requires(is_integral_v<T> && !is_same_v<T, bool> && !is_same_v<T, char>)
    struct formatter<T>
    {
        template <typename FormatContext>
        static auto format(T val, FormatContext& ctx) -> void
        {
            char buf[numeric_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            int base = default_base;
            if (ctx.specs().type == 'x' || ctx.specs().type == 'X')
            {
                base = hex_base;
            }
            else if (ctx.specs().type == 'o')
            {
                base = octal_base;
            }
            else if (ctx.specs().type == 'b' || ctx.specs().type == 'B')
            {
                base = binary_base;
            }

            auto res = to_chars(buf, buf + sizeof(buf), val, base);
            if (res)
            {
                if (ctx.specs().type == 'X')
                {
                    for (char* char_ptr = buf; char_ptr < res.ptr; ++char_ptr)
                    {
                        if (*char_ptr >= 'a' && *char_ptr <= 'f')
                        {
                            *char_ptr = static_cast<char>(*char_ptr - 'a' + 'A');
                        }
                    }
                }
                ctx.write_padded(string_view(buf, res.ptr - buf));
            }
        }
    };

    template <typename T>
        requires(is_floating_point_v<T>)
    struct formatter<T>
    {
        template <typename FormatContext>
        static auto format(T val, FormatContext& ctx) -> void
        {
            char buf[numeric_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            auto fmt = chars_format::general;
            if (ctx.specs().type == 'f' || ctx.specs().type == 'F')
            {
                fmt = chars_format::fixed;
            }
            else if (ctx.specs().type == 'e' || ctx.specs().type == 'E')
            {
                fmt = chars_format::scientific;
            }

            auto res = to_chars(buf, buf + sizeof(buf), val, fmt, ctx.specs().precision);
            if (res)
            {
                if (ctx.specs().type == 'F' || ctx.specs().type == 'E')
                {
                    for (char* char_ptr = buf; char_ptr < res.ptr; ++char_ptr)
                    {
                        if (*char_ptr >= 'a' && *char_ptr <= 'z')
                        {
                            *char_ptr = static_cast<char>(*char_ptr - 'a' + 'A');
                        }
                    }
                }
                ctx.write_padded(string_view(buf, res.ptr - buf));
            }
        }
    };

    template <>
    struct formatter<string_view>
    {
        template <typename FormatContext>
        static auto format(string_view val, FormatContext& ctx) -> void
        {
            ctx.write_padded(val);
        }
    };

    template <>
    struct formatter<string>
    {
        template <typename FormatContext>
        static auto format(const string& val, FormatContext& ctx) -> void
        {
            ctx.write_padded(string_view(val));
        }
    };

    template <>
    struct formatter<cstring_view>
    {
        template <typename FormatContext>
        static auto format(cstring_view val, FormatContext& ctx) -> void
        {
            ctx.write_padded(string_view(val));
        }
    };

    template <>
    struct formatter<const char*>
    {
        template <typename FormatContext>
        static auto format(const char* val, FormatContext& ctx) -> void
        {
            if (val != nullptr)
            {
                ctx.write_padded(string_view(val));
            }
            else
            {
                ctx.write_padded("(null)");
            }
        }
    };

    template <>
    struct formatter<guid>
    {
        template <typename FormatContext>
        static auto format(const guid& val, FormatContext& ctx) -> void
        {
            char buf[guid_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            auto* out_ptr = buf;
            constexpr const char* hex_lower = "0123456789abcdef";
            constexpr const char* hex_upper = "0123456789ABCDEF";
            const char* hex = (ctx.specs().type == 'X') ? hex_upper : hex_lower;

            for (size_t byte_idx = 0; byte_idx < val.data.size(); ++byte_idx)
            {
                if (byte_idx == guid_hyphen_pos_1 || byte_idx == guid_hyphen_pos_2 || byte_idx == guid_hyphen_pos_3 ||
                    byte_idx == guid_hyphen_pos_4)
                {
                    *out_ptr++ = '-';
                }
                auto byte_val = static_cast<uint8_t>(val.data[byte_idx]);
                *out_ptr++ = hex[(byte_val >> 4) & guid_hex_nibble_mask];
                *out_ptr++ = hex[byte_val & guid_hex_nibble_mask];
            }
            ctx.write_padded(string_view(buf, guid_string_length));
        }
    };

    /// @brief Concept for range/container formatting.
    template <typename T>
    concept formattable_range = requires(const T& range_val) {
        begin(range_val);
        end(range_val);
    } && !is_convertible_v<const T&, string_view> && !is_same_v<remove_cvref_t<T>, string>;

    template <formattable_range R>
    struct formatter<R>
    {
        template <typename FormatContext>
        static auto format(const R& range, FormatContext& ctx) -> void
        {
            ctx.write("[");
            bool first = true;
            for (const auto& elem : range)
            {
                if (!first)
                {
                    ctx.write(", ");
                }
                first = false;
                formatter<remove_cvref_t<decltype(elem)>>::format(elem, ctx);
            }
            ctx.write("]");
        }
    };

    //=========================================================================
    // Formatting API Functions
    //=========================================================================

    /// @brief Formats arguments according to a compile-time validated format string.
    template <typename... Args>
    auto format(format_string<type_identity_t<Args>...> fmt, Args&&... args) -> string
    {
        char stack_buf[default_format_stack_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
        detail::format_sink sink(stack_buf, stack_buf + sizeof(stack_buf));

        if constexpr (sizeof...(Args) > 0)
        {
            detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
        }
        else
        {
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
        }

        if (sink.total_written <= sizeof(stack_buf))
        {
            return {stack_buf, sink.total_written};
        }

        string result;
        result.resize(sink.total_written);
        detail::format_sink heap_sink(result.data(), result.data() + result.size());

        if constexpr (sizeof...(Args) > 0)
        {
            detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            detail::vformat_to(heap_sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
        }
        else
        {
            detail::vformat_to(heap_sink, fmt.str, span<const detail::format_arg>{});
        }

        return result;
    }

    /// @brief Formats arguments according to a runtime-specified format string.
    template <typename... Args>
    auto format(runtime_format_string fmt, Args&&... args) -> expected<string, format_error>
    {
        char stack_buf[default_format_stack_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
        detail::format_sink sink(stack_buf, stack_buf + sizeof(stack_buf));

        format_error err{format_error::none};
        if constexpr (sizeof...(Args) > 0)
        {
            detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
        }
        else
        {
            err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
        }

        if (err != format_error::none)
        {
            return unexpected<format_error>{err};
        }

        if (sink.total_written <= sizeof(stack_buf))
        {
            return string(stack_buf, sink.total_written);
        }

        string result;
        result.resize(sink.total_written);
        detail::format_sink heap_sink(result.data(), result.data() + result.size());

        if constexpr (sizeof...(Args) > 0)
        {
            detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            detail::vformat_to(heap_sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
        }
        else
        {
            detail::vformat_to(heap_sink, fmt.str, span<const detail::format_arg>{});
        }

        return result;
    }

    /// @brief Formats arguments into an output iterator with a compile-time format string.
    template <typename OutputIt, typename... Args>
    auto format_to(OutputIt out, format_string<type_identity_t<Args>...> fmt, Args&&... args) -> OutputIt
    {
        if constexpr (is_same_v<OutputIt, char*>)
        {
            detail::format_sink sink(out, reinterpret_cast<char*>(UINTPTR_MAX));
            if constexpr (sizeof...(Args) > 0)
            {
                detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
            }
            else
            {
                detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
            }
            return out + sink.total_written;
        }
        else
        {
            // For other iterators, format to stack buffer and copy
            char stack_buf[default_format_stack_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            detail::format_sink sink(stack_buf, stack_buf + sizeof(stack_buf));
            if constexpr (sizeof...(Args) > 0)
            {
                detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
            }
            else
            {
                detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
            }

            for (size_t copy_idx = 0; copy_idx < sink.total_written; ++copy_idx)
            {
                *out++ = stack_buf[copy_idx];
            }
            return out;
        }
    }

    /// @brief Formats arguments into an output iterator with a runtime format string.
    template <typename OutputIt, typename... Args>
    auto format_to(OutputIt out, runtime_format_string fmt, Args&&... args) -> expected<OutputIt, format_error>
    {
        if constexpr (is_same_v<OutputIt, char*>)
        {
            detail::format_sink sink(out, reinterpret_cast<char*>(UINTPTR_MAX));
            format_error err{format_error::none};
            if constexpr (sizeof...(Args) > 0)
            {
                detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
            }
            else
            {
                err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
            }

            if (err != format_error::none)
            {
                return unexpected<format_error>{err};
            }
            return out + sink.total_written;
        }
        else
        {
            char stack_buf[default_format_stack_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            detail::format_sink sink(stack_buf, stack_buf + sizeof(stack_buf));
            format_error err{format_error::none};
            if constexpr (sizeof...(Args) > 0)
            {
                detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
            }
            else
            {
                err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
            }

            if (err != format_error::none)
            {
                return unexpected<format_error>{err};
            }

            for (size_t copy_idx = 0; copy_idx < sink.total_written; ++copy_idx)
            {
                *out++ = stack_buf[copy_idx];
            }
            return out;
        }
    }

    /// @brief Formats arguments into a fixed character array with compile-time format string.
    template <size_t N, typename... Args>
    auto format_to_buffer(char (&buf)[N], // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                          format_string<type_identity_t<Args>...> fmt, Args&&... args) -> size_t
    {
        if constexpr (N == 0)
        {
            return 0;
        }

        detail::format_sink sink(buf, buf + (N - 1));
        if constexpr (sizeof...(Args) > 0)
        {
            detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
        }
        else
        {
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
        }

        size_t written = sink.buffer_curr - buf;
        buf[written] = '\0';
        return written;
    }

    /// @brief Formats arguments into a fixed character array with runtime format string.
    template <size_t N, typename... Args>
    auto format_to_buffer(char (&buf)[N], // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                          runtime_format_string fmt, Args&&... args) -> expected<size_t, format_error>
    {
        if constexpr (N == 0)
        {
            return static_cast<size_t>(0);
        }

        detail::format_sink sink(buf, buf + (N - 1));
        format_error err{format_error::none};
        if constexpr (sizeof...(Args) > 0)
        {
            detail::format_arg packed[] = {detail::make_format_arg(args)...}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed, sizeof...(Args)));
        }
        else
        {
            err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
        }

        if (err != format_error::none)
        {
            buf[0] = '\0';
            return unexpected<format_error>{err};
        }

        size_t written = sink.buffer_curr - buf;
        buf[written] = '\0';
        return written;
    }
} // namespace tempest

#endif // tempest_core_format_hpp
