#ifndef tempest_render_system_shadow_pass_hpp
#define tempest_render_system_shadow_pass_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/render_system/shelf_allocator.hpp>

namespace tempest::render_system
{
    struct TEMPEST_API shadow_pass_data
    {
        render_graph::rg_texture_id shadow_atlas{};
    };

    struct TEMPEST_API shadow_pass_result
    {
        directional_shadow_data shadow_data{};
        render_graph::rg_texture_id shadow_atlas{};
    };

    struct TEMPEST_API shadow_pass_params
    {
        render_graph::render_graph& graph;
        resource_pool& pool;
        shader_manager& shaders;
        render_graph::rg_texture_id shadow_atlas;
        shelf_allocator& allocator;
        const ecs::archetype_registry& registry;
        const camera_system* camera_sys{nullptr};
        optional<render_camera> camera_override{nullopt};
        uint32_t opaque_draw_count{0};
        uint32_t opaque_draw_offset{0};
        uint32_t alpha_masked_draw_count{0};
        uint32_t alpha_masked_draw_offset{0};
        enum_mask<rhi::pipeline_statistic_flags> pipeline_statistics{rhi::pipeline_statistic_flags::none};
    };

    auto TEMPEST_API add_shadow_pass(shadow_pass_params params) -> shadow_pass_result;
} // namespace tempest::render_system

#endif // tempest_render_system_shadow_pass_hpp
