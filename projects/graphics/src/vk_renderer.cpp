#include <vulkan/vulkan.h>

#include "vk_renderer.hpp"

#include "glfw_window.hpp"

#include <GLFW/glfw3.h>

#include <vuk/CommandBuffer.hpp>
#include <vuk/Pipeline.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/Util.hpp>

#include <cassert>
#include <fstream>
#include <sstream>

namespace tempest::graphics::vk
{
    namespace
    {
        std::vector<uint32_t> read_spirv(const std::string& path)
        {
            std::ostringstream buf;
            std::ifstream input(path.c_str(), std::ios::ate | std::ios::binary);
            assert(input);
            size_t file_size = (size_t)input.tellg();
            std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
            input.seekg(0);
            input.read(reinterpret_cast<char*>(buffer.data()), file_size);
            return buffer;
        }

        instance create_instance()
        {
            auto instance_wrapper = instance(instance_factory::create_info{
                .name{"Tempest Renderer"},
                .version_major{0},
                .version_minor{0},
                .version_patch{1},
            });

            return instance_wrapper;
        }

        vuk::Swapchain make_swapchain(vkb::Device vkbdevice, std::optional<VkSwapchainKHR> old_swapchain)
        {
            vkb::SwapchainBuilder swb =
                vkb::SwapchainBuilder{vkbdevice}
                    .set_desired_format(
                        vuk::SurfaceFormatKHR{vuk::Format::eR8G8B8A8Srgb, vuk::ColorSpaceKHR::eSrgbNonlinear})
                    .add_fallback_format(
                        vuk::SurfaceFormatKHR{vuk::Format::eB8G8R8A8Srgb, vuk::ColorSpaceKHR::eSrgbNonlinear})
                    .set_desired_present_mode((VkPresentModeKHR)vuk::PresentModeKHR::eImmediate)
                    .set_image_usage_flags(VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                           VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            if (old_swapchain)
            {
                swb.set_old_swapchain(*old_swapchain);
            }
            auto vkswapchain = swb.build();

            vuk::Swapchain sw{};
            auto images = *vkswapchain->get_images();
            auto views = *vkswapchain->get_image_views();

            for (auto& i : images)
            {
                sw.images.push_back(vuk::Image{i, nullptr});
            }
            for (auto& i : views)
            {
                sw.image_views.emplace_back();
                sw.image_views.back().payload = i;
            }
            sw.extent = vuk::Extent2D{vkswapchain->extent.width, vkswapchain->extent.height};
            sw.format = vuk::Format(vkswapchain->image_format);
            sw.surface = vkbdevice.surface;
            sw.swapchain = vkswapchain->swapchain;
            return sw;
        }
    } // namespace

    renderer::renderer(iwindow& win) : _inst{create_instance()}
    {
        auto vkb_instance = _inst.raw();

        auto& device_wrapper = _inst.get_devices()[0];
        auto vkb_device = reinterpret_cast<device*>(device_wrapper.get())->raw();

        _graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
        auto graphics_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

        _transfer_queue = vkb_device.get_queue(vkb::QueueType::transfer).value();
        auto transfer_queue_family_index = vkb_device.get_dedicated_queue_index(vkb::QueueType::transfer).value();

        _compute_queue = vkb_device.get_queue(vkb::QueueType::compute).value();
        auto compute_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::compute).value();

        _ctx.emplace(vuk::ContextCreateParameters{
            .instance{vkb_instance.instance},
            .device{vkb_device.device},
            .physical_device{vkb_device.physical_device},
            .graphics_queue{_graphics_queue},
            .graphics_queue_family_index{graphics_queue_family_index},
            .compute_queue{_compute_queue},
            .compute_queue_family_index{compute_queue_family_index},
            .transfer_queue{_transfer_queue},
            .transfer_queue_family_index{transfer_queue_family_index},
            .pointers{},
        });

        _resources.emplace(*_ctx, FRAMES_IN_FLIGHT);
        _allocator.emplace(*_resources);

        _present_ready = vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>>(*_allocator);
        _render_complete = vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>>(*_allocator);
        _allocator->allocate_semaphores(*_present_ready);
        _allocator->allocate_semaphores(*_render_complete);

        _win = &static_cast<glfw::window&>(win);
        glfwCreateWindowSurface(vkb_instance.instance, _win->raw(), nullptr, &_surface);

        vkb_device.surface = _surface;
        _swapchain_ref = _ctx->add_swapchain(make_swapchain(vkb_device, std::nullopt));

        _resource_alloc = resource_allocator();
        _resource_alloc->set_context(*_ctx);
    }

