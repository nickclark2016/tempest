#ifndef tempest_graphics_render_system_hpp
#define tempest_graphics_render_system_hpp

#include "mesh_component.hpp"

#include <tempest/archetype.hpp>

namespace tempest::graphics
{
    class render_system
    {
      public:
        mesh_layout upload_mesh();
      private:
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_system_hpp