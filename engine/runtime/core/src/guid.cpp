#include <tempest/guid.hpp>

#include <format>
#include <random>

namespace tempest
{
    namespace
    {
        constexpr size_t byte_distribution_size = 8;

        std::independent_bits_engine<std::default_random_engine, byte_distribution_size, std::random_device::result_type> byte_distribution( // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
            std::random_device{}());
    }

    auto guid::generate_random_guid() -> guid
    {
        auto result = guid{};

        for (auto& guid_byte : result.data)
        {
            const auto rnd = byte_distribution();
            // extract the low 8 bytes
            guid_byte = static_cast<byte>(rnd & 0xFF); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) -- Mask the high bits off to get the low 8 bits of the random number.
        }

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