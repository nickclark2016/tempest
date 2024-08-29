#ifndef tempest_guid_hpp
#define tempest_guid_hpp

#include <tempest/array.hpp>
#include <tempest/int.hpp>

namespace tempest
{
    struct guid
    {
        array<byte, 16> data;

        static guid generate_random_guid();
    };
};

#endif // tempest_guid_hpp