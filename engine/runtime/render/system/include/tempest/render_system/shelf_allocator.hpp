#ifndef tempest_render_system_shelf_allocator_hpp
#define tempest_render_system_shelf_allocator_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_system
{
    enum class allocation_error
    {
        out_of_space,
        allocation_too_large,
        zero_size,
    };

    struct viewport_rect
    {
        uint32_t x{0};
        uint32_t y{0};
        uint32_t width{0};
        uint32_t height{0};
    };

    class TEMPEST_API shelf_allocator
    {
      public:
        shelf_allocator() = default;
        shelf_allocator(uint32_t atlas_width, uint32_t atlas_height, uint32_t padding = 4) noexcept;

        void reset() noexcept;
        void reset(uint32_t atlas_width, uint32_t atlas_height, uint32_t padding = 4) noexcept;

        /// @brief Allocates a rectangular viewport with the configured border padding.
        /// @return The viewport rectangle inside the atlas, or an allocation_error if it could not be placed.
        [[nodiscard]] auto allocate(uint32_t width, uint32_t height) -> expected<viewport_rect, allocation_error>;

        [[nodiscard]] auto get_atlas_width() const noexcept -> uint32_t
        {
            return _atlas_width;
        }

        [[nodiscard]] auto get_atlas_height() const noexcept -> uint32_t
        {
            return _atlas_height;
        }

        [[nodiscard]] auto get_padding() const noexcept -> uint32_t
        {
            return _padding;
        }

      private:
        struct shelf
        {
            uint32_t y{0};
            uint32_t height{0};
            uint32_t current_x{0};
        };

        uint32_t _atlas_width{0};
        uint32_t _atlas_height{0};
        uint32_t _padding{4};
        uint32_t _current_y{0};
        vector<shelf> _shelves;
    };
} // namespace tempest::render_system

#endif // tempest_render_system_shelf_allocator_hpp
