#ifndef tempest_ecs_component_hpp
#define tempest_ecs_component_hpp

#include <cstdint>

namespace tempest::ecs
{
    namespace detail
    {
        struct component_id_helper
        {
            inline static std::uint32_t id = 0;

            template <typename T> inline static std::uint32_t get() noexcept
            {
                static std::uint32_t local_id = id++;
                return local_id;
            }
        };
    } // namespace detail

    template <typename T> struct component
    {
        inline static std::uint32_t id() noexcept
        {
            static auto identifier = detail::component_id_helper::get<T>();
            return identifier;
        }
    };
} // namespace tempest::ecs

#endif // tempest_ecs_component_hpp