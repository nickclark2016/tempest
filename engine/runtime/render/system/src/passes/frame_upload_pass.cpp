#include <tempest/render_system/passes/frame_upload_pass.hpp>

namespace tempest::render_system
{
    auto add_frame_upload_pass(render_graph::render_graph& graph, resource_pool& pool)
        -> const frame_upload_pass_data&
    {
        return graph.add_transfer_pass<frame_upload_pass_data>(
            "FrameUploadPass",
            [&pool](render_graph::pass_builder& builder, frame_upload_pass_data& data) {
                data.scene_constants = builder.import_buffer(pool.get_scene_constants_buffer());
                data.object_buffer = builder.import_buffer(pool.get_object_buffer());
                data.instance_buffer = builder.import_buffer(pool.get_instance_buffer());
                data.draw_commands = builder.import_buffer(pool.get_draw_commands_buffer());

                builder.write(data.scene_constants, rhi::pipeline_stage::copy, rhi::resource_access::write);
                builder.write(data.object_buffer, rhi::pipeline_stage::copy, rhi::resource_access::write);
                builder.write(data.instance_buffer, rhi::pipeline_stage::copy, rhi::resource_access::write);
                builder.write(data.draw_commands, rhi::pipeline_stage::copy, rhi::resource_access::write);
            },
            []([[maybe_unused]] const frame_upload_pass_data& data,
               [[maybe_unused]] render_graph::pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {
                // Host CPU direct memcpy was already performed before DAG execution.
                // This pass establishes DAG buffer readiness and pipeline barriers for downstream passes.
            });
    }
} // namespace tempest::render_system
