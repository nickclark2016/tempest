#ifndef tempest_vertex_hpp
#define tempest_vertex_hpp

#include <tempest/vec.hpp>

namespace tempest::core
{
    struct vertex
    {
        math::fvec3 position;
        math::fvec2 uv;
        math::fvec3 normal;
        math::fvec4 tangent;
    };
}

#endif // tempest_vertex_hpp