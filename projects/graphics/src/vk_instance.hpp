#ifndef tempest_vk_instance_hpp__
#define tempest_vk_instance_hpp__

#include <tempest/instance.hpp>

#include <VkBootstrap.h>

#include <functional>

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

        inline vkb::Device raw() const noexcept
        {
            return _dev;
        }

        void release() override
        {
            _dev = {};
        }

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

        inline const vkb::Instance& raw() const noexcept
        {
            return _inst;
        }

        inline void release() override
        {
            for (auto& dev : _devices)
            {
                dev->release();
            }
            _devices.clear();
            _inst = {};
        }

      private:
        vkb::Instance _inst;
        std::vector<std::unique_ptr<idevice>> _devices;

        void _release();
    };
} // namespace tempest::graphics::vk

#endif // tempest_vk_instance_hpp__
