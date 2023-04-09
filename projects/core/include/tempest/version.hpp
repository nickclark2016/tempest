#ifndef tempest_version_hpp
#define tempest_version_hpp

#include <cstdint>

namespace tempest::core
{
    struct version
    {
        std::int32_t major;
        std::int32_t minor;
        std::int32_t patch;
    };
}

#endif // tempest_version_hpp
