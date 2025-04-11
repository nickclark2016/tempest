#include <tempest/vk/rhi.hpp>

#include "window.hpp"

#include <tempest/flat_unordered_map.hpp>
#include <tempest/logger.hpp>
#include <tempest/optional.hpp>
#include <tempest/tuple.hpp>

namespace tempest::rhi::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::vk::render_device"}});

        [[maybe_unused]] VKAPI_ATTR VkBool32 VKAPI_CALL
        debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                       [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, [[maybe_unused]] void* pUserData)
        {
            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                logger->error("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            {
                logger->warn("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            {
                logger->info("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            {
                logger->debug("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else
            {
                logger->debug("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }

            return VK_FALSE;
        }

        constexpr VkPresentModeKHR to_vulkan(rhi::present_mode mode)
        {
            switch (mode)
            {
            case rhi::present_mode::IMMEDIATE:
                return VK_PRESENT_MODE_IMMEDIATE_KHR;
            case rhi::present_mode::MAILBOX:
                return VK_PRESENT_MODE_MAILBOX_KHR;
            case rhi::present_mode::FIFO:
                return VK_PRESENT_MODE_FIFO_KHR;
            case rhi::present_mode::FIFO_RELAXED:
                return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            default:
                logger->critical("Invalid present mode: {}", static_cast<uint32_t>(mode));
                terminate();
            }
        }

        constexpr VkFormat to_vulkan(rhi::image_format fmt)
        {
            switch (fmt)
            {
            case rhi::image_format::R8_UNORM:
                return VK_FORMAT_R8_UNORM;
            case rhi::image_format::R8_SNORM:
                return VK_FORMAT_R8_SNORM;
            case rhi::image_format::R16_UNORM:
                return VK_FORMAT_R16_UNORM;
            case rhi::image_format::R16_SNORM:
                return VK_FORMAT_R16_SNORM;
            case rhi::image_format::R16_FLOAT:
                return VK_FORMAT_R16_SFLOAT;
            case rhi::image_format::R32_FLOAT:
                return VK_FORMAT_R32_SFLOAT;
            case rhi::image_format::RG8_UNORM:
                return VK_FORMAT_R8G8_UNORM;
            case rhi::image_format::RG8_SNORM:
                return VK_FORMAT_R8G8_SNORM;
            case rhi::image_format::RG16_UNORM:
                return VK_FORMAT_R16G16_UNORM;
            case rhi::image_format::RG16_SNORM:
                return VK_FORMAT_R16G16_SNORM;
            case rhi::image_format::RG16_FLOAT:
                return VK_FORMAT_R16G16_SFLOAT;
            case rhi::image_format::RG32_FLOAT:
                return VK_FORMAT_R32G32_SFLOAT;
            case rhi::image_format::RGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case rhi::image_format::RGBA8_SNORM:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case rhi::image_format::RGBA8_SRGB:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case rhi::image_format::BGRA8_SRGB:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case rhi::image_format::RGBA16_UNORM:
                return VK_FORMAT_R16G16B16A16_UNORM;
            case rhi::image_format::RGBA16_SNORM:
                return VK_FORMAT_R16G16B16A16_SNORM;
            case rhi::image_format::RGBA16_FLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case rhi::image_format::RGBA32_FLOAT:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case rhi::image_format::S8_UINT:
                return VK_FORMAT_S8_UINT;
            case rhi::image_format::D16_UNORM:
                return VK_FORMAT_D16_UNORM;
            case rhi::image_format::D24_UNORM:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case rhi::image_format::D32_FLOAT:
                return VK_FORMAT_D32_SFLOAT;
            case rhi::image_format::D16_UNORM_S8_UINT:
                return VK_FORMAT_D16_UNORM_S8_UINT;
            case rhi::image_format::D24_UNORM_S8_UINT:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case rhi::image_format::D32_FLOAT_S8_UINT:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case rhi::image_format::A2BGR10_UNORM_PACK32:
                return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            default:
                logger->critical("Invalid image format: {}", static_cast<uint32_t>(fmt));
                terminate();
            }
        }

        VkColorSpaceKHR to_vulkan(rhi::color_space color_space)
        {
            switch (color_space)
            {
            case rhi::color_space::ADOBE_RGB_LINEAR:
                return VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT;
            case rhi::color_space::ADOBE_RGB_NONLINEAR:
                return VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT;
            case rhi::color_space::BT709_LINEAR:
                return VK_COLOR_SPACE_BT709_LINEAR_EXT;
            case rhi::color_space::BT709_NONLINEAR:
                return VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
            case rhi::color_space::BT2020_LINEAR:
                return VK_COLOR_SPACE_BT2020_LINEAR_EXT;
            case rhi::color_space::DCI_P3_NONLINEAR:
                return VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT;
            case rhi::color_space::DISPLAY_NATIVE_AMD:
                return VK_COLOR_SPACE_DISPLAY_NATIVE_AMD;
            case rhi::color_space::DISPLAY_P3_LINEAR:
                return VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT;
            case rhi::color_space::DISPLAY_P3_NONLINEAR:
                return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
            case rhi::color_space::EXTENDED_SRGB_LINEAR:
                return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
            case rhi::color_space::EXTENDED_SRGB_NONLINEAR:
                return VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT;
            case rhi::color_space::HDR10_HLG:
                return VK_COLOR_SPACE_HDR10_HLG_EXT;
            case rhi::color_space::HDR10_ST2084:
                return VK_COLOR_SPACE_HDR10_ST2084_EXT;
            case rhi::color_space::PASS_THROUGH:
                return VK_COLOR_SPACE_PASS_THROUGH_EXT;
            case rhi::color_space::SRGB_NONLINEAR:
                return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            default:
                logger->critical("Invalid color space: {}", static_cast<uint32_t>(color_space));
                terminate();
            }
        }
    } // namespace

    instance::instance(vkb::Instance instance, vector<vkb::PhysicalDevice> devices) noexcept
        : _vkb_instance(tempest::move(instance)), _vkb_phys_devices(tempest::move(devices))
    {
        _devices.resize(_vkb_phys_devices.size());
    }

    instance::~instance()
    {
        _devices.clear(); // Devices must be released before destroying the instance
        vkb::destroy_instance(_vkb_instance);
    }

    vector<rhi_device_description> instance::get_devices() const noexcept
    {
        vector<rhi_device_description> devices;

        for (size_t i = 0; i < _vkb_phys_devices.size(); ++i)
        {
            devices.push_back({
                .device_index = static_cast<uint32_t>(i),
                .device_name = _vkb_phys_devices[i].name.c_str(),
            });
        }

        return devices;
    }

    rhi::device& instance::acquire_device(uint32_t device_index) noexcept
    {
        if (_devices[device_index] == nullptr)
        {
            vkb::DeviceBuilder bldr(_vkb_phys_devices[device_index]);
            auto result = bldr.build();
            if (!result)
            {
                terminate();
            }

            _devices[device_index] = make_unique<rhi::vk::device>(tempest::move(result).value(), &_vkb_instance);
        }

        return *_devices[device_index];
    }

    device::device(vkb::Device dev, vkb::Instance* instance)
        : _vkb_instance{instance}, _vkb_device{tempest::move(dev)}, _dispatch_table{dev.make_table()}
    {
        auto& queue_families = _vkb_device.queue_families;
        flat_unordered_map<uint32_t, uint32_t> queues_allocated;

        auto family_matcher = [&](VkQueueFlags flags) -> optional<tuple<VkQueueFamilyProperties, uint32_t, uint32_t>> {
            optional<tuple<VkQueueFamilyProperties, uint32_t, uint32_t>> best_match;

            uint32_t family_idx = 0;
            for (const auto& family : queue_families)
            {
                if (family.queueFlags == flags)
                {
                    auto index = queues_allocated[family_idx] < family.queueCount ? queues_allocated[family_idx]++ : 0;

                    return make_tuple(family, family_idx, index);
                }
                else if ((family.queueFlags & flags) == flags)
                {
                    if (best_match)
                    {
                        auto idx = get<1>(*best_match);
                        queues_allocated[idx]--;
                    }

                    auto index = queues_allocated[family_idx] < family.queueCount ? queues_allocated[family_idx]++ : 0;

                    best_match = make_tuple(family, family_idx, index);
                }

                family_idx++;
            }

            return best_match;
        };

        auto default_queue_match = family_matcher(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
        auto compute_queue_match = family_matcher(VK_QUEUE_COMPUTE_BIT);
        auto transfer_queue_match = family_matcher(VK_QUEUE_TRANSFER_BIT);

        if (default_queue_match)
        {
            VkQueue queue;
            _dispatch_table.getDeviceQueue(get<1>(*default_queue_match), get<2>(*default_queue_match), &queue);
            _primary_work_queue.emplace(&_dispatch_table, queue, get<1>(*default_queue_match));
        }
        else
        {
            logger->critical("Failed to find a suitable queue family for the device.");
            terminate();
        }

        if (compute_queue_match && get<1>(*compute_queue_match) != get<1>(*default_queue_match))
        {
            VkQueue queue;
            _dispatch_table.getDeviceQueue(get<1>(*compute_queue_match), get<2>(*compute_queue_match), &queue);
            _dedicated_compute_queue.emplace(&_dispatch_table, queue, get<1>(*compute_queue_match));
        }

        if (transfer_queue_match && get<1>(*transfer_queue_match) != get<1>(*default_queue_match))
        {
            VkQueue queue;
            _dispatch_table.getDeviceQueue(get<1>(*transfer_queue_match), get<2>(*transfer_queue_match), &queue);
            _dedicated_transfer_queue.emplace(&_dispatch_table, queue, get<1>(*transfer_queue_match));
        }
    }

    device::~device()
    {
        _dispatch_table.deviceWaitIdle();

        for (auto img : _images)
        {
            if (img.image_view)
            {
                _dispatch_table.destroyImageView(img.image_view, nullptr);
            }
            if (img.image && !img.swapchain_image)
            {
                _dispatch_table.destroyImage(img.image, nullptr);
            }
        }
        _images.clear();

        for (auto sc : _swapchains)
        {
            vkb::destroy_swapchain(sc.swapchain);
            vkb::destroy_surface(_vkb_instance->instance, sc.surface);
        }
        _swapchains.clear();

        vkb::destroy_device(_vkb_device);
    }

    typed_rhi_handle<rhi_handle_type::buffer> device::create_buffer(const buffer_desc& desc) noexcept
    {
        return typed_rhi_handle<rhi_handle_type::buffer>::null_handle;
    }

    typed_rhi_handle<rhi_handle_type::image> device::create_image(const image_desc& desc) noexcept
    {
        return typed_rhi_handle<rhi_handle_type::image>::null_handle;
    }

    typed_rhi_handle<rhi_handle_type::fence> device::create_fence(const fence_info& info) noexcept
    {
        return typed_rhi_handle<rhi_handle_type::fence>::null_handle;
    }

    typed_rhi_handle<rhi_handle_type::semaphore> device::create_semaphore(const semaphore_info& info) noexcept
    {
        return typed_rhi_handle<rhi_handle_type::semaphore>::null_handle;
    }

    typed_rhi_handle<rhi_handle_type::render_surface> device::create_render_surface(
        const render_surface_desc& desc) noexcept
    {
        auto existing_swapchain_key =
            create_slot_map_key<uint64_t>(desc.render_surface.id, desc.render_surface.generation);
        auto existing_swapchain_it = _swapchains.find(existing_swapchain_key);

        VkSurfaceKHR surface;
        if (existing_swapchain_it != _swapchains.end())
        {
            surface = existing_swapchain_it->surface;
        }
        else
        {
            auto window = static_cast<vk::window_surface*>(desc.window);
            auto surf_res = window->get_surface(_vkb_instance->instance);
            if (!surf_res)
            {
                logger->error("Failed to create render surface for window: {}", desc.window->name().c_str());
                return typed_rhi_handle<rhi_handle_type::render_surface>::null_handle;
            }

            surface = surf_res.value();
        }

        vkb::SwapchainBuilder swap_bldr =
            vkb::SwapchainBuilder(_vkb_device.physical_device, _vkb_device, surface)
                .add_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .set_required_min_image_count(desc.min_image_count)
                .set_desired_extent(desc.width, desc.height)
                .set_desired_present_mode(to_vulkan(desc.present_mode))
                .set_desired_format({
                    .format = to_vulkan(desc.format.format),
                    .colorSpace = to_vulkan(desc.format.space),
                })
                .set_image_array_layer_count(desc.layers);

        auto result = swap_bldr.build();
        if (!result)
        {
            return typed_rhi_handle<rhi_handle_type::render_surface>::null_handle;
        }

        auto vkb_sc = result.value();

        swapchain sc = {
            .swapchain = vkb_sc,
            .surface = surface,
            .images = {},
        };

        auto images_result = sc.swapchain.get_images();
        if (!images_result)
        {
            return typed_rhi_handle<rhi_handle_type::render_surface>::null_handle;
        }

        auto images = images_result.value();
        sc.images.reserve(images.size());

        auto image_views_result = sc.swapchain.get_image_views();
        if (!image_views_result)
        {
            return typed_rhi_handle<rhi_handle_type::render_surface>::null_handle;
        }
        
        auto image_views = image_views_result.value();

        for (size_t i = 0; i < images.size(); ++i)
        {
            sc.images.push_back(acquire_image({
                .image = images[i],
                .image_view = image_views[i],
                .swapchain_image = true,
            }));
        }

        auto new_key = _swapchains.insert(sc);

        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::render_surface>(new_key_id, new_key_gen);
    }

    void device::destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept
    {
    }

    void device::destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept
    {
    }

    void device::destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept
    {
    }

    void device::destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept
    {
    }

    void device::destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept
    {
    }

    rhi::work_queue& device::get_primary_work_queue() noexcept
    {
        return *_primary_work_queue;
    }

    rhi::work_queue& device::get_dedicated_transfer_queue() noexcept
    {
        if (_dedicated_transfer_queue)
        {
            return *_dedicated_transfer_queue;
        }
        else
        {
            return *_primary_work_queue;
        }
    }

    rhi::work_queue& device::get_dedicated_compute_queue() noexcept
    {
        if (_dedicated_compute_queue)
        {
            return *_dedicated_compute_queue;
        }
        else
        {
            return *_primary_work_queue;
        }
    }

    render_surface_info device::query_render_surface_info(const rhi::window_surface& window) noexcept
    {
        return render_surface_info{};
    }

    span<const typed_rhi_handle<rhi_handle_type::image>> device::get_render_surfaces(
        typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept
    {
        return span<const typed_rhi_handle<rhi_handle_type::image>>{};
    }

    expected<swapchain_image_acquire_info_result, swapchain_error_code> device::acquire_next_image(
        typed_rhi_handle<rhi_handle_type::semaphore> signal_sem,
        typed_rhi_handle<rhi_handle_type::fence> signal_fence) noexcept
    {
        return unexpected{swapchain_error_code::FAILURE};
    }

    typed_rhi_handle<rhi_handle_type::image> device::acquire_image(image img) noexcept
    {
        auto new_key = _images.insert(img);

        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::image>(new_key_id, new_key_gen);
    }

    work_queue::work_queue(vkb::DispatchTable* dispatch, VkQueue queue, uint32_t queue_family_index) noexcept
        : _dispatch(dispatch), _queue(queue), _queue_family_index(queue_family_index)
    {
    }

    work_queue::~work_queue()
    {
        // TODO: Release command pool and other resources
    }

    typed_rhi_handle<rhi_handle_type::command_list> work_queue::get_command_list() noexcept
    {
        return typed_rhi_handle<rhi_handle_type::command_list>::null_handle;
    }

    bool work_queue::submit(span<const submit_info> infos, typed_rhi_handle<rhi_handle_type::fence> fence) noexcept
    {
        return false;
    }

    bool work_queue::present(const present_info& info) noexcept
    {
        return false;
    }

    unique_ptr<rhi::instance> create_instance() noexcept
    {
        vkb::InstanceBuilder bldr;
        bldr.set_app_name("Tempest Application")
            .set_app_version(0, 1, 0)
            .set_engine_name("Tempest Engine")
            .set_engine_version(0, 1, 0)
            .require_api_version(1, 3, 0);

#if defined(_DEBUG)
        bldr.enable_validation_layers(true)
            .set_debug_callback(debug_callback)
            .add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            .add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            .add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            .add_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
            .add_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
            .add_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
            .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
            .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
#if defined(TEMPEST_ENABLE_GPU_ASSISTED_VALDATION)
        bldr.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
#endif
#endif
        auto result = bldr.build();
        if (!result)
        {
            return nullptr;
        }

        auto instance = tempest::move(result).value();

        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extended_dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT,
            .pNext = nullptr,
            .extendedDynamicState3TessellationDomainOrigin = VK_FALSE,
            .extendedDynamicState3DepthClampEnable = VK_FALSE,
            .extendedDynamicState3PolygonMode = VK_FALSE,
            .extendedDynamicState3RasterizationSamples = VK_TRUE,
            .extendedDynamicState3SampleMask = VK_FALSE,
            .extendedDynamicState3AlphaToCoverageEnable = VK_FALSE,
            .extendedDynamicState3AlphaToOneEnable = VK_FALSE,
            .extendedDynamicState3LogicOpEnable = VK_FALSE,
            .extendedDynamicState3ColorBlendEnable = VK_FALSE,
            .extendedDynamicState3ColorBlendEquation = VK_FALSE,
            .extendedDynamicState3ColorWriteMask = VK_FALSE,
            .extendedDynamicState3RasterizationStream = VK_FALSE,
            .extendedDynamicState3ConservativeRasterizationMode = VK_FALSE,
            .extendedDynamicState3ExtraPrimitiveOverestimationSize = VK_FALSE,
            .extendedDynamicState3DepthClipEnable = VK_FALSE,
            .extendedDynamicState3SampleLocationsEnable = VK_FALSE,
            .extendedDynamicState3ColorBlendAdvanced = VK_FALSE,
            .extendedDynamicState3ProvokingVertexMode = VK_FALSE,
            .extendedDynamicState3LineRasterizationMode = VK_FALSE,
            .extendedDynamicState3LineStippleEnable = VK_FALSE,
            .extendedDynamicState3DepthClipNegativeOneToOne = VK_FALSE,
            .extendedDynamicState3ViewportWScalingEnable = VK_FALSE,
            .extendedDynamicState3ViewportSwizzle = VK_FALSE,
            .extendedDynamicState3CoverageToColorEnable = VK_FALSE,
            .extendedDynamicState3CoverageToColorLocation = VK_FALSE,
            .extendedDynamicState3CoverageModulationMode = VK_FALSE,
            .extendedDynamicState3CoverageModulationTableEnable = VK_FALSE,
            .extendedDynamicState3CoverageModulationTable = VK_FALSE,
            .extendedDynamicState3CoverageReductionMode = VK_FALSE,
            .extendedDynamicState3RepresentativeFragmentTestEnable = VK_FALSE,
            .extendedDynamicState3ShadingRateImageEnable = VK_FALSE,
        };

        VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragment_shader_interlock = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT,
            .pNext = nullptr,
            .fragmentShaderSampleInterlock = VK_TRUE,
            .fragmentShaderPixelInterlock = VK_TRUE,
            .fragmentShaderShadingRateInterlock = VK_FALSE,
        };

        vkb::PhysicalDeviceSelector selector =
            vkb::PhysicalDeviceSelector(instance)
                .prefer_gpu_device_type(vkb::PreferredDeviceType::integrated)
                .defer_surface_initialization()
                .require_present()
                .add_required_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME)
                .add_required_extension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME)
                .set_minimum_version(1, 3)
                .set_required_features({
#ifdef _DEBUG
                    .robustBufferAccess = VK_TRUE,
#else
                    .robustBufferAccess = VK_FALSE,
#endif
                    .fullDrawIndexUint32 = VK_FALSE,
                    .imageCubeArray = VK_FALSE,
                    .independentBlend = VK_TRUE,
                    .geometryShader = VK_FALSE,
                    .tessellationShader = VK_FALSE,
                    .sampleRateShading = VK_FALSE,
                    .dualSrcBlend = VK_FALSE,
                    .logicOp = VK_TRUE,
                    .multiDrawIndirect = VK_TRUE,
                    .drawIndirectFirstInstance = VK_TRUE,
                    .depthClamp = VK_TRUE,
                    .depthBiasClamp = VK_TRUE,
                    .fillModeNonSolid = VK_TRUE,
                    .depthBounds = VK_TRUE,
                    .wideLines = VK_FALSE,
                    .largePoints = VK_FALSE,
                    .alphaToOne = VK_FALSE,
                    .multiViewport = VK_FALSE,
                    .samplerAnisotropy = VK_TRUE,
                    .textureCompressionETC2 = VK_FALSE,
                    .textureCompressionASTC_LDR = VK_FALSE,
                    .textureCompressionBC = VK_FALSE,
                    .occlusionQueryPrecise = VK_FALSE,
                    .pipelineStatisticsQuery = VK_TRUE,
                    .vertexPipelineStoresAndAtomics = VK_FALSE,
                    .fragmentStoresAndAtomics = VK_TRUE,
                    .shaderTessellationAndGeometryPointSize = VK_FALSE,
                    .shaderImageGatherExtended = VK_FALSE,
                    .shaderStorageImageExtendedFormats = VK_FALSE,
                    .shaderStorageImageMultisample = VK_FALSE,
                    .shaderStorageImageReadWithoutFormat = VK_FALSE,
                    .shaderStorageImageWriteWithoutFormat = VK_FALSE,
                    .shaderUniformBufferArrayDynamicIndexing = VK_TRUE,
                    .shaderSampledImageArrayDynamicIndexing = VK_TRUE,
                    .shaderStorageBufferArrayDynamicIndexing = VK_TRUE,
                    .shaderStorageImageArrayDynamicIndexing = VK_TRUE,
                    .shaderClipDistance = VK_FALSE,
                    .shaderCullDistance = VK_FALSE,
                    .shaderFloat64 = VK_FALSE,
                    .shaderInt64 = VK_FALSE,
                    .shaderInt16 = VK_TRUE,
                    .shaderResourceResidency = VK_FALSE,
                    .shaderResourceMinLod = VK_FALSE,
                    .sparseBinding = VK_FALSE,
                    .sparseResidencyBuffer = VK_FALSE,
                    .sparseResidencyImage2D = VK_FALSE,
                    .sparseResidencyImage3D = VK_FALSE,
                    .sparseResidency2Samples = VK_FALSE,
                    .sparseResidency4Samples = VK_FALSE,
                    .sparseResidency8Samples = VK_FALSE,
                    .sparseResidency16Samples = VK_FALSE,
                    .sparseResidencyAliased = VK_FALSE,
                    .variableMultisampleRate = VK_FALSE,
                    .inheritedQueries = VK_FALSE,
                })
                .set_required_features_11({
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                    .pNext = nullptr,
                    .storageBuffer16BitAccess = VK_TRUE,
                    .uniformAndStorageBuffer16BitAccess = VK_TRUE,
                    .storagePushConstant16 = VK_FALSE,
                    .storageInputOutput16 = VK_FALSE,
                    .multiview = VK_FALSE,
                    .multiviewGeometryShader = VK_FALSE,
                    .multiviewTessellationShader = VK_FALSE,
                    .variablePointersStorageBuffer = VK_FALSE,
                    .variablePointers = VK_FALSE,
                    .protectedMemory = VK_FALSE,
                    .samplerYcbcrConversion = VK_FALSE,
                    .shaderDrawParameters = VK_TRUE,
                })
                .set_required_features_12({
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                    .pNext = nullptr,
                    .samplerMirrorClampToEdge = VK_FALSE,
                    .drawIndirectCount = VK_FALSE,
                    .storageBuffer8BitAccess = VK_TRUE,
                    .uniformAndStorageBuffer8BitAccess = VK_TRUE,
                    .storagePushConstant8 = VK_FALSE,
                    .shaderBufferInt64Atomics = VK_FALSE,
                    .shaderSharedInt64Atomics = VK_FALSE,
                    .shaderFloat16 = VK_TRUE,
                    .shaderInt8 = VK_FALSE,
                    .descriptorIndexing = VK_FALSE,
                    .shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
                    .shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
                    .shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
                    .shaderUniformBufferArrayNonUniformIndexing = VK_TRUE,
                    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
                    .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
                    .shaderStorageImageArrayNonUniformIndexing = VK_TRUE,
                    .shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
                    .shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE,
                    .shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE,
                    .descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE,
                    .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
                    .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
                    .descriptorBindingStorageBufferUpdateAfterBind = VK_FALSE,
                    .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
                    .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
                    .descriptorBindingUpdateUnusedWhilePending = VK_FALSE,
                    .descriptorBindingPartiallyBound = VK_TRUE,
                    .descriptorBindingVariableDescriptorCount = VK_TRUE,
                    .runtimeDescriptorArray = VK_TRUE,
                    .samplerFilterMinmax = VK_FALSE,
                    .scalarBlockLayout = VK_FALSE,
                    .imagelessFramebuffer = VK_TRUE,
                    .uniformBufferStandardLayout = VK_TRUE,
                    .shaderSubgroupExtendedTypes = VK_FALSE,
                    .separateDepthStencilLayouts = VK_TRUE,
                    .hostQueryReset = VK_TRUE,
                    .timelineSemaphore = VK_FALSE,
                    .bufferDeviceAddress = VK_TRUE,
                    .bufferDeviceAddressCaptureReplay = VK_FALSE,
                    .bufferDeviceAddressMultiDevice = VK_FALSE,
                    .vulkanMemoryModel = VK_TRUE,
                    .vulkanMemoryModelDeviceScope = VK_TRUE,
                    .vulkanMemoryModelAvailabilityVisibilityChains = VK_TRUE,
                    .shaderOutputViewportIndex = VK_FALSE,
                    .shaderOutputLayer = VK_FALSE,
                    .subgroupBroadcastDynamicId = VK_FALSE,
                })
                .set_required_features_13({
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                    .pNext = nullptr,
                    .robustImageAccess = VK_FALSE,
                    .inlineUniformBlock = VK_FALSE,
                    .descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE,
                    .pipelineCreationCacheControl = VK_FALSE,
                    .privateData = VK_FALSE,
                    .shaderDemoteToHelperInvocation = VK_TRUE,
                    .shaderTerminateInvocation = VK_FALSE,
                    .subgroupSizeControl = VK_FALSE,
                    .computeFullSubgroups = VK_FALSE,
                    .synchronization2 = VK_TRUE,
                    .textureCompressionASTC_HDR = VK_FALSE,
                    .shaderZeroInitializeWorkgroupMemory = VK_FALSE,
                    .dynamicRendering = VK_TRUE,
                    .shaderIntegerDotProduct = VK_FALSE,
                    .maintenance4 = VK_FALSE,
                })
                .add_required_extension_features(extended_dynamic_state)
                .add_required_extension_features(fragment_shader_interlock);

        auto devices = selector.select_devices();
        if (!devices || devices->empty())
        {
            return nullptr;
        }

        vector<vkb::PhysicalDevice> vkb_devices(devices->begin(), devices->end());

        return make_unique<rhi::vk::instance>(tempest::move(instance), tempest::move(vkb_devices));
    }
} // namespace tempest::rhi::vk
