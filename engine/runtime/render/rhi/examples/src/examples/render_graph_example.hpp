#ifndef tempest_rhi_examples_render_graph_example_hpp
#define tempest_rhi_examples_render_graph_example_hpp

#include "../example.hpp"
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/rhi.hpp>

namespace tempest::rhi::examples
{
    class render_graph_example final : public example
    {
      public:
        [[nodiscard]] static auto create() -> unique_ptr<example>
        {
            return make_unique<render_graph_example>();
        }

        [[nodiscard]] auto init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool override;
        auto render(const frame_render_info& info) -> void override;
        auto on_resize(rhi::device& dev, rhi::render_surface_format surface_format, uint32_t width, uint32_t height)
            -> void override;
        auto shutdown(rhi::device& dev) -> void override;

      private:
        auto create_pipeline(rhi::device& dev, rhi::render_surface_format surface_format) -> bool;
        auto create_compute_pipeline(rhi::device& dev) -> bool;

        rhi::buffer_handle _positions_buffer{};
        rhi::buffer_handle _colors_buffer{};
        rhi::buffer_handle _accent_positions_buffer{};
        rhi::buffer_handle _accent_colors_buffer{};
        rhi::buffer_handle _emblem_positions_buffer{};
        rhi::buffer_handle _emblem_colors_buffer{};
        rhi::buffer_handle _index_buffer{};
        rhi::graphics_pipeline_handle _pipeline{};
        rhi::render_surface_format _current_format{rhi::render_surface_format::unknown};
        unique_ptr<render_graph::render_graph> _render_graph;
        rhi::device* _device{nullptr};
    };
} // namespace tempest::rhi::examples

#endif // tempest_rhi_examples_render_graph_example_hpp
