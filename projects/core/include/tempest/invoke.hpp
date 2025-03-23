#ifndef tempest_core_invoke_hpp
#define tempest_core_invoke_hpp

#include <tempest/type_traits.hpp>

namespace tempest
{
    template <typename Fn, typename... Args>
        requires is_invocable_v<Fn, Args...>
    constexpr auto invoke(Fn&& fn, Args&&... args) noexcept(tempest::is_nothrow_invocable_v<Fn, Args...>)
        -> tempest::invoke_result_t<Fn, Args...>;

    template <typename R, typename Fn, typename... Args>
        requires is_invocable_r_v<R, Fn, Args...>
    constexpr auto invoke_r(Fn&& fn, Args&&... args) noexcept(tempest::is_nothrow_invocable_r_v<R, Fn, Args...>) -> R;

    namespace detail
    {
        template <typename T>
        inline constexpr bool is_reference_wrapper_v = false;

        template <typename T>
        inline constexpr bool is_reference_wrapper_v<reference_wrapper<T>> = true;

        template <typename C, typename T, typename O, typename... Args>
        inline constexpr decltype(auto) invoke_member_pointer(T C::* member, O&& o, Args&&... args)
        {
            using object_t = remove_cvref_t<O>;
            constexpr bool is_member_func = tempest::is_function_v<T>;
            constexpr bool is_wrapped = is_reference_wrapper_v<object_t>;
            constexpr bool is_derived = tempest::is_same_v<C, object_t> || tempest::is_base_of_v<C, object_t>;

            if constexpr (is_member_func)
            {
                if constexpr (is_derived)
                {
                    return (tempest::forward<O>(o).*member)(tempest::forward<Args>(args)...);
                }
                else if constexpr (is_wrapped)
                {
                    return (o.get().*member)(tempest::forward<Args>(args)...);
                }
                else
                {
                    return ((*tempest::forward<O>(o)).*member)(tempest::forward<Args>(args)...);
                }
            }
            else
            {
                static_assert(tempest::is_object_v<T> && sizeof...(args) == 0);
                if constexpr (is_derived)
                {
                    return tempest::forward<O>(o).*member;
                }
                else if constexpr (is_wrapped)
                {
                    return o.get().*member;
                }
                else
                {
                    return (*tempest::forward<O>(o)).*member;
                }
            }
        }
    } // namespace detail

    template <typename Fn, typename... Args>
        requires is_invocable_v<Fn, Args...>
    inline constexpr auto invoke(Fn&& fn, Args&&... args) noexcept(tempest::is_nothrow_invocable_v<Fn, Args...>)
        -> tempest::invoke_result_t<Fn, Args...>
    {
        if constexpr (tempest::is_member_function_pointer_v<tempest::remove_cvref_t<Fn>>)
        {
            return detail::invoke_member_pointer(tempest::forward<Fn>(fn), tempest::forward<Args>(args)...);
        }
        else
        {
            return tempest::forward<Fn>(fn)(tempest::forward<Args>(args)...);
        }
    }

    template <typename R, typename Fn, typename... Args>
        requires is_invocable_r_v<R, Fn, Args...>
    inline constexpr auto invoke_r(Fn&& fn, Args&&... args) noexcept(tempest::is_nothrow_invocable_r_v<R, Fn, Args...>)
        -> R
    {
        if constexpr (tempest::is_void_v<R>)
        {
            tempest::invoke(tempest::forward<Fn>(fn), tempest::forward<Args>(args)...);
        }
        else
        {
            return tempest::invoke(tempest::forward<Fn>(fn), tempest::forward<Args>(args)...);
        }
    }
} // namespace tempest

#endif // tempest_core_invoke_hpp
