#include <tempest/utility.hpp>

#include <cstdlib>

namespace tempest
{
    void abort() noexcept
    {
        std::abort();
    }
} // namespace tempest