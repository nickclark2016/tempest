#include <tempest/passes/clustered_lighting.hpp>

#include <tempest/files.hpp>

namespace tempest::graphics::passes
{
    bool build_cluster_grid_pass::init(render_device& device)
    {
        auto compute_shader_src = core::read_bytes("assets/shaders/build_cluster_grid.comp.spv");

        descriptor_binding_info set0_bindings[] = {
            passes::light_cluster_desc,
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        push_constant_layout pc_layouts[] = {
            {
                .offset = 0,
                .range = static_cast<uint32_t>(sizeof(push_constants)),
            },
        };

        auto pipeline = device.create_compute_pipeline({
            .layout =
                {
                    .set_layouts = layouts,
                    .push_constants = {pc_layouts}, // Push constants allow us to send data to the shader
                },
            .compute_shader =
                {
                    .bytes = tempest::move(compute_shader_src),
                    .entrypoint = "main",
                    .name = "Build Cluster Grid Compute Shader Module",
                },
            .name = "Build Cluster Grid Compute Pipeline",
        });

        _pipeline = pipeline;

        return _pipeline != compute_pipeline_resource_handle{};
    }

    bool build_cluster_grid_pass::execute([[maybe_unused]] render_device& dev, command_list& cmds,
                                          const compute_command_state& state, push_constants pc) const
    {
        cmds.use_pipeline(_pipeline).push_constants(0, pc, _pipeline).dispatch(state.x, state.y, state.z);

        return true;
    }

    void build_cluster_grid_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_compute_pipeline(_pipeline);
        }
    }
} // namespace tempest::graphics::passes