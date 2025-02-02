#include <tempest/entity_inspector_view.hpp>

#include <tempest/imgui_context.hpp>

namespace tempest::editor
{
    void entity_inspector_view::set_selected_entity(tempest::ecs::entity ent) noexcept
    {
        _selected_entity = ent;
    }

    void entity_inspector_view::update(tempest::engine& eng)
    {
        using imgui = tempest::graphics::imgui_context;

        imgui::create_window("Entity Inspector", [&]() {
            if (_selected_entity != ecs::null)
            {
                auto& registry = eng.get_archetype_registry();
                auto name = registry.name(_selected_entity);
                if (name && !name->empty())
                {
                    imgui::label(tempest::string_view(std::format("Name: {}", name->data())));
                }
                else
                {
                    imgui::label("Name: <unnamed>");
                }

                bool requires_update = false;
                for (const auto& factory : _component_view_factories)
                {
                    requires_update |= factory->create_view(registry, _selected_entity);
                }

                if (requires_update) [[unlikely]]
                {
                    eng.get_render_system().mark_dirty();
                }
            }
            else
            {
                imgui::label("No entity selected.");
            }
        });
    }
} // namespace tempest::editor