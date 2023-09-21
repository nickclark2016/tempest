#ifndef tempest_graphics_render_system_hpp
#define tempest_graphics_render_system_hpp

#include "mesh_component.hpp"
#include "window.hpp"

#include <tempest/archetype.hpp>
#include <tempest/memory.hpp>
#include <tempest/version.hpp>
#include <tempest/vertex.hpp>

#include <memory>

namespace tempest::graphics
{
    class render_system
    {
      public:
        render_system(const core::version& ver, iwindow& win, core::allocator& allocator);
        ~render_system();

        void render();
        
        mesh_layout upload_mesh(const core::mesh_view& mesh);
      private:
        class render_system_impl;

        std::unique_ptr<render_system_impl> _impl;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_system_hpp