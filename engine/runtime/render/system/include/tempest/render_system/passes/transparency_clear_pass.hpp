#ifndef tempest_render_system_transparency_clear_pass_hpp
#define tempest_render_system_transparency_clear_pass_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct transparency_clear_pass_data
    {
        render_graph::rg_texture_id moments_texture;
        render_graph::rg_texture_id zeroth_moment_texture;
        uint32_t width{0};
        uint32_t height{0};
    };

    struct transparency_clear_push_constants
    {
        int32_t moments_storage_index{-1};
        int32_t zeroth_moment_storage_index{-1};
        uint32_t width{0};
        uint32_t height{0};
    };

    TEMPEST_API auto add_transparency_clear_pass(
        render_graph::render_graph& graph,
        shader_manager& shaders,
        render_graph::rg_texture_id moments_tex,
        render_graph::rg_texture_id zeroth_moment_tex,
        uint32_t width,
        uint32_t height)
        -> const transparency_clear_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_transparency_clear_pass_hpp
