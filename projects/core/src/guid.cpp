#include <tempest/guid.hpp>

#include <random>

namespace tempest
{
    namespace
    {
        std::independent_bits_engine<std::default_random_engine, 8, std::random_device::result_type> byte_distribution(std::random_device{}());
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
}