#include <tempest/rhi.hpp>

namespace tempest::rhi
{
#if defined(TEMPEST_RHI_VULKAN)
    namespace vk
    {
        extern auto create_context(const context_desc& desc) -> expected<unique_ptr<context>, context_creation_error>;
    }
#endif

    auto create_context(const context_desc& desc) -> expected<unique_ptr<context>, context_creation_error>
    {
        switch (desc.api)
        {
#if defined(TEMPEST_RHI_VULKAN)
        case graphics_api::vulkan:
            return vk::create_context(desc);
#endif
        default:
            return unexpected{ .value = context_creation_error::unsupported_api };
        }
    }
} // namespace tempest::rhi
