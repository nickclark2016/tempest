#include <tempest/exception.hpp>
#include <tempest/unreachable.hpp>

namespace tempest
{
    [[noreturn]] void terminate()
    {
        tempest::abort();
    }
} // namespace tempest
