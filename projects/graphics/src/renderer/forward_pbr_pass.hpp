#ifndef tempest_graphics_forward_pbr_pass_hpp
#define tempest_graphics_forward_pbr_pass_hpp

#include "fwd.hpp"
#include "resources.hpp"

namespace tempest::graphics
{
    struct model_payload
    {
        float transformation[16];
        std::uint32_t material_id;

      private:
        std::uint32_t padding0, padding1, padding2;
    };

    struct material_payload
    {
        std::uint32_t albedo_index;
        std::uint32_t normal_map_index;
        std::uint32_t metallic_roughness_index;
        std::uint32_t ao_map_index;
    };

    struct forward_pbr_pass
    {
        static constexpr std::size_t MAX_ENTITIES_PER_FRAME = 32 * 1024;

        render_pass_handle pass;
        descriptor_set_layout_handle buffer_layout_desc;
        pipeline_handle forward_pbr_pipeline;

        texture_handle color_target;
        texture_handle depth_target;

        buffer_handle scene_data_buffer;
        buffer_handle material_data_buffer;
        buffer_handle model_data_buffer;
        buffer_handle draw_parameter_buffer;

        static forward_pbr_pass create(gfx_device* device, texture_handle color, texture_handle depth);
        void release(gfx_device* device);
    };
} // namespace tempest::graphics

#endif // tempest_graphics_forward_pbr_pass_hpp
