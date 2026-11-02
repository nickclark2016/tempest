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
    /// @brief Default buffer sizes and numeric bases for formatting.
    inline constexpr size_t default_format_stack_buffer_size = 256;

    /// @brief Buffer size for numeric conversions.
    inline constexpr size_t numeric_buffer_size = 64;

    /// @brief Buffer size for GUID string conversions.
    inline constexpr size_t guid_buffer_size = 37;

    //// @brief Length of a GUID string without the null terminator.
    inline constexpr size_t guid_string_length = 36;

    /// @brief Buffer size for a single character.
    inline constexpr size_t single_char_buffer_size = 1;

    /// @brief Numeric base for binary conversions.
    inline constexpr int binary_base = 2;

    /// @brief Numeric base for octal conversions.
    inline constexpr int octal_base = 8;

    /// @brief Numeric base for hexadecimal conversions.
    inline constexpr int hex_base = 16;

    /// @brief Positions of hyphens in a GUID string.
    inline constexpr size_t guid_hyphen_pos_1 = 4;

    /// @brief Position of the second hyphen in a GUID string.
    inline constexpr size_t guid_hyphen_pos_2 = 6;

    /// @brief Position of the third hyphen in a GUID string.
    inline constexpr size_t guid_hyphen_pos_3 = 8;

    /// @brief Position of the fourth hyphen in a GUID string.
    inline constexpr size_t guid_hyphen_pos_4 = 10;

    /// @brief Mask for extracting a single hexadecimal nibble.
    inline constexpr uint8_t guid_hex_nibble_mask = 0x0F;

    /// @brief Errors that can occur during format string parsing or formatting.
    enum class format_error : uint8_t
    {
        /// @brief No error occurred.
        none = 0,
        /// @brief The format string is invalid.
        invalid_format_string,
        /// @brief There is an unmatched brace in the format string.
        unmatched_brace,
        /// @brief The argument index is out of range.
        argument_index_out_of_range,
        /// @brief The type specifier is invalid.
        invalid_type_specifier,
        /// @brief The buffer overflowed while formatting.
        buffer_overflow,
    };

    /// @brief Converts a format_error code to a human-readable string view.
    TEMPEST_API auto to_string_view(format_error err) noexcept -> string_view;

    /// @brief Text alignment options.
    enum class format_align : uint8_t
    {
        /// @brief No alignment will be applied.
        none = 0,
        /// @brief Align text to the left.
        left,
        /// @brief Align text to the right.
        right,
        /// @brief Center the text.
        center,
    };

    /// @brief Number sign options.
    enum class format_sign : uint8_t
    {
        /// @brief No sign will be used.
        none = 0,
        /// @brief Always show the sign for positive numbers.
        plus,
        /// @brief Always show the sign for negative numbers.
        minus,
        /// @brief Leave a space for positive numbers.
        space,
    };

    /// @brief Parsed standard format specifiers.
    struct format_specs
    {
        char fill = ' ';
        format_align align = format_align::none;
        format_sign sign = format_sign::none;
        bool alternate_form = false; // '#'
        bool zero_pad = false;       // '0'
        int width = 0;
        int precision = -1;
        char type = '\0';
    };

    namespace detail
    {
        /// @brief Sink for accumulating formatted output into a fixed buffer or string.
        struct format_sink
        {
            char* buffer_curr{nullptr};
            char* buffer_end{nullptr};
            size_t total_written{0};

            /// @brief Constructs a format sink with the given start and end pointers.
            /// @param start The start of the buffer.
            /// @param end The end of the buffer.
            constexpr format_sink(char* start, char* end) noexcept : buffer_curr(start), buffer_end(end)
            {
            }

            /// @brief Appends the given data to the sink.
            /// @param data The data to append.
            /// @param count The number of characters to append.
            TEMPEST_API auto append(const char* data, size_t count) noexcept -> void;

            /// @brief Appends a single character to the sink.
            /// @param character The character to append.
            TEMPEST_API auto append(char character) noexcept -> void;

            /// @brief Appends the given fill character to the sink multiple times.
            /// @param fill_character The fill character to append.
            /// @param count The number of times to append the fill character.
            TEMPEST_API auto append_fill(char fill_character, size_t count) noexcept -> void;
        };
    } // namespace detail

    /// @brief Base format context provided to formatter specializations.
    class format_context_base
    {
      public:
        /// @brief Constructs a format context base with the given sink, format specifications, and raw format string.
        /// @param sink The format sink to write output to.
        /// @param specs The parsed format specifications.
        /// @param raw_specs The raw format string.
        format_context_base(detail::format_sink& sink, const format_specs& specs, string_view raw_specs) noexcept
            : _sink(&sink), _specs(&specs), _raw_specs(raw_specs)
        {
        }

        /// @brief Returns the parsed format specifications.
        /// @return The parsed format specifications.
        [[nodiscard]] auto specs() const noexcept -> const format_specs&
        {
            return *_specs;
        }

        /// @brief Returns the raw format string.
        /// @return The raw format string.
        [[nodiscard]] auto raw_specs() const noexcept -> string_view
        {
            return _raw_specs;
        }

        /// @brief Writes the given text to the format sink.
        /// @param text The text to write.
        TEMPEST_API void write(string_view text) noexcept;

        /// @brief Writes the given text to the format sink with padding applied.
        /// @param text The text to write.
        TEMPEST_API void write_padded(string_view text) noexcept;

        /// @brief Returns the underlying format sink.
        /// @return The format sink.
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
    /// @tparam OutputIt The output iterator type.
    /// @tparam CharT The character type, defaults to char.
    template <typename OutputIt, typename CharT = char>
    class basic_format_context : public format_context_base
    {
      public:
        using format_context_base::format_context_base;
    };

    /// @brief Convenience alias for a format context with char output.
    /// @tparam OutputIt The output iterator type.
    template <typename OutputIt>
    using format_context = basic_format_context<OutputIt, char>;

    /// @brief Convenience alias for a format context with wchar_t output.
    /// @tparam OutputIt The output iterator type.
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
        /// @brief Finds the first occurrence of a character in a string view.
        /// @param text The string view to search.
        /// @param target The character to find.
        /// @return The index of the first occurrence of the character, or -1 if not found.
        constexpr auto find_char(string_view text, char target) noexcept -> size_t
        {
            for (auto char_idx = size_t{0}; char_idx < text.size(); ++char_idx)
            {
                if (text[char_idx] == target)
                {
                    return char_idx;
                }
            }
            return static_cast<size_t>(-1);
        }

        /// @brief Slices a string view from a given position with an optional count.
        /// @param text The string view to slice.
        /// @param pos The starting position of the slice.
        /// @param count The number of characters to include in the slice.
        /// @return A string view representing the sliced portion of the original string.
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

        /// @brief Enumerates the possible types of format arguments.
        enum class arg_type : uint8_t
        {
            /// @brief No argument.
            none = 0,
            /// @brief Boolean type
            bool_type,
            /// @brief Character type
            char_type,
            /// @brief Integer type
            int_type,
            /// @brief Unsigned integer type
            uint_type,
            /// @brief 64-bit signed integer type
            int64_type,
            /// @brief 64-bit unsigned integer type
            uint64_type,
            /// @brief 32-bit floating point type
            float_type,
            /// @brief 64-bit floating point type
            double_type,
            /// @brief String view type
            string_view_type,
            /// @brief Pointer type
            pointer_type,
            /// @brief Custom type
            custom_type,
        };

        /// @brief Represents a string view in the format argument system.
        struct string_view_repr
        {
            /// @brief Pointer to the string data.
            const char* data;
            /// @brief Size of the string.
            size_t size;
        };

        /// @brief Represents a single argument for formatting.
        struct format_arg
        {
            arg_type type = arg_type::none;
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

            /// @brief Default constructor for format_arg.
            constexpr format_arg() noexcept : ptr(nullptr)
            {
            }
        };

        /// @brief Creates a format_arg from a given value.
        /// @tparam T The type of the value.
        /// @param val The value to create a format_arg from.
        /// @return A format_arg representing the given value.
        template <typename T>
        auto make_format_arg(const T& val) noexcept -> format_arg
        {
            using Decayed = remove_cvref_t<T>;
            if constexpr (is_same_v<Decayed, bool>)
            {
                auto arg = format_arg{};
                arg.type = arg_type::bool_type;
                arg.b = val;
                return arg;
            }
            else if constexpr (is_same_v<Decayed, char>)
            {
                auto arg = format_arg{};
                arg.type = arg_type::char_type;
                arg.c = val;
                return arg;
            }
            else if constexpr (is_same_v<Decayed, float>)
            {
                auto arg = format_arg{};
                arg.type = arg_type::float_type;
                arg.f32 = val;
                return arg;
            }
            else if constexpr (is_same_v<Decayed, double>)
            {
                auto arg = format_arg{};
                arg.type = arg_type::double_type;
                arg.f64 = val;
                return arg;
            }
            else if constexpr (is_integral_v<Decayed> && is_signed_v<Decayed> && sizeof(Decayed) <= sizeof(int32_t))
            {
                auto arg = format_arg{};
                arg.type = arg_type::int_type;
                arg.i32 = static_cast<int32_t>(val);
                return arg;
            }
            else if constexpr (is_integral_v<Decayed> && is_unsigned_v<Decayed> && sizeof(Decayed) <= sizeof(uint32_t))
            {
                auto arg = format_arg{};
                arg.type = arg_type::uint_type;
                arg.u32 = static_cast<uint32_t>(val);
                return arg;
            }
            else if constexpr (is_integral_v<Decayed> && is_signed_v<Decayed> && sizeof(Decayed) == sizeof(int64_t))
            {
                auto arg = format_arg{};
                arg.type = arg_type::int64_type;
                arg.i64 = static_cast<int64_t>(val);
                return arg;
            }
            else if constexpr (is_integral_v<Decayed> && is_unsigned_v<Decayed> && sizeof(Decayed) == sizeof(uint64_t))
            {
                auto arg = format_arg{};
                arg.type = arg_type::uint64_type;
                arg.u64 = static_cast<uint64_t>(val);
                return arg;
            }
            else if constexpr (is_convertible_v<const T&, string_view>)
            {
                auto arg = format_arg{};
                arg.type = arg_type::string_view_type;
                auto sv_val = string_view(val);
                arg.str_repr.data = sv_val.data();
                arg.str_repr.size = sv_val.size();
                return arg;
            }
            else if constexpr (is_pointer_v<Decayed>)
            {
                auto arg = format_arg{};
                arg.type = arg_type::pointer_type;
                arg.ptr = reinterpret_cast<const void*>(val);
                return arg;
            }
            else
            {
                auto arg = format_arg{};
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
            auto count = size_t{0};
            auto max_explicit_index = -1;
            auto has_automatic = false;
            auto has_explicit = false;
            auto char_idx = size_t{0};
            const auto len = fmt.size();

            while (char_idx < len)
            {
                if (fmt[char_idx] == '{')
                {
                    if (char_idx + 1 < len && fmt[char_idx + 1] == '{')
                    {
                        char_idx += 2;
                        continue;
                    }

                    auto start = char_idx + 1;
                    auto end = start;
                    while (end < len && fmt[end] != '}')
                    {
                        end++;
                    }

                    if (end >= len)
                    {
                        return unexpected<format_error>{format_error::unmatched_brace};
                    }

                    auto inside = slice(fmt, start, end - start);
                    auto colon_pos = find_char(inside, ':');
                    auto id_str = tempest::cmp_equal(colon_pos, -1) ? inside : slice(inside, 0, colon_pos);

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
                        auto parsed_idx = size_t{0};
                        for (auto digit_char : id_str)
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

        /// @brief Validates a format string at compile time.
        /// @tparam ExpectedArgs The expected number of arguments in the format string.
        /// @param fmt The format string to validate.
        /// @return The format error, if any.
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

        /// @brief Reports a format error and aborts the program.
        /// @param err The format error to report.
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

        /// @brief Constructs a compile-time validated format string.
        /// @tparam T The type of the format string.
        /// @param fmt_str The format string to validate.
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
    /// @tparam N The size of the string literal, including the null terminator.
    template <size_t N>
    struct fixed_string
    {
        char buf[N]{}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

        /// @brief Constructs a fixed_string from a string literal.
        /// @param str_literal The string literal to copy into the fixed_string.
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
        constexpr fixed_string(const char (&str_literal)[N]) noexcept
        {
            for (auto elem_idx = size_t{0}; elem_idx < N; ++elem_idx)
            {
                buf[elem_idx] = str_literal[elem_idx];
            }
        }

        /// @brief Returns a pointer to the underlying character array.
        /// @return A pointer to the character array.
        [[nodiscard]] constexpr auto data() const noexcept -> const char*
        {
            return buf;
        }

        /// @brief Returns the size of the string literal, excluding the null terminator.
        /// @return The size of the string literal.
        [[nodiscard]] constexpr auto size() const noexcept -> size_t
        {
            return N > 0 ? N - 1 : 0;
        }

        /// @brief Returns a string_view representing the fixed_string.
        /// @return A string_view representing the fixed_string.
        [[nodiscard]] constexpr auto to_string_view() const noexcept -> string_view
        {
            return string_view(buf, size());
        }

        /// @brief Converts the fixed_string to a string_view.
        /// @return A string_view representing the fixed_string.
        constexpr operator string_view() const noexcept
        {
            return to_string_view();
        }
    };

    /// @brief Concept for verifying that a compile-time string literal has valid placeholders for Args.
    /// @tparam Fmt The compile-time format string.
    /// @tparam Args The types of the arguments to format.
    template <fixed_string Fmt, typename... Args>
    concept valid_format_literal =
        (detail::validate_format_string<sizeof...(Args)>(Fmt.to_string_view()) == format_error::none);

    /// @brief Runtime format string wrapper.
    struct runtime_format_string
    {
        /// @brief The runtime format string.
        tempest::string_view str;
    };

    /// @brief Creates a runtime format string for dynamic or user-specified format strings.
    /// @param str The format string to wrap.
    /// @return A runtime_format_string representing the given format string.
    inline auto runtime_format(tempest::string_view str) noexcept -> runtime_format_string
    {
        return runtime_format_string{
            .str = str,
        };
    }

    //=========================================================================
    // Built-in Formatter Specializations
    //=========================================================================

    /// @brief Formatter specialization for bool type.
    template <>
    struct formatter<bool>
    {
        /// @brief Formats a bool value according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param val The bool value to format.
        /// @param ctx The format context.
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

    /// @brief Formatter specialization for char type.
    template <>
    struct formatter<char>
    {
        /// @brief Formats a char value according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param val The char value to format.
        /// @param ctx The format context.
        template <typename FormatContext>
        static auto format(char val, FormatContext& ctx) -> void
        {
            auto char_buf = array<char, single_char_buffer_size>{val};
            ctx.write_padded(string_view(char_buf.data(), single_char_buffer_size));
        }
    };

    /// @brief Formatter specialization for integral types (excluding bool and char).
    /// @tparam T The integral type to format.
    template <typename T>
        requires(is_integral_v<T> && !is_same_v<T, bool> && !is_same_v<T, char>)
    struct formatter<T>
    {
        /// @brief Formats an integral value according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param val The integral value to format.
        /// @param ctx The format context.
        template <typename FormatContext>
        static auto format(T val, FormatContext& ctx) -> void
        {
            auto buf = array<char, numeric_buffer_size>{};
            auto base = default_base;
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

            auto res = to_chars(buf.data(), buf.data() + buf.size(), val, base);
            if (res)
            {
                if (ctx.specs().type == 'X')
                {
                    for (auto* char_ptr = buf.data(); char_ptr < res.ptr; ++char_ptr)
                    {
                        if (*char_ptr >= 'a' && *char_ptr <= 'f')
                        {
                            *char_ptr = static_cast<char>(*char_ptr - 'a' + 'A');
                        }
                    }
                }
                ctx.write_padded(string_view(buf.data(), res.ptr - buf.data()));
            }
        }
    };

    /// @brief Formatter specialization for floating-point types.
    /// @tparam T The floating-point type to format.
    template <typename T>
        requires(is_floating_point_v<T>)
    struct formatter<T>
    {
        /// @brief Formats a floating-point value according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param val The floating-point value to format.
        /// @param ctx The format context.
        template <typename FormatContext>
        static auto format(T val, FormatContext& ctx) -> void
        {
            auto buf = array<char, numeric_buffer_size>{};
            auto fmt = chars_format::general;
            if (ctx.specs().type == 'f' || ctx.specs().type == 'F')
            {
                fmt = chars_format::fixed;
            }
            else if (ctx.specs().type == 'e' || ctx.specs().type == 'E')
            {
                fmt = chars_format::scientific;
            }

            auto res = to_chars(buf.data(), buf.data() + buf.size(), val, fmt, ctx.specs().precision);
            if (res)
            {
                if (ctx.specs().type == 'F' || ctx.specs().type == 'E')
                {
                    for (auto* char_ptr = buf.data(); char_ptr < res.ptr; ++char_ptr)
                    {
                        if (*char_ptr >= 'a' && *char_ptr <= 'z')
                        {
                            *char_ptr = static_cast<char>(*char_ptr - 'a' + 'A');
                        }
                    }
                }
                ctx.write_padded(string_view(buf.data(), res.ptr - buf.data()));
            }
        }
    };

    /// @brief Formatter specialization for string_view type.
    template <>
    struct formatter<string_view>
    {
        /// @brief Formats a string_view value according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param val The string_view value to format.
        /// @param ctx The format context.
        template <typename FormatContext>
        static auto format(string_view val, FormatContext& ctx) -> void
        {
            ctx.write_padded(val);
        }
    };

    /// @brief Formatter specialization for string type.
    template <>
    struct formatter<string>
    {
        /// @brief Formats a string value according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param val The string value to format.
        /// @param ctx The format context.
        template <typename FormatContext>
        static auto format(const string& val, FormatContext& ctx) -> void
        {
            ctx.write_padded(string_view(val));
        }
    };

    /// @brief Formatter specialization for cstring_view type.
    template <>
    struct formatter<cstring_view>
    {
        /// @brief Formats a cstring_view value according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param val The cstring_view value to format.
        /// @param ctx The format context.
        template <typename FormatContext>
        static auto format(cstring_view val, FormatContext& ctx) -> void
        {
            ctx.write_padded(string_view(val));
        }
    };

    /// @brief Formatter specialization for const char* type.
    template <>
    struct formatter<const char*>
    {
        /// @brief Formats a const char* value according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param val The const char* value to format.
        /// @param ctx The format context.
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

    /// @brief Formatter specialization for guid type.
    template <>
    struct formatter<guid>
    {
        /// @brief Formats a guid value according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param val The guid value to format.
        /// @param ctx The format context.
        template <typename FormatContext>
        static auto format(const guid& val, FormatContext& ctx) -> void
        {
            auto buf = array<char, guid_buffer_size>{};
            auto* out_ptr = buf.data();
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
            ctx.write_padded(string_view(buf.data(), guid_string_length));
        }
    };

    /// @brief Concept for range/container formatting.
    /// @tparam T The type to check for formattable range.
    template <typename T>
    concept formattable_range = requires(const T& range_val) {
        begin(range_val);
        end(range_val);
    } && !is_convertible_v<const T&, string_view> && !is_same_v<remove_cvref_t<T>, string>;

    /// @brief Formatter specialization for formattable ranges.
    /// @tparam R The type of the formattable range.
    template <formattable_range R>
    struct formatter<R>
    {
        /// @brief Formats a formattable range according to the given format context.
        /// @tparam FormatContext The type of the format context.
        /// @param range The formattable range to format.
        /// @param ctx The format context.
        template <typename FormatContext>
        static auto format(const R& range, FormatContext& ctx) -> void
        {
            ctx.write("[");
            auto first = true;
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
    /// @tparam Args The types of the arguments to format.
    /// @param fmt The compile-time validated format string.
    /// @param args The arguments to format.
    /// @return The formatted string.
    template <typename... Args>
    auto format(format_string<type_identity_t<Args>...> fmt, Args&&... args) -> string
    {
        auto stack_buf = array<char, default_format_stack_buffer_size>{};
        auto sink = detail::format_sink(stack_buf.data(), stack_buf.data() + stack_buf.size());

        if constexpr (sizeof...(Args) > 0)
        {
            auto packed = array{detail::make_format_arg(args)...};
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
        }
        else
        {
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
        }

        if (sink.total_written <= sizeof(stack_buf))
        {
            return {stack_buf.data(), sink.total_written};
        }

        auto result = string{};
        result.resize(sink.total_written);
        auto heap_sink = detail::format_sink(result.data(), result.data() + result.size());

        if constexpr (sizeof...(Args) > 0)
        {
            auto packed = array{detail::make_format_arg(args)...};
            detail::vformat_to(heap_sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
        }
        else
        {
            detail::vformat_to(heap_sink, fmt.str, span<const detail::format_arg>{});
        }

        return result;
    }

    /// @brief Formats arguments according to a runtime-specified format string.
    /// @tparam Args The types of the arguments to format.
    /// @param fmt The runtime format string.
    /// @param args The arguments to format.
    /// @return The formatted string on success, or a format_error on failure.
    template <typename... Args>
    auto format(runtime_format_string fmt, Args&&... args) -> expected<string, format_error>
    {
        auto stack_buf = array<char, default_format_stack_buffer_size>{};
        auto sink = detail::format_sink(stack_buf.data(), stack_buf.data() + stack_buf.size());

        auto err = format_error::none;
        if constexpr (sizeof...(Args) > 0)
        {
            auto packed = array{detail::make_format_arg(args)...};
            err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
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
            return string(stack_buf.data(), sink.total_written);
        }

        auto result = string{};
        result.resize(sink.total_written);
        auto heap_sink = detail::format_sink(result.data(), result.data() + result.size());

        if constexpr (sizeof...(Args) > 0)
        {
            auto packed = array{detail::make_format_arg(args)...};
            detail::vformat_to(heap_sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
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
            auto sink = detail::format_sink(out, reinterpret_cast<char*>(uintptr_max)); // NOLINT
            if constexpr (sizeof...(Args) > 0)
            {
                auto packed = array{detail::make_format_arg(args)...};
                detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
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
            auto stack_buf = array<char, default_format_stack_buffer_size>{};
            auto sink = detail::format_sink(stack_buf.data(), stack_buf.data() + stack_buf.size());
            if constexpr (sizeof...(Args) > 0)
            {
                auto packed = array{detail::make_format_arg(args)...};
                detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
            }
            else
            {
                detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
            }

            for (auto copy_idx = size_t{0}; copy_idx < sink.total_written; ++copy_idx)
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
            auto sink = detail::format_sink(out, reinterpret_cast<char*>(uintptr_max)); // NOLINT
            auto err = format_error::none;
            if constexpr (sizeof...(Args) > 0)
            {
                auto packed = array{detail::make_format_arg(args)...};
                err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
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
            auto stack_buf = array<char, default_format_stack_buffer_size>{};
            auto sink = detail::format_sink(stack_buf.data(), stack_buf.data() + stack_buf.size());
            auto err = format_error::none;
            if constexpr (sizeof...(Args) > 0)
            {
                auto packed = array{detail::make_format_arg(args)...};
                err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
            }
            else
            {
                err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
            }

            if (err != format_error::none)
            {
                return unexpected<format_error>{err};
            }

            for (auto copy_idx = size_t{0}; copy_idx < sink.total_written; ++copy_idx)
            {
                *out++ = stack_buf[copy_idx];
            }
            return out;
        }
    }

    /// @brief Formats arguments into a fixed character array with compile-time format string.
    /// @tparam N The size of the fixed character array.
    /// @tparam Args The types of the arguments to format.
    /// @param buf The fixed character array to format into.
    /// @param fmt The compile-time format string.
    /// @param args The arguments to format.
    /// @return The number of characters written to the buffer.
    template <size_t N, typename... Args>
    auto format_to_buffer(char (&buf)[N], // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                          format_string<type_identity_t<Args>...> fmt, Args&&... args) -> size_t
    {
        if constexpr (N == 0)
        {
            return 0;
        }

        auto sink = detail::format_sink(buf, buf + (N - 1));
        if constexpr (sizeof...(Args) > 0)
        {
            auto packed = array{detail::make_format_arg(args)...};
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
        }
        else
        {
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
        }

        const auto written = sink.buffer_curr - buf;
        buf[written] = '\0';
        return written;
    }

    /// @brief Formats arguments into a fixed character array with runtime format string.
    /// @tparam N The size of the fixed character array.
    /// @tparam Args The types of the arguments to format.
    /// @param buf The fixed character array to format into.
    /// @param fmt The runtime format string.
    /// @param args The arguments to format.
    /// @return The number of characters written to the buffer, or a format_error if formatting failed.
    template <size_t N, typename... Args>
    auto format_to_buffer(char (&buf)[N], // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                          runtime_format_string fmt, Args&&... args) -> expected<size_t, format_error>
    {
        if constexpr (N == 0)
        {
            return static_cast<size_t>(0);
        }

        auto sink = detail::format_sink(buf, buf + (N - 1));
        auto err = format_error::none;
        if constexpr (sizeof...(Args) > 0)
        {
            auto packed = array{detail::make_format_arg(args)...};
            err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
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

        const auto written = sink.buffer_curr - buf;
        buf[written] = '\0';
        return written;
    }

    /// @brief Formats arguments into a fixed character array with compile-time format string.
    /// @tparam N The size of the fixed character array.
    /// @tparam Args The types of the arguments to format.
    /// @param buf The fixed character array to format into.
    /// @param fmt The compile-time format string.
    /// @param args The arguments to format.
    /// @return The number of characters written to the buffer.
    template <size_t N, typename... Args>
    auto format_to_buffer(array<char, N>& buf, format_string<type_identity_t<Args>...> fmt, Args&&... args) -> size_t
    {
        if constexpr (N == 0)
        {
            return 0;
        }

        auto sink = detail::format_sink(buf.data(), buf.data() + (N - 1));

        if constexpr (sizeof...(Args) > 0)
        {
            auto packed = array{detail::make_format_arg(args)...};
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
        }
        else
        {
            detail::vformat_to(sink, fmt.str, span<const detail::format_arg>{});
        }

        const auto written = sink.buffer_curr - buf.begin();
        buf[written] = '\0';
        return written;
    }

    template <size_t N, typename... Args>
    auto format_to_buffer(array<char, N>& buf, runtime_format_string fmt, Args&&... args)
        -> expected<size_t, format_error>
    {
        if constexpr (N == 0)
        {
            return static_cast<size_t>(0);
        }

        auto sink = detail::format_sink(buf.data(), buf.data() + (N - 1));
        auto err = format_error::none;
        if constexpr (sizeof...(Args) > 0)
        {
            auto packed = array{detail::make_format_arg(args)...};
            err = detail::vformat_to(sink, fmt.str, span<const detail::format_arg>(packed.data(), sizeof...(Args)));
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

        const auto written = sink.buffer_curr - buf.data();
        buf[written] = '\0';
        return written;
    }
} // namespace tempest

#endif // tempest_core_format_hpp
