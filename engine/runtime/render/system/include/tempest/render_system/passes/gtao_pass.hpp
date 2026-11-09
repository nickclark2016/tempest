#ifndef tempest_render_system_gtao_pass_hpp
#define tempest_render_system_gtao_pass_hpp

#include <tempest/api.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/rhi.hpp>

namespace tempest::render_system
{
    struct gtao_pass_data
    {
        render_graph::rg_texture_id depth_texture;
        render_graph::rg_texture_id gtao_raw;
        render_graph::rg_buffer_id scene_constants;
    };

    // NOLINTNEXTLINE -- Align the struct to 16 bytes for GPU compatibility
    struct alignas(16) gtao_push_constants
    {
        uint64_t scene_constants_address;
        uint32_t depth_texture_id;
        uint32_t point_sampler_index;

        uint32_t slice_count;
        uint32_t steps_per_slice;

        float radius;
        float thickness;
        float power;

        uint32_t _pad0;
        uint32_t _pad1;
        uint32_t _pad2;
    };

    // NOLINTNEXTLINE -- Ensure the size of the push constants struct is as expected for GPU compatibility
    static_assert(sizeof(gtao_push_constants) == 48, "gtao_push_constants must be 48 bytes in size");

    TEMPEST_API auto add_gtao_pass(render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
                                   render_graph::rg_texture_id depth_tex, uint32_t slice_count,
                                   uint32_t steps_per_slice, float radius, float thickness, float power,
                                   enum_mask<rhi::pipeline_statistic_flags> pipeline_stats =
                                       rhi::pipeline_statistic_flags::none) -> const gtao_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_gtao_pass_hpp
