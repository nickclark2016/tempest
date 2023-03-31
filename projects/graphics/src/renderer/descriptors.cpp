#include "descriptors.hpp"

#include "device.hpp"
#include "resources.hpp"

#include <tempest/logger.hpp>

#include <vulkan/vulkan.h>

namespace tempest::graphics
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::descriptor_pool"}});
    }

    descriptor_pool::descriptor_pool(gfx_device* device)
        : _device{device}, _descriptor_set_pool{_device->_global_allocator, 128, sizeof(descriptor_set)}
    {
        constexpr std::uint32_t max_global_pool_elements = 256;

        constexpr VkDescriptorPoolSize default_pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, max_global_pool_elements},
        };

        constexpr VkDescriptorPoolSize bindless_pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_global_pool_elements},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_global_pool_elements},
        };

        constexpr auto default_pool_size_count =
            static_cast<std::uint32_t>(std::extent_v<decltype(default_pool_sizes)>);
        constexpr auto bindless_pool_size_count =
            static_cast<std::uint32_t>(std::extent_v<decltype(bindless_pool_sizes)>);

        VkDescriptorPoolCreateInfo default_ci = {
            .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO},
            .pNext{nullptr},
            .flags{VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT},
            .maxSets{default_pool_size_count * max_global_pool_elements},
            .poolSizeCount{default_pool_size_count},
            .pPoolSizes{default_pool_sizes},
        };

        VkDescriptorPoolCreateInfo bindless_ci = {
            .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO},
            .pNext{nullptr},
            .flags{VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT},
            .maxSets{bindless_pool_size_count * max_global_pool_elements},
            .poolSizeCount{bindless_pool_size_count},
            .pPoolSizes{bindless_pool_sizes},
        };

        const auto default_result =
            _device->_dispatch.createDescriptorPool(&default_ci, _device->_alloc_callbacks, &_default_pool);
        const auto bindless_result =
            _device->_dispatch.createDescriptorPool(&bindless_ci, _device->_alloc_callbacks, &_bindless_pool);

        if (default_result != VK_SUCCESS || bindless_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkDescriptorPools.");
        }
    }

    descriptor_pool::~descriptor_pool()
    {
        _device->_dispatch.destroyDescriptorPool(_default_pool, _device->_alloc_callbacks);
        _device->_dispatch.destroyDescriptorPool(_bindless_pool, _device->_alloc_callbacks);
    }

    descriptor_set_handle descriptor_pool::create(const descriptor_set_create_info& ci)
    {
        return descriptor_set_handle();
    }

    void descriptor_pool::release(descriptor_set_handle handle)
    {
    }
} // namespace tempest::graphics