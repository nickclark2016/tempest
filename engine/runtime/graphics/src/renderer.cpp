#include <tempest/renderer.hpp>

namespace tempest::graphics
{
    renderer::builder& renderer::builder::add_pbr_customization(
        function<void(pbr_frame_graph&)> callback)
    {
        _pbr_customization_callbacks.push_back(move(callback));
        return *this;
    }

    renderer renderer::builder::build(logger& log)
    {
        auto instance = rhi::vk::create_instance(&log);
        auto& device = instance->acquire_device(0);

        auto camera_sys = make_unique<camera_system>(*_pbr_inputs.entity_registry,
                                                      _pbr_inputs.entity_registry->event_registry());
        _pbr_inputs.camera_sys = camera_sys.get();

        auto graph = make_unique<pbr_frame_graph>(device, _pbr_cfg, _pbr_inputs);
        for (auto&& callback : _pbr_customization_callbacks)
        {
            callback(*graph);
        }
        return renderer(log, tempest::move(instance), device, tempest::move(graph), tempest::move(camera_sys));
    }

    tuple<unique_ptr<rhi::window_surface>, rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>> renderer::
        create_window(const rhi::window_surface_desc& desc, bool install_swapchain_blit)
    {
        (void)desc;
        (void)install_swapchain_blit;
        return {};
    }

    void renderer::destroy_window(rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> handle)
    {
        (void)handle;
    }

    void renderer::upload_objects_sync(span<const ecs::entity> entities, const core::mesh_registry& meshes,
                                       const core::texture_registry& textures, const core::material_registry& materials)
    {
        _graph->upload_objects_sync(entities, meshes, textures, materials);
    }

    void renderer::finalize_graph()
    {
        _graph->compile({
            .graphics_queues = 1,
            .compute_queues = 1,
            .transfer_queues = 1,
        });
    }

    void renderer::render()
    {
        _graph->execute();
    }

    renderer::renderer(logger& log, unique_ptr<rhi::instance> instance, rhi::device& device,
                       unique_ptr<pbr_frame_graph> graph, unique_ptr<camera_system> camera_sys)
        : _log(&log), _instance(tempest::move(instance)), _device(&device), _graph(tempest::move(graph)),
          _camera_system(tempest::move(camera_sys))
    {
    }
} // namespace tempest::graphics