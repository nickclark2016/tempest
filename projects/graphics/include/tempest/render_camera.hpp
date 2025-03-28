#ifndef tempest_graphics_render_camera_hpp
#define tempest_graphics_render_camera_hpp

#include <tempest/mat4.hpp>
#include <tempest/vec3.hpp>

namespace tempest::graphics
{
    struct render_camera
    {
        math::mat4<float> proj;
        math::mat4<float> inv_proj;
        math::mat4<float> view;
        math::mat4<float> inv_view;
        math::vec4<float> eye_position;
    };
}

#endif