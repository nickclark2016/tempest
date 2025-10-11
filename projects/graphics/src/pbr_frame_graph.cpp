#include <tempest/frame_graph.hpp>

#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/enum.hpp>
#include <tempest/files.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/int.hpp>
#include <tempest/mat4.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>

namespace tempest::graphics
{
    pbr_frame_graph::pbr_frame_graph(rhi::device& device, pbr_frame_graph_config cfg, pbr_frame_graph_inputs inputs)
        : _device(&device), _cfg(cfg), _inputs(inputs), _builder{graph_builder{}}, _executor{none()}
    {
    }

    pbr_frame_graph::~pbr_frame_graph() = default;

    optional<graph_builder&> pbr_frame_graph::get_builder() noexcept
    {
        if (_builder.has_value())
        {
            return some(_builder.value());
        }
        return none();
    }

    void pbr_frame_graph::compile(queue_configuration cfg)
    {
        auto exec_plan = move(_builder).value().compile(cfg);
        _builder = none();
        _executor = graph_executor(*_device);
        _executor->set_execution_plan(tempest::move(exec_plan));
    }

    void pbr_frame_graph::execute()
    {
        TEMPEST_ASSERT(_executor.has_value());
        _executor->execute();
    }
} // namespace tempest::graphics