#ifndef tempest_vertex_hpp
#define tempest_vertex_hpp

#include <tempest/math/vec2.hpp>
#include <tempest/math/vec3.hpp>
#include <tempest/math/vec4.hpp>

namespace tempest::core
{
    struct vertex
    {
        math::vec3<float> position;
        math::vec2<float> uv;
        math::vec3<float> normal;
        math::vec4<float> tangent;
    };
}

#endif // tempest_vertex_hpp