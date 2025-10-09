#include <tempest/exception.hpp>

#include <exception>

namespace tempest
{
    [[noreturn]] void terminate()
    {
        std::terminate();
    }
} // namespace tempest
