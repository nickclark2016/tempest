#ifndef tempest_vk_instance_hpp__
#define tempest_vk_instance_hpp__

#include <tempest/instance.hpp>

#include <VkBootstrap.h>

namespace tempest::graphics::vk
{
    class device final : public idevice
    {
      public:
        ~device() override = default;
    };

    class instance final : public iinstance
    {
      public:
        instance(const instance_factory::create_info& info);
        instance(const instance&) = delete;
        instance(instance&& other) noexcept;

        ~instance() override;

        instance& operator=(const instance&) = delete;
        instance& operator=(instance&& rhs) noexcept;

      private:
        vkb::Instance _inst;

        void _release();
    };
}

#endif // tempest_vk_instance_hpp__
