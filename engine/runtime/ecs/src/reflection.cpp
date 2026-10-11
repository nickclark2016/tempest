#include <tempest/reflection.hpp>

namespace tempest::ecs
{
    auto type_descriptor::find_field(string_view field_name) const noexcept -> const field_descriptor*
    {
        for (const auto& field : fields)
        {
            if (field.name == field_name)
            {
                return &field;
            }
        }
        return nullptr;
    }
} // namespace tempest::ecs
