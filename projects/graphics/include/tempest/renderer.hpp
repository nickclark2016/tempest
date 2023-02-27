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

    struct render_target
    {
        std::string_view name;
        std::string_view output_name{};
        render_target_type type;
    };

    struct render_pass
    {
        std::span<render_target> resources;
    };

    class irenderer_graph
    {
      public:
        virtual irenderer_graph& set_final_target(render_target target) = 0;
        virtual irenderer_graph& add_pass(const render_pass& pass) = 0;
        virtual ~irenderer_graph() = default;
    };

    class irenderer
    {
      public:
        virtual ~irenderer() = default;

        virtual std::unique_ptr<irenderer_graph> create_render_graph() = 0;
        virtual void execute(irenderer_graph& graph) = 0;

        static std::unique_ptr<irenderer> create(iwindow& win);
    };
} // namespace tempest::graphics

#endif // tempest_renderer_hpp__
