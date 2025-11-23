#include "tempest/frame_graph.hpp"
#include <tempest/renderer.hpp>

namespace tempest::graphics
{
    renderer::builder& renderer::builder::set_pbr_frame_graph_config(pbr_frame_graph_config cfg)
    {
        _pbr_cfg = cfg;
        return *this;
    }

    renderer::builder& renderer::builder::set_pbr_frame_graph_inputs(pbr_frame_graph_inputs inputs)
    {
        _pbr_inputs = inputs;
        return *this;
    }

    renderer::builder& renderer::builder::add_pbr_frame_graph_customization_callback(
        function<void(pbr_frame_graph&)> callback)
    {
        _pbr_customization_callbacks.push_back(move(callback));
        return *this;
    }

    renderer renderer::builder::build()
    {
        auto instance = rhi::vk::create_instance();
        auto& device = instance->acquire_device(0);

        auto graph = make_unique<pbr_frame_graph>(device, _pbr_cfg, _pbr_inputs);
        for (auto&& callback : _pbr_customization_callbacks)
        {
            callback(*graph);
        }
        return renderer(tempest::move(instance), device, tempest::move(graph));
    }

    unique_ptr<rhi::window_surface> renderer::create_window(const rhi::window_surface_desc& desc)
    {
        auto win = rhi::vk::create_window_surface(desc);
        auto surface = _device->create_render_surface({
            .window = win.get(),
            .min_image_count = 2,
            .format =
                {
                    .space = rhi::color_space::srgb_nonlinear,
                    .format = rhi::image_format::bgra8_srgb,
                },
            .present_mode = rhi::present_mode::immediate,
            .width = win->framebuffer_width(),
            .height = win->framebuffer_height(),
            .layers = 1,
        });

        auto imported_handle = _graph->get_builder()->import_render_surface("Render Surface", surface);
        _graph->get_builder()->create_transfer_pass(
            "Present to Swapchain",
            [&](transfer_task_builder& builder) {
                auto color_handle = _graph->get_tonemapped_color_handle();
                builder.read(color_handle, rhi::image_layout::transfer_src, make_enum_mask(rhi::pipeline_stage::blit),
                             make_enum_mask(rhi::memory_access::transfer_read));
                builder.write(imported_handle, rhi::image_layout::transfer_dst,
                              make_enum_mask(rhi::pipeline_stage::blit),
                              make_enum_mask(rhi::memory_access::transfer_write));
            },
            [](transfer_task_execution_context& ctx, auto swapchain_handle, auto color_handle) {
                ctx.blit(color_handle, swapchain_handle);
            },
            imported_handle, _graph->get_tonemapped_color_handle());

        return win;
    }

    void renderer::upload_objects_sync(span<const ecs::archetype_entity> entities, const core::mesh_registry& meshes,
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

    void renderer::render() {
        _graph->execute();
    }

    renderer::renderer(unique_ptr<rhi::instance> instance, rhi::device& device, unique_ptr<pbr_frame_graph> graph)
        : _instance(tempest::move(instance)), _device(&device), _graph(tempest::move(graph))
    {
    }
} // namespace tempest::graphics