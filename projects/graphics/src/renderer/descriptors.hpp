#ifndef tempest_graphics_descriptors_hpp__
#define tempest_graphics_descriptors_hpp__

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

        descriptor_set_handle create(const descriptor_set_create_info& ci);
        void release(descriptor_set_handle handle);

      private:
        gfx_device* _device;

        core::object_pool _descriptor_set_pool;

        VkDescriptorPool _default_pool;
        VkDescriptorPool _bindless_pool;
    };
}

#endif // tempest_graphics_descriptors_hpp__