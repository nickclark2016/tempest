#ifndef tempest_rhi_examples_example_registry_hpp
#define tempest_rhi_examples_example_registry_hpp

#include "example.hpp"

#include <tempest/optional.hpp>
#include <tempest/span.hpp>

namespace tempest::rhi::examples
{
    class example_registry
    {
      public:
        [[nodiscard]] static auto get_examples() -> span<const example_metadata>;
        [[nodiscard]] static auto find_example(string_view name) -> optional<example_metadata>;
        static auto register_example(example_metadata metadata) -> void;
    };
} // namespace tempest::rhi::examples

#endif
