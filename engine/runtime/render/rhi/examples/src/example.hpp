#ifndef tempest_rhi_examples_example_hpp
#define tempest_rhi_examples_example_hpp

#include <tempest/memory.hpp>
#include <tempest/rhi.hpp>
#include <tempest/string_view.hpp>

namespace tempest::rhi::examples
{
    class example
    {
      public:
        virtual ~example() = default;

        [[nodiscard]] virtual auto init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool = 0;
        virtual auto render(rhi::command_list& cmd, uint32_t width, uint32_t height) -> void = 0;
        virtual auto on_resize([[maybe_unused]] rhi::device& dev,
                               [[maybe_unused]] rhi::render_surface_format surface_format,
                               [[maybe_unused]] uint32_t width,
                               [[maybe_unused]] uint32_t height) -> void
        {
        }
        virtual auto shutdown(rhi::device& dev) -> void = 0;
    };

    using example_factory_fn = auto (*)() -> unique_ptr<example>;

    struct example_metadata
    {
        string_view name;
        string_view description;
        example_factory_fn factory = nullptr;
    };
} // namespace tempest::rhi::examples

#endif
