#ifndef tempest_exception_hpp
#define tempest_exception_hpp

#include <tempest/api.hpp>

namespace tempest
{
    [[noreturn]] TEMPEST_API void terminate();
}

#endif // tempest_exception_hpp
