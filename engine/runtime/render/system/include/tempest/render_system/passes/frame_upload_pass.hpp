#ifndef tempest_render_system_frame_upload_pass_hpp
#define tempest_render_system_frame_upload_pass_hpp

#include <tempest/api.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/resource_pool.hpp>

namespace tempest::render_system
{
    struct frame_upload_pass_data
    {
        render_graph::rg_buffer_id scene_constants;
        render_graph::rg_buffer_id object_buffer;
        render_graph::rg_buffer_id instance_buffer;
        render_graph::rg_buffer_id draw_commands;
    };

    TEMPEST_API auto add_frame_upload_pass(render_graph::render_graph& graph, resource_pool& pool)
        -> const frame_upload_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_frame_upload_pass_hpp
