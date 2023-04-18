#ifndef tempest_vertex_hpp
#define tempest_vertex_hpp

#include <tempest/vec.hpp>

namespace tempest::assets
{
    struct vertex
    {
        tempest::math::fvec3 position;
        tempest::math::fvec2 uv;
        tempest::math::fvec3 normal;
        tempest::math::fvec4 tangent;
    };
}

#endif // tempest_vertex_hpp