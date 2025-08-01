#ifndef tempest_editor_entity_panes_hpp
#define tempest_editor_entity_panes_hpp

#include <tempest/archetype.hpp>
#include <tempest/pane.hpp>

namespace tempest::editor
{
    /// <summary>
    /// Pane used to inspect and edit properties of a selected entity.
    /// </summary>
    class entity_inspector final : public ui::pane
    {
      public:
        explicit entity_inspector(ecs::archetype_registry& registry);
        entity_inspector(const entity_inspector&) = delete;
        entity_inspector(entity_inspector&&) noexcept = delete;
        ~entity_inspector() override = default;

        entity_inspector& operator=(const entity_inspector&) = delete;
        entity_inspector& operator=(entity_inspector&&) noexcept = delete;

        void render() override;
        bool should_render() const noexcept override;
        bool should_close() const noexcept override;

        string_view name() const noexcept override
        {
            return "Entity Inspector";
        }

        void set_selected_entity(ecs::archetype_entity entity) noexcept
        {
            _selected_entity = entity;
        }

        ecs::archetype_entity get_selected_entity() const noexcept
        {
            return _selected_entity;
        }

      private:
        ecs::archetype_entity _selected_entity = ecs::tombstone;
        ecs::archetype_registry* _registry = nullptr;
    };

    /// <summary>
    /// Pane that displays the hierarchy of entities in the current scene.
    /// </summary>
    class scene_hierarchy final : public ui::pane
    {
      public:
        explicit scene_hierarchy(ecs::archetype_registry& registry);
        scene_hierarchy(const scene_hierarchy&) = delete;
        scene_hierarchy(scene_hierarchy&&) noexcept = delete;
        ~scene_hierarchy() override = default;

        scene_hierarchy& operator=(const scene_hierarchy&) = delete;
        scene_hierarchy& operator=(scene_hierarchy&&) noexcept = delete;

        void render() override;
        bool should_render() const noexcept override;
        bool should_close() const noexcept override;

        string_view name() const noexcept override
        {
            return "Scene Hierarchy";
        }

        ecs::archetype_entity get_selected_entity() const noexcept
        {
            return _selected_entity;
        }

        void set_selected_entity(ecs::archetype_entity entity) noexcept
        {
            _selected_entity = entity;
        }

      private:
        ecs::archetype_registry* _registry = nullptr;
        ecs::archetype_entity _selected_entity = ecs::tombstone;
    };
} // namespace tempest::editor

#endif // tempest_editor_entity_panes_hpp
