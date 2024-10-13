#include <tempest/utility.hpp>

#include <cstdlib>

namespace tempest
{
    [[noexcept]] void abort() noexcept
    {
        std::abort();
    }
} // namespace tempest