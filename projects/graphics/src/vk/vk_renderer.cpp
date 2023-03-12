#include <tempest/renderer.hpp>

#include "vk_renderer.hpp"

#include "../glfw_window.hpp"

#include <tempest/logger.hpp>

#include <vuk/RenderGraph.hpp>
#include <vuk/Swapchain.hpp>

namespace tempest::graphics
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"[tempest::graphics::vk_renderer]"}});

        vkb::Instance create_instance(const core::version& info)
        {
            vkb::InstanceBuilder bldr = vkb::InstanceBuilder{}
                                            .set_engine_name("Tempest Rendering Engine")
                                            .set_engine_version(0, 0, 1)
                                            .set_app_name("Tempest Rendering Application")
                                            .set_app_version(info.major, info.minor, info.patch)
                                            .require_api_version(1, 3, 0);
#ifdef _DEBUG
            bldr.request_validation_layers().set_debug_callback(
                [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VkBool32 {
                    switch (messageSeverity)
                    {
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
                        logger->error("{0}", pCallbackData->pMessage);
                        break;
                    }
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
                        logger->warn("{0}", pCallbackData->pMessage);
                        break;
                    }
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
                        logger->info("{0}", pCallbackData->pMessage);
                        break;
                    }
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: {
                        logger->debug("{0}", pCallbackData->pMessage);
                        break;
                    }
                    }
                    return VK_FALSE;
                });
