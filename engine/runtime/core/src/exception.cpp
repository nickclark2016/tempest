#include <tempest/exception.hpp>

#include <cstdlib>

namespace tempest
{
    [[noreturn]] void terminate()
    {
        ::abort();
    }
} // namespace tempest
