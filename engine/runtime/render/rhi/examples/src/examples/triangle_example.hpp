#ifndef tempest_rhi_examples_triangle_example_hpp
#define tempest_rhi_examples_triangle_example_hpp

#include "../example.hpp"

namespace tempest::rhi::examples
{
    class triangle_example final : public example
    {
      public:
        triangle_example() = default;
        ~triangle_example() override = default;

        [[nodiscard]] auto init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool override;
        auto render(rhi::command_list& cmd, uint32_t width, uint32_t height) -> void override;
        auto on_resize(rhi::device& dev, rhi::render_surface_format surface_format, uint32_t width,
                       uint32_t height) -> void override;
        auto shutdown(rhi::device& dev) -> void override;

        [[nodiscard]] static auto create() -> unique_ptr<example>
        {
            return make_unique<triangle_example>();
        }

      private:
        auto create_pipeline(rhi::device& dev, rhi::render_surface_format surface_format) -> bool;

        rhi::buffer_handle _positions_buffer{};
        rhi::buffer_handle _colors_buffer{};
        rhi::buffer_handle _index_buffer{};
        rhi::graphics_pipeline_handle _pipeline{};
        rhi::render_surface_format _current_format{rhi::render_surface_format::unknown};
    };
} // namespace tempest::rhi::examples

#endif
