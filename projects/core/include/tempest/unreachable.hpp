#ifndef tempest_core_unreachable_hpp
#define tempest_core_unreachable_hpp

namespace tempest
{
    [[noreturn]] void abort() noexcept;

    [[noreturn]] inline void unreachable()
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
