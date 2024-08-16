#include <tempest/algorithm.hpp>

#include <cstring>

namespace tempest::detail
{
    void copy_bytes(const void* src, void* dest, size_t count)
    {
        std::memcpy(dest, src, count);
    }
}