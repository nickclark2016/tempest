#ifndef TEMPEST_RHI_VK_CONTEXT_HPP
#define TEMPEST_RHI_VK_CONTEXT_HPP

#include <tempest/rhi.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>
#include <tempest/vk/bootstrap.hpp>

namespace tempest
{
    class logger;
}

namespace tempest::rhi::vk
{
    TEMPEST_API auto create_context(const context_desc& desc, logger& log)
        -> expected<unique_ptr<rhi::context>, context_creation_error>;

    class TEMPEST_API context final : public rhi::context
    {
      public:
        static auto create(const context_desc& desc, logger& log)
            -> expected<unique_ptr<rhi::context>, context_creation_error>;

        context(const context&) = delete;
        context(context&&) noexcept = delete;
        ~context() override;

        context& operator=(const context&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        context& operator=(context&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        [[nodiscard]] auto enumerate_devices() -> span<const device_desc> override;
        [[nodiscard]] auto create_device(guid device_uuid) -> unique_ptr<rhi::device> override;

      private:
        context(native_instance instance, vector<device_desc> devices, vector<physical_device_info> physical_devices);

        native_instance _instance;
        vector<device_desc> _devices;
        vector<physical_device_info> _physical_devices;
    };
} // namespace tempest::rhi::vk

#endif // TEMPEST_RHI_VK_CONTEXT_HPP
