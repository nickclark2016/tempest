#include "example_registry.hpp"
#include "examples/triangle_example.hpp"

#include <tempest/vector.hpp>

namespace tempest::rhi::examples
{
    namespace
    {
        auto get_registry_storage() -> vector<example_metadata>&
        {
            static auto registry = [] {
                auto reg = vector<example_metadata>{};
                reg.push_back(example_metadata{
                    .name = "triangle",
                    .description = "Draws an sRGB correct RGB triangle using storage buffers and an index buffer",
                    .factory = &triangle_example::create,
                });
                return reg;
            }();
            return registry;
        }
    } // namespace

    auto example_registry::get_examples() -> span<const example_metadata>
    {
        const auto& registry = get_registry_storage();
        return span<const example_metadata>{registry.data(), registry.size()};
    }

    auto example_registry::find_example(string_view name) -> optional<example_metadata>
    {
        const auto& registry = get_registry_storage();
        for (const auto& item : registry)
        {
            if (item.name == name)
            {
                return item;
            }
        }
        return nullopt;
    }

    auto example_registry::register_example(example_metadata metadata) -> void
    {
        auto& registry = get_registry_storage();
        registry.push_back(metadata);
    }
} // namespace tempest::rhi::examples
