#ifndef TEMPEST_RHI_VK_CONTEXT_HPP
#define TEMPEST_RHI_VK_CONTEXT_HPP

#include <tempest/rhi.hpp>
#include <tempest/vector.hpp>

#include <VkBootstrap.h>

namespace tempest::rhi::vk
{
    auto create_context(const context_desc& desc) -> expected<unique_ptr<rhi::context>, context_creation_error>;

    class TEMPEST_API context final : public rhi::context
    {
      public:
        static auto create(const context_desc& desc) -> expected<unique_ptr<rhi::context>, context_creation_error>;

        context(const context&) = delete;
        context(context&&) noexcept = delete;
        ~context() override;

        context& operator=(const context&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        context& operator=(context&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        [[nodiscard]] auto enumerate_devices() -> span<const device_desc> override;
        [[nodiscard]] auto create_device(guid device_uuid) -> unique_ptr<rhi::device> override;

      private:
        context(vkb::Instance instance, vector<device_desc> devices);

        vkb::Instance _instance;
        vector<device_desc> _devices;
    };
} // namespace tempest::rhi::vk

#endif // TEMPEST_RHI_VK_CONTEXT_HPP
