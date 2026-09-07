#include <tempest/format.hpp>

namespace tempest
{
    auto to_string_view(format_error err) noexcept -> string_view
    {
        switch (err)
        {
        case format_error::none:
            return "none";
        case format_error::invalid_format_string:
            return "invalid format string";
        case format_error::unmatched_brace:
            return "unmatched brace";
        case format_error::argument_index_out_of_range:
            return "argument index out of range";
        case format_error::invalid_type_specifier:
            return "invalid type specifier";
        case format_error::buffer_overflow:
            return "buffer overflow";
        }
        return "unknown format error";
    }

    namespace detail
    {
        void format_sink::append(const char* data, size_t count) noexcept
        {
            total_written += count;
            if (buffer_curr != nullptr && buffer_curr < buffer_end)
            {
                auto available = static_cast<size_t>(buffer_end - buffer_curr);
                auto to_copy = (count < available) ? count : available;
                for (size_t copy_idx = 0; copy_idx < to_copy; ++copy_idx)
                {
                    *buffer_curr++ = data[copy_idx];
                }
            }
        }

        void format_sink::append(char character) noexcept
        {
            total_written++;
            if (buffer_curr != nullptr && buffer_curr < buffer_end)
            {
                *buffer_curr++ = character;
            }
        }

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        void format_sink::append_fill(char fill_character, size_t count) noexcept
        {
            total_written += count;
            if (buffer_curr != nullptr && buffer_curr < buffer_end)
            {
                auto available = static_cast<size_t>(buffer_end - buffer_curr);
                auto to_copy = (count < available) ? count : available;
                for (size_t fill_idx = 0; fill_idx < to_copy; ++fill_idx)
                {
                    *buffer_curr++ = fill_character;
                }
            }
        }
    } // namespace detail

    void format_context_base::write(string_view text) noexcept
    {
        _sink->append(text.data(), text.size());
    }

    void format_context_base::write_padded(string_view text) noexcept
    {
        if (_specs->width <= 0 || static_cast<size_t>(_specs->width) <= text.size())
        {
            _sink->append(text.data(), text.size());
            return;
        }

        auto pad = static_cast<size_t>(_specs->width) - text.size();
        auto fill_ch = _specs->zero_pad ? '0' : _specs->fill;

        // If zero-padding, preserve prefixes in front of padding
        if (_specs->zero_pad)
        {
            // 3-character prefix: -0x, +0x, -0b, etc.
            if (text.size() >= 3 && (text[0] == '+' || text[0] == '-') && text[1] == '0' &&
                (text[2] == 'x' || text[2] == 'X' || text[2] == 'b' || text[2] == 'B'))
            {
                _sink->append(text.data(), 3);
                _sink->append_fill('0', pad);
                _sink->append(text.data() + 3, text.size() - 3);
                return;
            }
            // 2-character prefix: 0x, 0b, etc.
            if (text.size() >= 2 && text[0] == '0' &&
                (text[1] == 'x' || text[1] == 'X' || text[1] == 'b' || text[1] == 'B'))
            {
                _sink->append(text.data(), 2);
                _sink->append_fill('0', pad);
                _sink->append(text.data() + 2, text.size() - 2);
                return;
            }
            // 1-character prefix: +, -
            if (!text.empty() && (text[0] == '+' || text[0] == '-'))
            {
                _sink->append(text[0]);
                _sink->append_fill('0', pad);
                _sink->append(text.data() + 1, text.size() - 1);
                return;
            }
        }

        auto alignment = _specs->align;
        if (alignment == format_align::none)
        {
            alignment = format_align::left;
        }

        switch (alignment)
        {
        case format_align::left:
            _sink->append(text.data(), text.size());
            _sink->append_fill(fill_ch, pad);
            break;
        case format_align::right:
        case format_align::none:
            _sink->append_fill(fill_ch, pad);
            _sink->append(text.data(), text.size());
            break;
        case format_align::center: {
            auto left_pad = pad / 2;
            auto right_pad = pad - left_pad;
            _sink->append_fill(fill_ch, left_pad);
            _sink->append(text.data(), text.size());
            _sink->append_fill(fill_ch, right_pad);
            break;
        }
        }
    }

