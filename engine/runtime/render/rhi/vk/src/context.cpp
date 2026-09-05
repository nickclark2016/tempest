#ifdef TEMPEST_PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(TEMPEST_PLATFORM_LINUX)
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <tempest/vk/context.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/logger.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vk/bootstrap.hpp>
#include <tempest/vk/device.hpp>

#include <cstring>
#include <vulkan/vulkan_core.h>

namespace tempest::rhi::vk
{
    namespace
    {
        constexpr uint32_t VENDOR_ID_AMD = 0x1002;
        constexpr uint32_t VENDOR_ID_APPLE = 0x106B;
        constexpr uint32_t VENDOR_ID_ARM = 0x13B5;
        constexpr uint32_t VENDOR_ID_IMGTEC = 0x1010;
        constexpr uint32_t VENDOR_ID_INTEL = 0x8086;
        constexpr uint32_t VENDOR_ID_KHRONOS_MIN = 0x10000;
        constexpr uint32_t VENDOR_ID_KHRONOS_MAX = 0x10006;
        constexpr uint32_t VENDOR_ID_NVIDIA = 0x10DE;
        constexpr uint32_t VENDOR_ID_QUALCOMM = 0x5143;

        auto get_device_vendor_from_vk_vendor_id(uint32_t vk_vendor_id) -> rhi::device_vendor
        {
            switch (vk_vendor_id)
            {
            case VENDOR_ID_AMD:
                return rhi::device_vendor::amd;
            case VENDOR_ID_APPLE:
                return rhi::device_vendor::apple;
            case VENDOR_ID_ARM:
                return rhi::device_vendor::arm;
            case VENDOR_ID_IMGTEC:
                return rhi::device_vendor::imgtec;
            case VENDOR_ID_INTEL:
                return rhi::device_vendor::intel;
            case VENDOR_ID_NVIDIA:
                return rhi::device_vendor::nvidia;
            case VENDOR_ID_QUALCOMM:
                return rhi::device_vendor::qualcomm;
            default: {
                if (vk_vendor_id >= VENDOR_ID_KHRONOS_MIN && vk_vendor_id <= VENDOR_ID_KHRONOS_MAX)
                {
                    return rhi::device_vendor::khronos;
                }
                return rhi::device_vendor::unknown;
            }
            }
        }

        auto get_device_type_from_vk_device_type(VkPhysicalDeviceType type) -> rhi::device_type
        {
            switch (type)
            {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                return rhi::device_type::discrete_gpu;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                return rhi::device_type::integrated_gpu;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                return rhi::device_type::virtual_gpu;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                return rhi::device_type::cpu;
            default:
                return rhi::device_type::unknown;
            }
        }

        auto fetch_device_desc(const instance_dispatch_table& dispatch_table,
                               const physical_device_info& physical_device) -> device_desc
        {
            auto dev_desc = device_desc{};

            const auto& vk_physical_device = physical_device.physical_device;

            // Get the device UUID from the physical device properties
            auto physical_device_id_props = VkPhysicalDeviceIDProperties{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
                .pNext = nullptr,
            };
            auto physical_device_props2 = VkPhysicalDeviceProperties2{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &physical_device_id_props,
            };

            dispatch_table.getPhysicalDeviceProperties2(vk_physical_device, &physical_device_props2);

            static_assert(sizeof(dev_desc.device_uuid) == sizeof(physical_device_id_props.deviceUUID),
                          "Device UUID size mismatch");
            std::memcpy(dev_desc.device_uuid.data.data(), physical_device_id_props.deviceUUID,
                        sizeof(dev_desc.device_uuid));

            // Check extensions
            dev_desc.features.ray_query = physical_device.has_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
            dev_desc.features.ray_tracing =
                physical_device.has_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
                physical_device.has_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            dev_desc.features.mesh_shading = physical_device.has_extension(VK_NV_MESH_SHADER_EXTENSION_NAME);

            // Get device name, vendor, limits, and type
            const auto& physical_device_props = physical_device.properties;
            dev_desc.name = physical_device_props.deviceName;
            dev_desc.vendor = get_device_vendor_from_vk_vendor_id(physical_device_props.vendorID);
            dev_desc.type = get_device_type_from_vk_device_type(physical_device_props.deviceType);
            dev_desc.limits = device_limits{
                .max_image_dimension_1d = physical_device_props.limits.maxImageDimension1D,
                .max_image_dimension_2d = physical_device_props.limits.maxImageDimension2D,
                .max_image_dimension_3d = physical_device_props.limits.maxImageDimension3D,
                .max_image_dimension_cube = physical_device_props.limits.maxImageDimensionCube,
                .max_image_array_layers = physical_device_props.limits.maxImageArrayLayers,
                .max_uniform_buffer_range = physical_device_props.limits.maxUniformBufferRange,
                .max_storage_buffer_range = physical_device_props.limits.maxStorageBufferRange,
            };
            return dev_desc;
        }
    } // namespace

