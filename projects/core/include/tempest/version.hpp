#ifndef tempest_version_hpp
#define tempest_version_hpp

#include <tempest/int.hpp>

namespace tempest::core
{
    struct version
    {
        int32_t major;
        int32_t minor;
        int32_t patch;
    };
}

#endif // tempest_version_hpp