    namespace
    {
        inline constexpr size_t prefix_buffer_size = 8;
        inline constexpr size_t formatted_buffer_size = 80;
        inline constexpr size_t float_num_buffer_size = 80;
        inline constexpr size_t float_prefix_buffer_size = 4;
        inline constexpr size_t float_formatted_buffer_size = 96;
        inline constexpr size_t pointer_buffer_size = 32;
        inline constexpr int decimal_base = 10;
        inline constexpr uint64_t float_sign_mask = 0x8000'0000'0000'0000ULL;
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        auto parse_format_specs(string_view specs_str) noexcept -> format_specs
        {
            format_specs specs{};
            if (specs_str.empty())
            {
                return specs;
            }

            size_t idx = 0;
            size_t len = specs_str.size();

            // Check for fill and align: [[fill]align]
            if (len >= 2 && (specs_str[1] == '<' || specs_str[1] == '>' || specs_str[1] == '^'))
            {
                specs.fill = specs_str[0];
                if (specs_str[1] == '<')
                {
                    specs.align = format_align::left;
                }
                else if (specs_str[1] == '>')
                {
                    specs.align = format_align::right;
                }
                else
                {
                    specs.align = format_align::center;
                }
                idx = 2;
            }
            else if (len >= 1 && (specs_str[0] == '<' || specs_str[0] == '>' || specs_str[0] == '^'))
            {
                if (specs_str[0] == '<')
                {
                    specs.align = format_align::left;
                }
                else if (specs_str[0] == '>')
                {
                    specs.align = format_align::right;
                }
                else
                {
                    specs.align = format_align::center;
                }
                idx = 1;
            }

            // Sign
            if (idx < len && (specs_str[idx] == '+' || specs_str[idx] == '-' || specs_str[idx] == ' '))
            {
                if (specs_str[idx] == '+')
                {
                    specs.sign = format_sign::plus;
                }
                else if (specs_str[idx] == '-')
                {
                    specs.sign = format_sign::minus;
                }
                else
                {
                    specs.sign = format_sign::space;
                }
                idx++;
            }

            // Alternate form '#'
            if (idx < len && specs_str[idx] == '#')
            {
                specs.alternate_form = true;
                idx++;
            }

            // Zero padding '0'
            if (idx < len && specs_str[idx] == '0')
            {
                specs.zero_pad = true;
                if (specs.align == format_align::none)
                {
                    specs.align = format_align::right;
                }
                idx++;
            }

            // Width
            while (idx < len && specs_str[idx] >= '0' && specs_str[idx] <= '9')
            {
                specs.width = (specs.width * decimal_base) + (specs_str[idx] - '0');
                idx++;
            }

            // Precision '.N'
            if (idx < len && specs_str[idx] == '.')
            {
                idx++;
                specs.precision = 0;
                while (idx < len && specs_str[idx] >= '0' && specs_str[idx] <= '9')
                {
                    specs.precision = (specs.precision * decimal_base) + (specs_str[idx] - '0');
                    idx++;
                }
            }

            // Type
            if (idx < len)
            {
                specs.type = specs_str[idx];
            }

            return specs;
        }

        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        void format_integer_arg(detail::format_sink& sink, const detail::format_arg& arg,
                                const format_specs& specs) noexcept
        {
            char num_buf[numeric_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            int base = decimal_base;
            if (specs.type == 'x' || specs.type == 'X')
            {
                base = hex_base;
            }
            else if (specs.type == 'o')
            {
                base = octal_base;
            }
            else if (specs.type == 'b' || specs.type == 'B')
            {
                base = binary_base;
            }

            to_chars_result res;
            bool is_negative = false;

            if (arg.type == detail::arg_type::int_type)
            {
                is_negative = arg.i32 < 0;
                res = to_chars(num_buf, num_buf + sizeof(num_buf), arg.i32, base);
            }
            else if (arg.type == detail::arg_type::int64_type)
            {
                is_negative = arg.i64 < 0;
                res = to_chars(num_buf, num_buf + sizeof(num_buf), arg.i64, base);
            }
            else if (arg.type == detail::arg_type::uint_type)
            {
                res = to_chars(num_buf, num_buf + sizeof(num_buf), arg.u32, base);
            }
            else
            {
                res = to_chars(num_buf, num_buf + sizeof(num_buf), arg.u64, base);
            }

            if (!res)
            {
                return;
            }

            // Handle uppercase hex
            if (specs.type == 'X')
            {
                for (char* char_ptr = num_buf; char_ptr < res.ptr; ++char_ptr)
                {
                    if (*char_ptr >= 'a' && *char_ptr <= 'f')
                    {
                        *char_ptr = static_cast<char>(*char_ptr - 'a' + 'A');
                    }
                }
            }

            // Prepare prefix (+, -, 0x, 0b, etc.)
            char prefix_buf[prefix_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            size_t prefix_len = 0;
            const char* num_start = num_buf;
            size_t num_len = res.ptr - num_buf;

            if (is_negative)
            {
                prefix_buf[prefix_len++] = '-';
                num_start++;
                num_len--;
            }
            else
            {
                if (specs.sign == format_sign::plus)
                {
                    prefix_buf[prefix_len++] = '+';
                }
                else if (specs.sign == format_sign::space)
                {
                    prefix_buf[prefix_len++] = ' ';
                }
            }

            if (specs.alternate_form)
            {
                if (specs.type == 'x')
                {
                    prefix_buf[prefix_len++] = '0';
                    prefix_buf[prefix_len++] = 'x';
                }
                else if (specs.type == 'X')
                {
                    prefix_buf[prefix_len++] = '0';
                    prefix_buf[prefix_len++] = 'X';
                }
                else if (specs.type == 'b')
                {
                    prefix_buf[prefix_len++] = '0';
                    prefix_buf[prefix_len++] = 'b';
                }
                else if (specs.type == 'B')
                {
                    prefix_buf[prefix_len++] = '0';
                    prefix_buf[prefix_len++] = 'B';
                }
                else if (specs.type == 'o')
                {
                    prefix_buf[prefix_len++] = '0';
                }
            }

            size_t total_len = prefix_len + num_len;

            // Combine into temporary buffer for padding
            char formatted[formatted_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            for (size_t prefix_idx = 0; prefix_idx < prefix_len; ++prefix_idx)
            {
                formatted[prefix_idx] = prefix_buf[prefix_idx];
            }
            for (size_t num_idx = 0; num_idx < num_len; ++num_idx)
            {
                formatted[prefix_len + num_idx] = num_start[num_idx];
            }

            format_specs actual_specs = specs;
            if (actual_specs.align == format_align::none)
            {
                actual_specs.align = format_align::right;
            }

            format_context_base ctx(sink, actual_specs, {});
            ctx.write_padded(string_view(formatted, total_len));
        }

        void format_float_arg(detail::format_sink& sink, const detail::format_arg& arg,
                              const format_specs& specs) noexcept
        {
            char num_buf[float_num_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            auto fmt = chars_format::general;
            if (specs.type == 'f' || specs.type == 'F')
            {
                fmt = chars_format::fixed;
            }
            else if (specs.type == 'e' || specs.type == 'E')
            {
                fmt = chars_format::scientific;
            }

            double val = (arg.type == detail::arg_type::float_type) ? static_cast<double>(arg.f32) : arg.f64;
            auto res = to_chars(num_buf, num_buf + sizeof(num_buf), val, fmt, specs.precision);
            if (!res)
            {
                return;
            }

            if (specs.type == 'F' || specs.type == 'E')
            {
                for (char* char_ptr = num_buf; char_ptr < res.ptr; ++char_ptr)
                {
                    if (*char_ptr >= 'a' && *char_ptr <= 'z')
                    {
                        *char_ptr = static_cast<char>(*char_ptr - 'a' + 'A');
                    }
                }
            }

            char prefix_buf[float_prefix_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            size_t prefix_len = 0;

            uint64_t float_bits = 0;
            memcpy(&float_bits, &val, sizeof(val));
            bool is_negative = (float_bits & float_sign_mask) != 0;

            if (!is_negative)
            {
                if (specs.sign == format_sign::plus)
                {
                    prefix_buf[prefix_len++] = '+';
                }
                else if (specs.sign == format_sign::space)
                {
                    prefix_buf[prefix_len++] = ' ';
                }
            }

            size_t num_len = res.ptr - num_buf;
            char formatted[float_formatted_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            for (size_t prefix_idx = 0; prefix_idx < prefix_len; ++prefix_idx)
            {
                formatted[prefix_idx] = prefix_buf[prefix_idx];
            }
            for (size_t num_idx = 0; num_idx < num_len; ++num_idx)
            {
                formatted[prefix_len + num_idx] = num_buf[num_idx];
            }

            format_specs actual_specs = specs;
            if (actual_specs.align == format_align::none)
            {
                actual_specs.align = format_align::right;
            }

            format_context_base ctx(sink, actual_specs, {});
            ctx.write_padded(string_view(formatted, prefix_len + num_len));
        }
    } // namespace

    namespace detail
    {
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        auto vformat_to(detail::format_sink& sink, string_view fmt, span<const format_arg> args) noexcept
            -> format_error
        {
            size_t auto_idx = 0;
            size_t char_idx = 0;
            size_t len = fmt.size();

            while (char_idx < len)
            {
                if (fmt[char_idx] == '{')
                {
                    if (char_idx + 1 < len && fmt[char_idx + 1] == '{')
                    {
                        sink.append('{');
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
                        return format_error::unmatched_brace;
                    }

                    string_view inside = detail::slice(fmt, start, end - start);
                    size_t colon_pos = detail::find_char(inside, ':');
                    string_view id_str =
                        (tempest::cmp_equal(colon_pos, -1)) ? inside : detail::slice(inside, 0, colon_pos);
                    string_view specs_str =
                        (tempest::cmp_equal(colon_pos, -1)) ? string_view{} : detail::slice(inside, colon_pos + 1);

                    size_t arg_idx = 0;
                    if (id_str.empty())
                    {
                        arg_idx = auto_idx++;
                    }
                    else
                    {
                        for (char digit_char : id_str)
                        {
                            if (digit_char < '0' || digit_char > '9')
                            {
                                return format_error::invalid_format_string;
                            }
                            arg_idx = (arg_idx * decimal_base) + static_cast<size_t>(digit_char - '0');
                        }
                    }

                    if (arg_idx >= args.size())
                    {
                        return format_error::argument_index_out_of_range;
                    }

                    auto specs = parse_format_specs(specs_str);
                    const auto& arg = args[arg_idx];

                    switch (arg.type)
                    {
                    case arg_type::bool_type: {
                        format_context_base ctx(sink, specs, specs_str);
                        formatter<bool>::format(arg.b, ctx);
                        break;
                    }
                    case arg_type::char_type: {
                        format_context_base ctx(sink, specs, specs_str);
                        formatter<char>::format(arg.c, ctx);
                        break;
                    }
                    case arg_type::int_type:
                    case arg_type::uint_type:
                    case arg_type::int64_type:
                    case arg_type::uint64_type: {
                        format_integer_arg(sink, arg, specs);
                        break;
                    }
                    case arg_type::float_type:
                    case arg_type::double_type: {
                        format_float_arg(sink, arg, specs);
                        break;
                    }
                    case arg_type::string_view_type: {
                        format_context_base ctx(sink, specs, specs_str);
                        ctx.write_padded(string_view(arg.str_repr.data, arg.str_repr.size));
                        break;
                    }
                    case arg_type::pointer_type: {
                        char ptr_buf[pointer_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                        ptr_buf[0] = '0';
                        ptr_buf[1] = 'x';
                        auto res =
                            to_chars(ptr_buf + 2, ptr_buf + sizeof(ptr_buf), reinterpret_cast<uintptr_t>(arg.ptr), hex_base);
                        format_context_base ctx(sink, specs, specs_str);
                        ctx.write_padded(string_view(ptr_buf, res.ptr - ptr_buf));
                        break;
                    }
                    case arg_type::custom_type: {
                        format_context_base ctx(sink, specs, specs_str);
                        arg.custom.format_fn(arg.custom.value, ctx);
                        break;
                    }
                    case arg_type::none:
                        break;
                    }

                    char_idx = end + 1;
                }
                else if (fmt[char_idx] == '}')
                {
                    if (char_idx + 1 < len && fmt[char_idx + 1] == '}')
                    {
                        sink.append('}');
                        char_idx += 2;
                        continue;
                    }
                    return format_error::unmatched_brace;
                }
                else
                {
                    sink.append(fmt[char_idx]);
                    char_idx++;
                }
            }

            return format_error::none;
        }
    } // namespace detail
} // namespace tempest