    auto create_context(const context_desc& desc, logger& log)
        -> expected<unique_ptr<rhi::context>, context_creation_error>
    {
        return context::create(desc, log);
    }

    auto context::create(const context_desc& desc, logger& log)
        -> expected<unique_ptr<rhi::context>, context_creation_error>
    {
        auto inst_res = create_instance(desc, log);
        if (!inst_res.has_value())
        {
            return unexpected{.value = inst_res.error()};
        }

        auto native_inst = tempest::move(inst_res).value();

        auto phys_devices_res = enumerate_physical_devices(native_inst);
        if (!phys_devices_res.has_value())
        {
            destroy_instance(native_inst);
            return unexpected{.value = phys_devices_res.error()};
        }

        auto phys_devices = tempest::move(phys_devices_res).value();
        auto devices = vector<device_desc>{};
        devices.reserve(phys_devices.size());

        for (const auto& phys_dev : phys_devices)
        {
            devices.push_back(fetch_device_desc(native_inst.dispatch, phys_dev));
        }

        return unique_ptr<rhi::context>{
            new context{tempest::move(native_inst), tempest::move(devices), tempest::move(phys_devices)}};
    }

    context::~context()
    {
        destroy_instance(_instance);
    }

    auto context::enumerate_devices() -> span<const device_desc>
    {
        return _devices;
    }

    auto context::create_device(guid device_uuid) -> unique_ptr<rhi::device>
    {
        // Find the physical device with the matching UUID
        auto* const physical_device_iter = tempest::find_if(
            _physical_devices.begin(), _physical_devices.end(),
            [&device_uuid, this](const physical_device_info& phys_dev) -> auto {
                auto physical_device_id_props = VkPhysicalDeviceIDProperties{
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
                    .pNext = nullptr,
                };
                auto physical_device_props2 = VkPhysicalDeviceProperties2{
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                    .pNext = &physical_device_id_props,
                };

                _instance.dispatch.getPhysicalDeviceProperties2(phys_dev.physical_device, &physical_device_props2);

                return std::memcmp(physical_device_id_props.deviceUUID, device_uuid.data.data(), sizeof(device_uuid)) ==
                       0;
            });

        if (physical_device_iter == _physical_devices.end())
        {
            return nullptr;
        }

        const auto& phys_dev = *physical_device_iter;
        auto dev_res = vk::create_device(_instance, phys_dev);
        if (!dev_res.has_value())
        {
            return nullptr;
        }

        auto native_dev = tempest::move(dev_res).value();
        auto desc = fetch_device_desc(_instance.dispatch, phys_dev);

        return device::create(_instance, phys_dev, tempest::move(native_dev), tempest::move(desc));
    }

    context::context(native_instance instance, vector<device_desc> devices,
                     vector<physical_device_info> physical_devices)
        : _instance{tempest::move(instance)}, _devices{tempest::move(devices)},
          _physical_devices{tempest::move(physical_devices)}
    {
    }
} // namespace tempest::rhi::vk
