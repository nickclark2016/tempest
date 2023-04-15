#include "descriptors.hpp"

#include "device.hpp"
#include "resources.hpp"

#include <tempest/logger.hpp>

#include <vulkan/vulkan.h>

namespace tempest::graphics
{
    namespace
    {
        auto logger = tempest::logger::logger_factory::create({.prefix{"tempest::graphics::descriptor_pool"}});

        constexpr std::uint32_t max_global_pool_elements = 256;
        const std::size_t max_bindless_resource_count = 1024;

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
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_bindless_resource_count},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_bindless_resource_count},
        };

        constexpr auto default_pool_size_count =
            static_cast<std::uint32_t>(std::extent_v<decltype(default_pool_sizes)>);
        constexpr auto bindless_pool_size_count =
            static_cast<std::uint32_t>(std::extent_v<decltype(bindless_pool_sizes)>);

        constexpr std::uint32_t bindless_image_index{0};
        constexpr std::uint32_t storage_image_index{bindless_image_index + 1};
        constexpr std::uint32_t bindless_set{1};
    } // namespace

    descriptor_pool::descriptor_pool(gfx_device* device)
        : _device{device}, _descriptor_set_pool{_device->_global_allocator, 128, sizeof(descriptor_set)}
    {
        logger->debug("Creating descriptor_pool.");

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
            .maxSets{bindless_pool_size_count * max_bindless_resource_count},
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
            return;
        }

        logger->debug("Creating global bindless descriptors.");
        {
            std::array<VkDescriptorSetLayoutBinding, bindless_pool_size_count> bindings;

            bindings[0] = {
                .binding{bindless_image_index},
                .descriptorType{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                .descriptorCount{max_bindless_resource_count},
                .stageFlags{VK_SHADER_STAGE_ALL},
                .pImmutableSamplers{nullptr},
            };

            bindings[1] = {
                .binding{storage_image_index},
                .descriptorType{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                .descriptorCount{max_bindless_resource_count},
                .stageFlags{VK_SHADER_STAGE_ALL},
                .pImmutableSamplers{nullptr},
            };

            constexpr VkDescriptorBindingFlags binding_flag =
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT /* |
                                                            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT */
                ;

            constexpr std::array<VkDescriptorBindingFlags, bindless_pool_size_count> binding_flags = {binding_flag,
                                                                                                      binding_flag};

            VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_ci = {
                .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO},
                .pNext{nullptr},
                .bindingCount{2},
                .pBindingFlags{binding_flags.data()}};

            VkDescriptorSetLayoutCreateInfo layout_ci = {
                .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
                .pNext{&binding_flags_ci},
                .flags{VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT},
                .bindingCount{2},
                .pBindings{bindings.data()},
            };

            auto bindless_set_layout_result = _device->_dispatch.createDescriptorSetLayout(
                &layout_ci, _device->_alloc_callbacks, &_image_bindless_layout);

            if (bindless_set_layout_result != VK_SUCCESS)
            {
                logger->error("Failed to create VkDescriptorSetLayout for bindless descriptors.");
            }

            // allocate bindless set
            constexpr std::uint32_t max_binding_count = max_bindless_resource_count - 1;

            VkDescriptorSetVariableDescriptorCountAllocateInfo variable_alloc = {
                .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO},
                .pNext{nullptr},
                .descriptorSetCount{1},
                .pDescriptorCounts{&max_binding_count},
            };

            VkDescriptorSetAllocateInfo alloc_info = {
                .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO},
                .pNext{&variable_alloc},
                .descriptorPool{_bindless_pool},
                .descriptorSetCount{1},
                .pSetLayouts{&_image_bindless_layout},
            };

            auto alloc_result = _device->_dispatch.allocateDescriptorSets(&alloc_info, &_texture_bindless_set);
            if (alloc_result != VK_SUCCESS)
            {
                logger->error("Failed to allocate bindless descriptor set.");
            }

            std::size_t alloc_size = (sizeof(VkDescriptorSetLayoutBinding) + sizeof(descriptor_binding)) * 2;
            std::byte* memory = reinterpret_cast<std::byte*>(_device->_global_allocator->allocate(alloc_size, 1));

            descriptor_set_layout_handle layout_handle{.index{_device->_descriptor_set_layout_pool.acquire_resource()}};
            descriptor_set_layout* layout_ptr = _device->access_descriptor_set_layout(layout_handle);
            layout_ptr->set_index = bindless_set;
            layout_ptr->layout = _image_bindless_layout;
            layout_ptr->num_bindings = 2;
            layout_ptr->handle = layout_handle;
            layout_ptr->bindings = reinterpret_cast<descriptor_binding*>(memory);
            layout_ptr->vk_binding = reinterpret_cast<VkDescriptorSetLayoutBinding*>(
                memory + sizeof(descriptor_binding) * layout_ptr->num_bindings);

            layout_ptr->bindings[0] = {
                .type{bindings[0].descriptorType},
                .start{static_cast<std::uint16_t>(bindings[0].binding)},
                .count{static_cast<std::uint16_t>(bindings[0].descriptorCount)},
                .set{layout_ptr->set_index},
                .name{"BindlessTexture_Binding"},
            };

            layout_ptr->bindings[1] = {
                .type{bindings[1].descriptorType},
                .start{static_cast<std::uint16_t>(bindings[1].binding)},
                .count{static_cast<std::uint16_t>(bindings[1].descriptorCount)},
                .set{layout_ptr->set_index},
                .name{"BindlessStorageImage_Binding"},
            };

            layout_ptr->vk_binding[0] = bindings[0];
            layout_ptr->vk_binding[1] = bindings[1];

            _image_bindless_layout_handle = layout_handle;
        }

        logger->debug("Successfully created descriptor_pool.");
    }

    descriptor_pool::~descriptor_pool()
    {
        _device->release_descriptor_set_layout(_image_bindless_layout_handle);
        _device->_dispatch.destroyDescriptorPool(_default_pool, _device->_alloc_callbacks);
        _device->_dispatch.destroyDescriptorPool(_bindless_pool, _device->_alloc_callbacks);
    }

    descriptor_set* descriptor_pool::access(descriptor_set_handle handle)
    {
        return reinterpret_cast<descriptor_set*>(_descriptor_set_pool.access(handle.index));
    }

    const descriptor_set* descriptor_pool::access(descriptor_set_handle handle) const
    {
        return reinterpret_cast<const descriptor_set*>(_descriptor_set_pool.access(handle.index));
    }

    descriptor_set_handle descriptor_pool::create(const descriptor_set_create_info& ci)
    {
        descriptor_set_handle handle{.index{_descriptor_set_pool.acquire_resource()}};
        if (handle.index == invalid_resource_handle)
        {
            return handle;
        }

        descriptor_set* set = access(handle);
        const descriptor_set_layout* layout = _device->access_descriptor_set_layout(ci.layout);

        VkDescriptorSetAllocateInfo alloc_info = {
            .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO},
            .pNext{nullptr},
            .descriptorPool{_default_pool},
            .descriptorSetCount{1},
            .pSetLayouts{&layout->layout},
        };

        auto result = _device->_dispatch.allocateDescriptorSets(&alloc_info, &set->set);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create VkDescriptorSet {0}", ci.name);
            release(handle);
            handle.index = invalid_resource_handle;
            return handle;
        }

        _device->_set_resource_name(VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<std::uint64_t>(set->set), ci.name);

        std::size_t alloc_size = ci.resource_count *
                                 (sizeof(resource_handle) + sizeof(sampler_handle) + sizeof(std::uint16_t)) *
                                 ci.resource_count;
        std::byte* mem = reinterpret_cast<std::byte*>(_device->_global_allocator->allocate(alloc_size, 1));
        set->resources = reinterpret_cast<resource_handle*>(mem);
        set->samplers = reinterpret_cast<sampler_handle*>(mem + sizeof(resource_handle) * ci.resource_count);
        set->bindings = reinterpret_cast<std::uint16_t*>(mem + (sizeof(resource_handle) + sizeof(sampler_handle)) *
                                                                   ci.resource_count);
        set->num_resources = ci.resource_count;
        set->layout = layout;

        VkWriteDescriptorSet desc_write[max_descriptors_per_set];
        VkDescriptorBufferInfo buffer_info[max_descriptors_per_set];
        VkDescriptorImageInfo image_info[max_descriptors_per_set];

        std::uint32_t num_resources = ci.resource_count;
        _device->_fill_write_descriptor_sets(layout, set->set, desc_write, buffer_info, image_info, num_resources,
                                             std::span<const resource_handle>(ci.resources),
                                             std::span<const sampler_handle>(ci.samplers),
                                             std::span<const std::uint16_t>(ci.bindings));

        std::copy_n(ci.resources.begin(), ci.resource_count, set->resources);
        std::copy_n(ci.samplers.begin(), ci.resource_count, set->samplers);
        std::copy_n(ci.bindings.begin(), ci.resource_count, set->bindings);

        _device->_dispatch.updateDescriptorSets(num_resources, desc_write, 0, nullptr);

        set->pool = alloc_info.descriptorPool;

        return handle;
    }

    void descriptor_pool::release(descriptor_set_handle handle)
    {
        auto set = access(handle);
        // TODO: make sure this isn't in use
        _device->_dispatch.freeDescriptorSets(_bindless_pool, 1, &_texture_bindless_set);
        _device->_dispatch.destroyDescriptorSetLayout(_image_bindless_layout, _device->_alloc_callbacks);
        _device->_dispatch.freeDescriptorSets(set->pool, 1, &set->set);
        _descriptor_set_pool.release_resource(handle.index);
        _device->_global_allocator->deallocate(set->resources);
    }

    std::uint32_t descriptor_pool::get_bindless_texture_index() const noexcept
    {
        return bindless_image_index;
    }

    std::uint32_t descriptor_pool::get_bindless_storage_image_index() const noexcept
    {
        return storage_image_index;
    }

    descriptor_set_builder::descriptor_set_builder(std::string_view name)
    {
        _ci.name = name;
    }

    descriptor_set_builder& descriptor_set_builder::set_layout(descriptor_set_layout_handle layout)
    {
        _ci.layout = layout;
        return *this;
    }

    descriptor_set_builder& descriptor_set_builder::add_image(texture_handle tex, std::uint16_t binding_index)
    {
        _ci.samplers[_ci.resource_count] = {invalid_resource_handle};
        _ci.bindings[_ci.resource_count] = binding_index;
        _ci.resources[_ci.resource_count] = tex.index;

        _ci.resource_count++;

        return *this;
    }

    descriptor_set_builder& descriptor_set_builder::add_texture(texture_handle tex, sampler_handle smp,
                                                                std::uint16_t binding_index)
    {
        _ci.samplers[_ci.resource_count] = smp;
        _ci.bindings[_ci.resource_count] = binding_index;
        _ci.resources[_ci.resource_count] = tex.index;

        _ci.resource_count++;

        return *this;
    }

    descriptor_set_builder& descriptor_set_builder::add_sampler(sampler_handle smp, std::uint16_t binding_index)
    {
        _ci.samplers[_ci.resource_count] = smp;
        _ci.bindings[_ci.resource_count] = binding_index;
        _ci.resources[_ci.resource_count] = {invalid_resource_handle};

        _ci.resource_count++;

        return *this;
    }

    descriptor_set_builder& descriptor_set_builder::add_buffer(buffer_handle buf, std::uint16_t binding_index)
    {
        _ci.samplers[_ci.resource_count] = {invalid_resource_handle};
        _ci.bindings[_ci.resource_count] = binding_index;
        _ci.resources[_ci.resource_count] = buf.index;

        _ci.resource_count++;

        return *this;
    }

    descriptor_set_handle descriptor_set_builder::build(descriptor_pool& pool) const
    {
        return pool.create(_ci);
    }
} // namespace tempest::graphics