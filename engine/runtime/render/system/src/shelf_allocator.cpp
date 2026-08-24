#include <tempest/render_system/shelf_allocator.hpp>

namespace tempest::render_system
{
    shelf_allocator::shelf_allocator(uint32_t atlas_width, uint32_t atlas_height, uint32_t padding) noexcept
        : _atlas_width{atlas_width}, _atlas_height{atlas_height}, _padding{padding}
    {
    }

    void shelf_allocator::reset() noexcept
    {
        _current_y = 0;
        _shelves.clear();
    }

    void shelf_allocator::reset(uint32_t atlas_width, uint32_t atlas_height, uint32_t padding) noexcept
    {
        _atlas_width = atlas_width;
        _atlas_height = atlas_height;
        _padding = padding;
        _current_y = 0;
        _shelves.clear();
    }

    auto shelf_allocator::allocate(uint32_t width, uint32_t height) -> expected<viewport_rect, allocation_error>
    {
        if (width == 0 || height == 0)
        {
            return unexpected(allocation_error::zero_size);
        }

        // Guard against underflow & overflow:
        // 1. Total border padding along an axis is 2 * _padding. Check if padding alone exceeds atlas dimensions.
        // 2. Rather than checking (width + total_padding > _atlas_width), which can overflow uint32_t when width is near UINT32_MAX,
        //    we compare (width > _atlas_width - total_padding).
        // Since we verify _atlas_width >= total_padding first, the subtraction is guaranteed never to underflow,
        // and subsequent addition `width + total_padding` is guaranteed <= _atlas_width and will never overflow uint32_t.
        const auto total_pad_x = _padding * 2;
        const auto total_pad_y = _padding * 2;

        if (_atlas_width < total_pad_x || _atlas_height < total_pad_y ||
            width > _atlas_width - total_pad_x || height > _atlas_height - total_pad_y)
        {
            return unexpected(allocation_error::allocation_too_large);
        }

        auto padded_w = width + total_pad_x;
        auto padded_h = height + total_pad_y;

        // First-fit search across existing shelves
        for (auto& s : _shelves)
        {
            // Use subtraction (_atlas_width - s.current_x >= padded_w) to prevent overflow if atlas_width is large.
            if (padded_w <= _atlas_width - s.current_x && padded_h <= s.height)
            {
                auto slot_x = s.current_x;
                auto slot_y = s.y;
                s.current_x += padded_w;

                return viewport_rect{
                    .x = slot_x + _padding,
                    .y = slot_y + _padding,
                    .width = width,
                    .height = height,
                };
            }
        }

        // Create new shelf if vertical space allows (subtraction prevents potential overflow)
        if (padded_h <= _atlas_height - _current_y)
        {
            auto slot_x = 0U;
            auto slot_y = _current_y;
            _current_y += padded_h;

            _shelves.push_back(shelf{
                .y = slot_y,
                .height = padded_h,
                .current_x = padded_w,
            });

            return viewport_rect{
                .x = slot_x + _padding,
                .y = slot_y + _padding,
                .width = width,
                .height = height,
            };
        }

        return unexpected(allocation_error::out_of_space);
    }
} // namespace tempest::render_system
