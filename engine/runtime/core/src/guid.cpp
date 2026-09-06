#include <tempest/guid.hpp>

#include <format>
#include <tempest/random.hpp>

namespace tempest
{
    namespace
    {
        constexpr auto version_byte_index = static_cast<size_t>(6);
        constexpr auto version_mask = static_cast<uint8_t>(0x0FU);
        constexpr auto version_4_bits = static_cast<uint8_t>(0x40U);

        constexpr auto variant_byte_index = static_cast<size_t>(8);
        constexpr auto variant_mask = static_cast<uint8_t>(0x3FU);
        constexpr auto variant_rfc4122_bits = static_cast<uint8_t>(0x80U);
    } // namespace

    auto guid::generate_random_guid() -> guid
    {
        auto result = guid{};
        auto entropy_source = random_device{};
        entropy_source.generate(result.data.data(), result.data.size());

        // Format according to RFC 4122 version 4:
        // Set the 4 most significant bits of the 7th byte (byte 6) to 0100 (version 4)
        result.data[version_byte_index] =
            static_cast<byte>((static_cast<uint8_t>(result.data[version_byte_index]) & version_mask) | version_4_bits);
        // Set the 2 most significant bits of the 9th byte (byte 8) to 10 (variant 1, RFC 4122)
        result.data[variant_byte_index] =
            static_cast<byte>((static_cast<uint8_t>(result.data[variant_byte_index]) & variant_mask) | variant_rfc4122_bits);

        return result;
    }

    auto to_string(const guid& uid) -> string
    {
        string str;
        str.reserve(36); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) -- 36 is the length of a GUID string representation (32 hex digits + 4 hyphens).

        for (size_t i = 0; i < uid.data.size(); ++i)
        {
            if (i == 4 || i == 6 || i == 8 || i == 10) // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) -- These are the positions where hyphens are inserted in a GUID string representation.
            {
                str += '-';
            }

            auto hex = std::format("{:02X}", static_cast<unsigned char>(uid.data[i]));
            str += hex.c_str();
        }

        return str;
    }
} // namespace tempest