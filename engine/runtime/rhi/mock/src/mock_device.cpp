#include <tempest/rhi/mock/mock_device.hpp>

namespace tempest::rhi::mock
{
    auto mock_device::create_buffer(const buffer_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::buffer>
    {
        auto result = typed_rhi_handle<rhi_handle_type::buffer>{
            .id = _next_handle++,
            .generation = 0,
        };

        _history.push_back(create_buffer_cmd{
            .desc = desc,
            .result = result,
        });

        return result;
    }

    auto mock_device::create_image(const image_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::image>
    {
        auto result = typed_rhi_handle<rhi_handle_type::image>{
            .id = _next_handle++,
            .generation = 0,
        };

        _history.push_back(create_image_cmd{
            .desc = desc,
            .result = result,
        });

        return result;
    }

    auto mock_device::create_fence(const fence_info& info) noexcept -> typed_rhi_handle<rhi_handle_type::fence>
    {
        auto result = typed_rhi_handle<rhi_handle_type::fence>{
            .id = _next_handle++,
            .generation = 0,
        };

        _history.push_back(create_fence_cmd{
            .info = info,
            .result = result,
        });

        return result;
    }
} // namespace tempest::rhi::mock