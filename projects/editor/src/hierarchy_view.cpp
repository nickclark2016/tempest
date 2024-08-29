#include <tempest/hierarchy_view.hpp>

#include <vector>

namespace tempest::editor
{
    void hierarchy_view::update(tempest::engine& eng)
    {
        auto& registry = eng.get_registry();

        _create_entities_view(registry);
    }

    void hierarchy_view::_create_entities_view(tempest::ecs::registry& registry)
    {
        // Get all root-level entities
        std::vector<tempest::ecs::entity> root_entities;

        for (auto ent : registry.entities())
        {
            auto* relationship = registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(ent);

            if (relationship && relationship->parent == tempest::ecs::null)
            {
                root_entities.push_back(ent);
            }
            else if (!relationship)
            {
                root_entities.push_back(ent);
            }
        }

        for (auto root : root_entities)
        {
            _create_entities_view_dfs(registry, root);
        }
    }

    void hierarchy_view::_create_entities_view_dfs(ecs::registry& registry, ecs::entity parent)
    {
        using traits_type = tempest::ecs::registry::traits_type;
        using imgui = tempest::graphics::imgui_context;

        auto ent_name = registry.name(parent);

        std::string name;

        if (ent_name && !ent_name->empty())
        {
            name = std::string{*ent_name};
        }
        else
        {
            name = std::format("Entity {}:{}", traits_type::as_entity(parent), traits_type::as_version(parent));
        }

        auto* relationship = registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(parent);

        if (!relationship || relationship->first_child == tempest::ecs::null)
        {
            bool selected = imgui::create_tree_node_leaf(string_view(name), [&]() {}, _selected_entity == parent);
            if (selected)
            {
                _selected_entity = parent;
            }
            return;
        }

        bool selected = imgui::create_tree_node(
            string_view(name),
            [&]() {
                auto child = relationship->first_child;

                while (child != tempest::ecs::null)
                {
                    _create_entities_view_dfs(registry, child);

                    child =
                        registry.get<tempest::ecs::relationship_component<tempest::ecs::entity>>(child).next_sibling;
                }
            },
            _selected_entity == parent);

        if (selected)
        {
            _selected_entity = parent;
        }
    }
} // namespace tempest::editor