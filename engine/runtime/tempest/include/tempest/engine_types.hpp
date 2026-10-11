#ifndef tempest_engine_types_hpp
#define tempest_engine_types_hpp

#include <tempest/api.hpp>
#include <tempest/component_type_registry.hpp>

namespace tempest
{
    TEMPEST_API void register_engine_types(ecs::component_type_registry& types);
} // namespace tempest

#endif // tempest_engine_types_hpp
