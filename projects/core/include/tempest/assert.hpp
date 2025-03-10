#ifndef tempest_core_assert_hpp
#define tempest_core_assert_hpp

#include <tempest/source_location.hpp>
#include <tempest/unreachable.hpp>

#if !defined(NDEBUG)

namespace tempest::assertion::detail
{
    inline void do_basic_assrtt(bool expr, [[maybe_unused]] const source_location& loc,
                                [[maybe_unused]] const char* expression)
    {
        if (!expr)
        {
            tempest::abort();
        }
    }
} // namespace tempest::assertion::detail

#define TEMPEST_ASSERT(expr)                                                                                           \
    tempest::assertion::detail::do_basic_assrtt((expr), tempest::source_location::current(), #expr)

#else

#define TEMPEST_ASSERT(...) ((void)0)
#endif

#endif // tempest_core_assert_hpp