    renderer::~renderer()
    {
        _release();
    }

    void renderer::execute(irenderer_graph& graph)
    {
        renderer_graph& vk_graph = static_cast<renderer_graph&>(graph);

        vuk::Allocator frame_alloc{*_resources};

        vuk::RenderGraph rg("tempest renderer runner");
        rg.attach_swapchain("_swp", _swapchain_ref);
        rg.clear_image("_swp", "tempest_render_graph_target", vuk::ClearColor{0.3f, 0.5f, 0.3f, 1.0f});
        auto fut = vk_graph.finalize(
            vuk::Future(std::make_shared<vuk::RenderGraph>(std::move(rg)), "tempest_render_graph_target"));
        auto ptr = fut.get_render_graph();
        auto erg = *_compiler.link(std::span{&ptr, 1}, {});
        auto bundle =
            *vuk::acquire_one(*_ctx, _swapchain_ref, (*_present_ready)[_ctx->get_frame_count() % FRAMES_IN_FLIGHT],
                              (*_render_complete)[_ctx->get_frame_count() % FRAMES_IN_FLIGHT]);
        auto result = *vuk::execute_submit(frame_alloc, std::move(erg), std::move(bundle));
        vuk::present_to_one(*_ctx, std::move(result));
    }

    void renderer::_release()
    {
        _present_ready.reset();
        _render_complete.reset();
        _resources.reset();
        _ctx.reset();

        vkb::destroy_surface(_inst.raw().instance, _surface, nullptr);
    }

    irenderer_graph& renderer_graph::set_final_target(render_target target)
    {
        _final_target_name = target.output_name;
        return *this;
    }

    irenderer_graph& renderer_graph::add_pass(const render_pass& pass)
    {
        vuk::Pass p;
        p.resources.reserve(pass.resources.size());

        for (auto& target : pass.resources)
        {
            p.resources.push_back(vuk::Resource{
                vuk::Name(target.name), vuk::Resource::Type::eImage, static_cast<vuk::Access>(target.type),
                !target.output_name.empty() ? vuk::Name(target.output_name) : vuk::Name()});
        }

        auto fn = pass.execute;

        p.execute = [=](vuk::CommandBuffer& buf) {
            command_buffer b{&buf};
            fn(b);
        };

        _vuk_graph.add_pass(p);

        return *this;
    }

    vuk::Future renderer_graph::finalize(vuk::Future back_buffer)
    {
        _vuk_graph.attach_in(irenderer_graph::BACK_BUFFER, std::move(back_buffer));
        vuk::Future fut{std::make_unique<vuk::RenderGraph>(std::move(_vuk_graph)), _final_target_name};
        _vuk_graph = {"Tempest Renderer"};
        return fut;
    }
    
    void resource_allocator::set_context(vuk::Context& ctx)
    {
        _ctx = std::ref(ctx);
    }

    void resource_allocator::create_named_pipeline(std::span<shader_source> sources, std::string_view name)
    {
        vuk::PipelineBaseCreateInfo pci;
        for (const auto& src : sources)
        {
            pci.add_spirv(src.data, std::string(src.name));
        }

        _ctx.value().get().create_named_pipeline(name, pci);
    }
    
    command_buffer::command_buffer(vuk::CommandBuffer* buf) : _buf{buf}
    {
    }

    icommand_buffer& command_buffer::use_full_viewport(std::uint32_t vp_index)
    {
        _buf->set_viewport(vp_index, vuk::Rect2D::framebuffer());
        return *this;
    }
    
    icommand_buffer& command_buffer::use_full_scissor(std::uint32_t sc_index)
    {
        _buf->set_scissor(sc_index, vuk::Rect2D::framebuffer());
        return *this;
    }
    
    icommand_buffer& command_buffer::use_default_raster_state()
    {
        _buf->set_rasterization({});
        return *this;
    }
    
    icommand_buffer& command_buffer::use_default_color_blend(std::string_view render_target_name)
    {
        _buf->set_color_blend(render_target_name, {});
        return *this;
    }
    
    icommand_buffer& command_buffer::use_graphics_pipeline(std::string_view pipeline_name)
    {
        _buf->bind_graphics_pipeline(pipeline_name);
        return *this;
    }
    
    icommand_buffer& command_buffer::draw(std::uint32_t vertex_count, std::uint32_t instance_count,
                                          std::uint32_t first_vertex, std::uint32_t first_instance)
    {
        _buf->draw(vertex_count, instance_count, first_vertex, first_instance);
        return *this;
    }
} // namespace tempest::graphics::vk