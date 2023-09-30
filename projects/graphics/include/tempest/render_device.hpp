#ifndef tempest_graphics_render_device_hpp
#define tempest_graphics_render_device_hpp

#include "types.hpp"

#include <tempest/memory.hpp>

#include <compare>
#include <memory>

namespace tempest::graphics
{
    struct gfx_resource_handle
    {
        std::uint32_t id;
        std::uint32_t generation;

        constexpr gfx_resource_handle(std::uint32_t id, std::uint32_t generation) : id{id}, generation{generation}
        {
        }

        inline constexpr operator bool() const noexcept
        {
            return generation != ~0u;
        }

        constexpr auto operator<=>(const gfx_resource_handle& rhs) const noexcept = default;
    };

    struct image_resource_handle : public gfx_resource_handle
    {
        constexpr image_resource_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct buffer_resource_handle : public gfx_resource_handle
    {
        constexpr buffer_resource_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct graph_pass_handle : public gfx_resource_handle
    {
        constexpr graph_pass_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    class render_device;

    class render_context
    {
      public:
        virtual ~render_context() = default;

        virtual bool has_suitable_device() const noexcept = 0;
        virtual std::uint32_t device_count() const noexcept = 0;
        virtual render_device& get_device(std::uint32_t idx = 0) = 0;

        static std::unique_ptr<render_context> create(core::allocator* alloc);

      protected:
        core::allocator* _alloc;

        explicit render_context(core::allocator* alloc);
    };

    class render_device
    {
      public:
        virtual void start_frame() noexcept = 0;
        virtual void end_frame() noexcept = 0;

        virtual buffer_resource_handle create_buffer(const buffer_create_info& ci) = 0;
        virtual image_resource_handle create_image(const image_create_info& ci) = 0;

        virtual ~render_device() = default;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_device_hpp