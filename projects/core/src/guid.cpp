#include <tempest/guid.hpp>

#include <format>
#include <random>

namespace tempest
{
    namespace
    {
        std::independent_bits_engine<std::default_random_engine, 8, std::random_device::result_type> byte_distribution(
            std::random_device{}());
    }

    guid guid::generate_random_guid()
    {
        guid result;

        for (byte& b : result.data)
        {
            auto rnd = byte_distribution();
            // extract the low 8 bytes
            b = static_cast<byte>(rnd & 0xFF);
        }

        return result;
    }

    string to_string(const guid& g)
    {
        string str;
        str.reserve(36);

        for (size_t i = 0; i < g.data.size(); ++i)
        {
            if (i == 4 || i == 6 || i == 8 || i == 10)
            {
                str += '-';
            }

            auto hex = std::format("{:02X}", static_cast<unsigned char>(g.data[i]));
            str += hex.c_str();
        }

        return str;
    }
} // namespace tempest