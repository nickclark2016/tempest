#ifndef tempest_graphics_descriptors_hpp
#define tempest_graphics_descriptors_hpp

#include "fwd.hpp"
#include "resources.hpp"

#include <tempest/object_pool.hpp>

namespace tempest::graphics
{
    class descriptor_pool
    {
      public:
        explicit descriptor_pool(gfx_device* device);
        descriptor_pool(const descriptor_pool&) = delete;
        descriptor_pool(descriptor_pool&&) noexcept = delete;
        ~descriptor_pool();

        descriptor_pool& operator=(const descriptor_pool&) = delete;
        descriptor_pool& operator=(descriptor_pool&&) noexcept = delete;

        descriptor_set* access(descriptor_set_handle handle);
        const descriptor_set* access(descriptor_set_handle handle) const;
        descriptor_set_handle create(const descriptor_set_create_info& ci);
        void release(descriptor_set_handle handle);

        inline VkDescriptorSet get_bindless_texture_descriptors() const noexcept
        {
            return _texture_bindless_set;
        }

        std::uint32_t get_bindless_texture_index() const noexcept;
        std::uint32_t get_bindless_storage_image_index() const noexcept;

      private:
        gfx_device* _device;

        core::object_pool _descriptor_set_pool;

        VkDescriptorPool _default_pool{};
        VkDescriptorPool _bindless_pool{};

        VkDescriptorSetLayout _image_bindless_layout{};
        VkDescriptorSet _texture_bindless_set{};
    };

    class descriptor_set_builder
    {
      public:
        explicit descriptor_set_builder(std::string_view name);
        descriptor_set_builder& set_layout(descriptor_set_layout_handle layout);
        descriptor_set_builder& add_image(texture_handle tex, std::uint16_t binding_index);
        descriptor_set_builder& add_texture(texture_handle tex, sampler_handle smp, std::uint16_t binding_index);
        descriptor_set_builder& add_sampler(sampler_handle smp, std::uint16_t binding_index);
        descriptor_set_builder& add_buffer(buffer_handle buf, std::uint16_t binding_index);
        descriptor_set_handle build(descriptor_pool& pool) const;

      private:
        friend class descriptor_pool;

        descriptor_set_create_info _ci{.resource_count{0}};
    };
} // namespace tempest::graphics

#endif // tempest_graphics_descriptors_hpp