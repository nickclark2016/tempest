#include <tempest/network/endpoint.hpp>

#include <tempest/array.hpp>
#include <tempest/format.hpp>

namespace tempest::network
{
    auto endpoint::parse(string_view str) noexcept -> optional<endpoint>
    {
        if (str.empty())
        {
            return nullopt;
        }

        auto octets = array<uint32_t, 4>{};
        auto octet_index = size_t{0};
        auto current_value = uint32_t{0};
        auto has_digit = false;
        auto character_index = size_t{0};
        const auto total_length = str.size();

        while (character_index < total_length && octet_index < 4)
        {
            const auto current_char = str[character_index];
            if (current_char >= '0' && current_char <= '9')
            {
                // NOLINTNEXTLINE Read a digit and accumulate its value into the current octet
                current_value = (current_value * 10) + static_cast<uint32_t>(current_char - '0');
                if (current_value > 255) // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers)
                {
                    return nullopt;
                }
                has_digit = true;
                ++character_index;
            }
            else if (current_char == '.')
            {
                if (!has_digit || octet_index >= 3)
                {
                    return nullopt;
                }
                octets[octet_index] = current_value;
                ++octet_index;
                current_value = 0;
                has_digit = false;
                ++character_index;
            }
            else if (current_char == ':')
            {
                break;
            }
            else
            {
                return nullopt;
            }
        }

        if (!has_digit || octet_index != 3)
        {
            return nullopt;
        }
        octets[octet_index] = current_value;

        auto port_value = uint32_t{0};
        if (character_index < total_length)
        {
            if (str[character_index] != ':')
            {
                return nullopt;
            }
            ++character_index;
            if (character_index == total_length)
            {
                return nullopt;
            }
            while (character_index < total_length)
            {
                const auto current_char = str[character_index];
                if (current_char >= '0' && current_char <= '9')
                {
                    // NOLINTNEXTLINE Read a digit and accumulate its value into the port number
                    port_value = (port_value * 10) + static_cast<uint32_t>(current_char - '0');
                    if (port_value > 65535) // NOLINT Max value for a 16-bit unsigned integer
                    {
                        return nullopt;
                    }
                    ++character_index;
                }
                else
                {
                    return nullopt;
                }
            }
        }

        return endpoint(static_cast<uint8_t>(octets[0]), static_cast<uint8_t>(octets[1]),
                        static_cast<uint8_t>(octets[2]), static_cast<uint8_t>(octets[3]),
                        static_cast<uint16_t>(port_value));
    }

    auto endpoint::to_string() const -> string
    {
        const auto octet_a = static_cast<uint32_t>((_address >> 24) & 0xFF);
        const auto octet_b = static_cast<uint32_t>((_address >> 16) & 0xFF);
        const auto octet_c = static_cast<uint32_t>((_address >> 8) & 0xFF);
        const auto octet_d = static_cast<uint32_t>(_address & 0xFF);
        return format("{}.{}.{}.{}:{}", octet_a, octet_b, octet_c, octet_d, _port);
    }
} // namespace tempest::network
