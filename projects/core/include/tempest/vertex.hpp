#ifndef tempest_vertex_hpp
#define tempest_vertex_hpp

#include <array>

namespace tempest::core
{
    struct vertex
    {
        std::array<float, 3> position;
        std::array<float, 2> uv;
        std::array<float, 3> normal;
        std::array<float, 4> tangent;
    };
}

#endif // tempest_vertex_hpp