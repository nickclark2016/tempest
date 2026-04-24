#ifndef tempest_core_unreachable_hpp
#define tempest_core_unreachable_hpp

#include <tempest/api.hpp>

namespace tempest
{
    [[noreturn]] TEMPEST_API void abort() noexcept;

    [[noreturn]] TEMPEST_API inline void unreachable()
    {
#ifdef _DEBUG
        abort();
#else
#ifdef _MSC_VER
        __assume(false);
#else
        __builtin_unreachable();
#endif
#endif
    }
} // namespace tempest

#endif // tempest_core_unreachable_hpp
