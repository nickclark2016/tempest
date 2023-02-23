#ifndef tempest_vk_instance_hpp__
#define tempest_vk_instance_hpp__

#include <tempest/instance.hpp>

#include <VkBootstrap.h>

namespace tempest::graphics::vk
{
    class device final : public idevice
    {
      public:
        device(vkb::Device&& dev);

        device(const device&) = delete;
        device(device&& other) noexcept;
        
        ~device() override;

        device& operator=(const device&) = delete;
        device& operator=(device&& rhs) noexcept;

      private:
        vkb::Device _dev;
        vkb::DispatchTable _dispatch;

        void _release();
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

        std::span<const std::unique_ptr<idevice>> get_devices() const noexcept override;

      private:
        vkb::Instance _inst;
        std::vector<std::unique_ptr<idevice>> _devices;

        void _release();
    };
} // namespace tempest::graphics::vk

#endif // tempest_vk_instance_hpp__
