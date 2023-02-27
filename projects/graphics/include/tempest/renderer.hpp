#ifndef tempest_renderer_hpp__
#define tempest_renderer_hpp__

#include "instance.hpp"
#include "window.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace tempest::graphics
{
    class irenderer;
    class icommand_buffer;
    class irenderer_graph;

    enum class render_target_type : std::uint64_t
    {
        COLOR_READ = 1ULL << 8,
        COLOR_WRITE = 1ULL << 7,
        COLOR_READ_WRITE = (1ULL << 8) | (1ULL << 7),
        DEPTH_STENCIL_READ = 1ULL << 12,
        DEPTH_STENCIL_WRITE = 1ULL << 13,
        DEPTH_STENCIL_READ_WRITE = (1ULL << 12) | (1ULL << 13),
        INPUT_READ = 1ULL << 14,
        FRAGMENT_SAMPLED = 1ULL << 20,
        FRAGMENT_READ = 1ULL << 21,
        FRAGMENT_WRITE = 1ULL << 22,
        FRAGMENT_READ_WRITE = (1ULL << 21) | (1ULL << 22),
    };

    class icommand_buffer
    {
      public:
        virtual ~icommand_buffer() = default;

        virtual icommand_buffer& use_full_viewport(std::uint32_t vp_index = 0) = 0;
        virtual icommand_buffer& use_full_scissor(std::uint32_t sc_index = 0) = 0;
        virtual icommand_buffer& use_default_raster_state() = 0;
        virtual icommand_buffer& use_default_color_blend(std::string_view render_target_name) = 0;
        virtual icommand_buffer& use_graphics_pipeline(std::string_view pipeline_name) = 0;
        virtual icommand_buffer& draw(std::uint32_t vertex_count, std::uint32_t instance_count,
                                      std::uint32_t first_vertex, std::uint32_t first_instance) = 0;
    };

    struct render_target
    {
        std::string_view name;
        std::string_view output_name{};
        render_target_type type;
    };

    struct render_pass
    {
        std::span<render_target> resources;
        std::function<void(icommand_buffer&)> execute;
    };

    class irenderer_graph
    {
      public:
        static constexpr std::string_view BACK_BUFFER{"tempest_render_graph_target"};

        virtual irenderer_graph& set_final_target(render_target target) = 0;
        virtual irenderer_graph& add_pass(const render_pass& pass) = 0;
        virtual ~irenderer_graph() = default;
    };

    class iresource_allocator
    {
      public:
        struct shader_source
        {
            std::string_view name;
            std::vector<std::uint32_t> data;
        };

        virtual ~iresource_allocator() = default;

        virtual void create_named_pipeline(std::span<shader_source> sources, std::string_view name) = 0;
    };

    class irenderer
    {
      public:
        virtual ~irenderer() = default;

        virtual iresource_allocator& get_allocator() = 0;
        virtual std::unique_ptr<irenderer_graph> create_render_graph() = 0;
        virtual void execute(irenderer_graph& graph) = 0;

        static std::unique_ptr<irenderer> create(iwindow& win);
    };
} // namespace tempest::graphics

#endif // tempest_renderer_hpp__