#endif
            auto result = bldr.build();
            if (!result)
            {
                logger->error("Failed to create VkInstance. VkResult: {0}",
                              static_cast<std::underlying_type_t<VkResult>>(result.full_error().vk_result));
            }

            return *result;
        }

        vkb::PhysicalDevice select_physical_device(const vkb::Instance& instance, VkSurfaceKHR surface)
        {
            auto result = vkb::PhysicalDeviceSelector{instance}
                              .add_required_extensions({VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
                                                        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME})
                              .require_present()
                              .set_surface(surface)
                              .set_required_features({
                                  .shaderInt64{VK_TRUE},
                              })
                              .set_required_features_11({
                                  .shaderDrawParameters{VK_TRUE},
                              })
                              .set_required_features_12({
                                  .shaderSampledImageArrayNonUniformIndexing{VK_TRUE},
                                  .descriptorBindingUpdateUnusedWhilePending{VK_TRUE},
                                  .descriptorBindingPartiallyBound{VK_TRUE},
                                  .descriptorBindingVariableDescriptorCount{VK_TRUE},
                                  .runtimeDescriptorArray{VK_TRUE},
                                  .hostQueryReset{VK_TRUE},
                                  .timelineSemaphore{VK_TRUE},
                                  .bufferDeviceAddress{VK_TRUE},
                                  .shaderOutputLayer{VK_TRUE},
                              })
                              .set_minimum_version(1, 2)
                              .select();
            if (!result)
            {
                logger->error("Failed to fetch suitable VkPhysicalDevice: {0}",
                              static_cast<std::underlying_type_t<VkResult>>(result.full_error().vk_result));
            }

            return *result;
        }

        vkb::Device create_device(const vkb::PhysicalDevice& physical)
        {
            VkPhysicalDeviceSynchronization2Features sync_feats = {
                .sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES},
                .synchronization2{VK_TRUE},
            };

            auto result = vkb::DeviceBuilder{physical}.add_pNext(&sync_feats).build();
            if (!result)
            {
                logger->error("Failed to build VkDevice: {0}",
                              static_cast<std::underlying_type_t<VkResult>>(result.full_error().vk_result));
            }

            return *result;
        }

        VkSurfaceKHR fetch_surface(vkb::Instance& inst, iwindow& win)
        {
            glfw::window& glfw_win = static_cast<glfw::window&>(win);
            auto native = glfw_win.raw();

            VkSurfaceKHR surface;
            auto surface_result = glfwCreateWindowSurface(inst.instance, native, nullptr, &surface);

            return surface;
        }

        vuk::Swapchain create_swapchain(vkb::Device device, std::optional<VkSwapchainKHR> previous)
        {
            auto swapchain_result =
                vkb::SwapchainBuilder{device}
                    .set_desired_format(
                        vuk::SurfaceFormatKHR{vuk::Format::eR8G8B8A8Srgb, vuk::ColorSpaceKHR::eSrgbNonlinear})
                    .add_fallback_format(
                        vuk::SurfaceFormatKHR{vuk::Format::eR8G8B8A8Srgb, vuk::ColorSpaceKHR::eSrgbNonlinear})
                    .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                    .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                    .set_old_swapchain(previous ? *previous : nullptr)
                    .build();

            if (!swapchain_result)
            {
                logger->error("Failed to create VkSwapchainKHR.");
            }
            auto swapchain = *swapchain_result;

            auto images_result = swapchain.get_images();
            auto views_result = swapchain.get_image_views();

            if (!images_result || !views_result)
            {
                logger->error("Failed to fetch VkImages for VkSwapchainKHR.");
            }

            vuk::Swapchain swap{};

            for (auto& i : *images_result)
            {
                swap.images.emplace_back(i, nullptr);
            }

            for (auto& v : *views_result)
            {
                swap.image_views.emplace_back().payload = v;
            }

            swap.extent = {swapchain.extent.width, swapchain.extent.height};
            swap.format = vuk::Format{swapchain.image_format};
            swap.surface = device.surface;
            swap.swapchain = swapchain.swapchain;

            return swap;
        }

    } // namespace

    std::unique_ptr<irenderer> irenderer::create(const core::version& version_info, iwindow& win)
    {
        return std::unique_ptr<irenderer>(new irenderer(version_info, win));
    }

    irenderer::irenderer(const core::version& version_info, iwindow& win) : _impl{new impl()}
    {
        _impl->instance = create_instance(version_info);
        _impl->surface = fetch_surface(_impl->instance, win);
        _impl->physical_device = select_physical_device(_impl->instance, _impl->surface);
        _impl->logical_device = create_device(_impl->physical_device);

        auto graphics_queue = _impl->logical_device.get_queue(vkb::QueueType::graphics).value();
        auto graphics_queue_index = _impl->logical_device.get_queue_index(vkb::QueueType::graphics).value();
        auto compute_queue = _impl->logical_device.get_queue(vkb::QueueType::compute).value();
        auto compute_queue_index = _impl->logical_device.get_queue_index(vkb::QueueType::compute).value();
        auto transfer_queue = _impl->logical_device.get_queue(vkb::QueueType::transfer).value();
        auto transfer_queue_index = _impl->logical_device.get_queue_index(vkb::QueueType::transfer).value();

        _impl->gfx_queue = graphics_queue;
        _impl->compute_queue = compute_queue;
        _impl->transfer_queue = transfer_queue;

        vuk::ContextCreateParameters::FunctionPointers context_fps = {
            .vkSetDebugUtilsObjectNameEXT{
                reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(_impl->logical_device.fp_vkGetDeviceProcAddr(
                    _impl->logical_device.device, "vkSetDebugUtilsObjectNameEXT"))},
            .vkCmdBeginDebugUtilsLabelEXT{
                reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(_impl->logical_device.fp_vkGetDeviceProcAddr(
                    _impl->logical_device.device, "vkCmdBeginDebugUtilsLabelEXT"))},
            .vkCmdEndDebugUtilsLabelEXT{
                reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(_impl->logical_device.fp_vkGetDeviceProcAddr(
                    _impl->logical_device.device, "vkCmdEndDebugUtilsLabelEXT"))},
        };

        _impl->vuk_context.emplace(vuk::ContextCreateParameters{
            .instance{_impl->instance.instance},
            .device{_impl->logical_device.device},
            .physical_device{_impl->physical_device.physical_device},
            .graphics_queue{_impl->gfx_queue},
            .graphics_queue_family_index{graphics_queue_index},
            .compute_queue{_impl->compute_queue},
            .compute_queue_family_index{compute_queue_index},
            .transfer_queue{_impl->transfer_queue},
            .transfer_queue_family_index{transfer_queue_index},
            .pointers{context_fps},
        });

        _impl->superframe_resources.emplace(*_impl->vuk_context, FRAMES_IN_FLIGHT);
        _impl->vuk_allocator.emplace(*_impl->superframe_resources);
        _impl->swapchain = _impl->vuk_context->add_swapchain(create_swapchain(_impl->logical_device, std::nullopt));

        _impl->present_ready = vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>>(*_impl->vuk_allocator);
        _impl->render_complete = vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>>(*_impl->vuk_allocator);

        _impl->vuk_allocator->allocate_semaphores(*_impl->present_ready);
        _impl->vuk_allocator->allocate_semaphores(*_impl->render_complete);
    }

    irenderer::~irenderer()
    {
        auto device = _impl->logical_device;
        auto instance = _impl->instance;
        auto surface = _impl->surface;

        _impl.reset();

        vkb::destroy_surface(instance, surface);
        vkb::destroy_device(device);
        vkb::destroy_instance(instance);
    }

    void irenderer::render()
    {
        auto& frame_resource = _impl->superframe_resources->get_next_frame();
        _impl->vuk_context->next_frame();

        vuk::Allocator frame_allocator{frame_resource};
        auto bundle =
            *vuk::acquire_one(*_impl->vuk_context, _impl->swapchain,
                              (*_impl->present_ready)[_impl->vuk_context->get_frame_count() % FRAMES_IN_FLIGHT],
                              (*_impl->render_complete)[_impl->vuk_context->get_frame_count() % FRAMES_IN_FLIGHT]);

        auto render_graph = std::make_shared<vuk::RenderGraph>("runner");
        render_graph->attach_swapchain("_swp", _impl->swapchain);
        render_graph->clear_image("_swp", "DEFAULT_BACK_BUFFER", vuk::ClearColor{0.3f, 0.5f, 0.3f, 1.0f});
        
        vuk::Future cleared_image_to_render_to{std::move(render_graph), "DEFAULT_BACK_BUFFER"};
        
        auto ptr = cleared_image_to_render_to.get_render_graph();
        auto erg = *_impl->compiler.link(std::span{&ptr, 1}, {});
        auto result = *vuk::execute_submit(frame_allocator, std::move(erg), std::move(bundle));
        vuk::present_to_one(*_impl->vuk_context, std::move(result));
    }
} // namespace tempest::graphics