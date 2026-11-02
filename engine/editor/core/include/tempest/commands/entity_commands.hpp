#ifndef tempest_editor_core_commands_entity_commands_hpp
#define tempest_editor_core_commands_entity_commands_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/guid.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/int.hpp>
#include <tempest/mat4.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/quat.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_operations.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor
{
    //==============================================================================
    // Entity Lifecycle Commands
    //==============================================================================

    /// @brief Command creating a new entity in a specified scene or as a dynamic session entity.
    class TEMPEST_EDITOR_API create_entity_command : public editor_command
    {
      public:
        static constexpr bool default_executed = false;

        /// @brief Constructs a command to create a new entity.
        create_entity_command() = default;

        /// @brief Constructs a command to create a new entity with the specified name.
        /// @param name The name of the new entity.
        explicit create_entity_command(string_view name);

        /// @brief Constructs a command to create a new entity in the specified scene.
        /// @param target_scene The scene in which to create the new entity.
        /// @param name The name of the new entity.
        create_entity_command(scene::scene_handle target_scene, string_view name = {});

        ~create_entity_command() override = default;

        create_entity_command(const create_entity_command&) = default;
        create_entity_command(create_entity_command&&) noexcept = default;
        auto operator=(const create_entity_command&) -> create_entity_command& = default;
        auto operator=(create_entity_command&&) noexcept -> create_entity_command& = default;

        /// @brief Applies the command in the given context.
        /// @param ctx The command context to apply the command in.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the label of the command.
        /// @return The label as a string view.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Returns the target scene of the command.
        /// @return The target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

        /// @brief Returns the address of the created entity.
        /// @return The entity address.
        [[nodiscard]] auto created_address() const noexcept -> const entity_address&;

      private:
        scene::scene_handle _scene = {};
        string _name;
        entity_address _created_address;
        bool _executed = default_executed;
    };

    /// @brief Command destroying an entity, supporting recursive snapshot destruction and child reparenting modes.
    class TEMPEST_EDITOR_API destroy_entity_command : public editor_command
    {
      public:
        /// @brief Modes for destroying an entity.
        enum class mode : uint8_t
        {
            /// @brief Recursively destroy the entity and all its children.
            recursive,

            /// @brief Reparent the children of the entity to its parent instead of destroying them.
            reparent_children,
        };

        static constexpr mode default_mode = mode::recursive;
        static constexpr bool default_was_root = false;
        static constexpr bool default_captured = false;

        /// @brief Data for a component within an entity snapshot.
        struct component_data
        {
            /// @brief The type index of the component.
            uint32_t type_index = 0;
            /// @brief The raw bytes of the component data.
            vector<byte> bytes;
        };

        /// @brief Node representing an entity within a snapshot.
        struct entity_node
        {
            /// @brief The name of the entity.
            string name;

            /// @brief The scene the entity belongs to.
            scene::scene_handle scene = {};

            /// @brief The local ID of the entity within the scene.
            optional<scene::entity_local_id> scene_local_id;

            /// @brief The dynamic local ID of the entity, if applicable.
            optional<scene::entity_local_id> dynamic_local_id;

            /// @brief The local ID of the prefab template, if applicable.
            optional<scene::entity_local_id> prefab_template_local_id;

            /// @brief The prefab instance component of the entity, if applicable.
            optional<scene::prefab_instance_component> prefab_instance;

            /// @brief The components of the entity.
            vector<component_data> components;

            /// @brief The indices of the child entities within the snapshot.
            vector<size_t> children;
        };

        /// @brief Snapshot of the entity and its hierarchy.
        struct entity_snapshot
        {
            /// @brief The nodes representing the entities in the snapshot.
            vector<entity_node> nodes;

            /// @brief The index of the root entity within the snapshot.
            size_t root_index = 0;
        };

        /// @brief Constructs a command to destroy an entity.
        /// @param target The address of the entity to destroy.
        /// @param destroy_mode The mode in which to destroy the entity.
        explicit destroy_entity_command(entity_address target, mode destroy_mode = default_mode);

        ~destroy_entity_command() override = default;

        destroy_entity_command(const destroy_entity_command&) = default;
        destroy_entity_command(destroy_entity_command&&) noexcept = default;
        auto operator=(const destroy_entity_command&) -> destroy_entity_command& = default;
        auto operator=(destroy_entity_command&&) noexcept -> destroy_entity_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Retrieves the label of the command.
        /// @return The label of the command.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Retrieves the target scene of the command.
        /// @return The target scene of the command.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

        /// @brief Represents a child entity that has been reparented as part of the destroy operation.
        struct reparented_child
        {
            /// @brief The address of the reparented child entity.
            entity_address addr;

            /// @brief The entity ID of the reparented child entity.
            ecs::entity entity = ecs::tombstone;
        };

      private:
        entity_address _target_addr;
        mode _mode = default_mode;
        entity_snapshot _snapshot;
        vector<reparented_child> _reparented_children;
        optional<entity_address> _prev_parent_addr;
        optional<entity_address> _prev_insert_before_addr;
        bool _was_root = default_was_root;
        scene::scene_handle _scene = {};
        bool _captured = default_captured;
    };

    /// @brief Command duplicating an entity and its hierarchy with fresh local IDs, inserted as a sibling.
    class TEMPEST_EDITOR_API duplicate_entity_command : public editor_command
    {
      public:
        static constexpr bool default_executed = false;
        static constexpr bool default_was_root = false;

        /// @brief Constructs a command to duplicate an entity.
        /// @param target The address of the entity to duplicate.
        explicit duplicate_entity_command(entity_address target);

        ~duplicate_entity_command() override = default;

        duplicate_entity_command(const duplicate_entity_command&) = default;
        duplicate_entity_command(duplicate_entity_command&&) noexcept = default;
        auto operator=(const duplicate_entity_command&) -> duplicate_entity_command& = default;
        auto operator=(duplicate_entity_command&&) noexcept -> duplicate_entity_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Retrieves the label of the command.
        /// @return The label of the command.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Retrieves the target scene of the command.
        /// @return The target scene of the command.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

        /// @brief Retrieves the address of the cloned entity.
        /// @return The address of the cloned entity.
        [[nodiscard]] auto cloned_address() const noexcept -> const entity_address&;

      private:
        entity_address _target_addr;
        entity_address _cloned_addr;
        destroy_entity_command::entity_snapshot _cloned_snapshot;
        optional<entity_address> _parent_addr;
        optional<entity_address> _insert_before_addr;
        bool _was_root = default_was_root;
        bool _executed = default_executed;
    };

    /// @brief Command renaming an entity.
    class TEMPEST_EDITOR_API rename_entity_command : public editor_command
    {
      public:
        static constexpr bool default_captured_old_name = false;

        /// @brief Constructs a command to rename an entity.
        /// @param target The address of the entity to rename.
        /// @param new_name The new name for the entity.
        rename_entity_command(entity_address target, string_view new_name);

        ~rename_entity_command() override = default;

        rename_entity_command(const rename_entity_command&) = default;
        rename_entity_command(rename_entity_command&&) noexcept = default;
        auto operator=(const rename_entity_command&) -> rename_entity_command& = default;
        auto operator=(rename_entity_command&&) noexcept -> rename_entity_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Retrieves the label of the command.
        /// @return The label of the command.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Retrieves the target scene of the command.
        /// @return The target scene of the command.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

        /// @brief Retrieves the old name of the entity.
        /// @return The old name of the entity.
        [[nodiscard]] auto old_name() const noexcept -> string_view;

        /// @brief Retrieves the new name of the entity.
        /// @return The new name of the entity.
        [[nodiscard]] auto new_name() const noexcept -> string_view;

      private:
        entity_address _target_addr;
        string _new_name;
        string _old_name;
        bool _captured_old_name = default_captured_old_name;
    };

    /// @brief Command reparenting an entity with optional world transform preservation.
    class TEMPEST_EDITOR_API reparent_entity_command : public editor_command
    {
      public:
        static constexpr bool default_preserve_world = false;
        static constexpr bool default_captured = false;
        static constexpr bool default_has_transform = false;
        static constexpr bool default_was_root = false;

        /// @brief Constructs a command to reparent an entity.
        /// @param child The address of the child entity to reparent.
        /// @param new_parent The optional new parent entity address.
        /// @param insert_before The optional entity address to insert before.
        /// @param preserve_world_transform Whether to preserve the world transform of the entity.
        reparent_entity_command(entity_address child, optional<entity_address> new_parent = nullopt,
                                optional<entity_address> insert_before = nullopt,
                                bool preserve_world_transform = default_preserve_world);
        ~reparent_entity_command() override = default;

        reparent_entity_command(const reparent_entity_command&) = default;
        reparent_entity_command(reparent_entity_command&&) noexcept = default;
        auto operator=(const reparent_entity_command&) -> reparent_entity_command& = default;
        auto operator=(reparent_entity_command&&) noexcept -> reparent_entity_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Retrieves the label of the command.
        /// @return The label of the command.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Retrieves the target scene of the command.
        /// @return The target scene of the command.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _child_addr;
        optional<entity_address> _new_parent_addr;
        optional<entity_address> _insert_before_addr;
        bool _preserve_world_transform = default_preserve_world;

        optional<entity_address> _prev_parent_addr;
        optional<entity_address> _prev_insert_before_addr;
        math::vec3<float> _prev_position{};
        math::quat<float> _prev_rotation{0.0F, 0.0F, 0.0F, 1.0F};
        math::vec3<float> _prev_scale{1.0F, 1.0F, 1.0F};
        bool _has_transform = default_has_transform;
        bool _was_root = default_was_root;
        bool _captured = default_captured;
    };

    //==============================================================================
    // Component Mutation Commands
    //==============================================================================

    /// @brief Command adding a component to an entity with automatic prefab override tracking.
    class TEMPEST_EDITOR_API add_component_command : public editor_command
    {
      public:
        static constexpr uint32_t invalid_type_index = ~uint32_t{0};
        static constexpr bool default_added_override = false;

        /// @brief Constructs a command to add a component to an entity.
        /// @param target The address of the target entity.
        /// @param type_index The type index of the component to add.
        add_component_command(entity_address target, uint32_t type_index);

        /// @brief Constructs a command to add a component to an entity.
        /// @param target The address of the target entity.
        /// @param type_name The name of the component type to add.
        add_component_command(entity_address target, string_view type_name);

        ~add_component_command() override = default;

        add_component_command(const add_component_command&) = default;
        add_component_command(add_component_command&&) noexcept = default;
        auto operator=(const add_component_command&) -> add_component_command& = default;
        auto operator=(add_component_command&&) noexcept -> add_component_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Retrieves the label of the command.
        /// @return The label of the command.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Retrieves the target scene of the command.
        /// @return The target scene of the command.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _target_addr;
        uint32_t _type_index = invalid_type_index;
        string _type_name;
        bool _added_override = default_added_override;
        entity_address _prefab_root_addr;
        scene::id_path _override_target_path;
    };

    /// @brief Command removing a component from an entity with automatic prefab override tracking.
    class TEMPEST_EDITOR_API remove_component_command : public editor_command
    {
      public:
        static constexpr uint32_t invalid_type_index = ~uint32_t{0};
        static constexpr bool default_added_override = false;

        /// @brief Constructs a command to remove a component from an entity.
        /// @param target The address of the target entity.
        /// @param type_index The type index of the component to remove.
        remove_component_command(entity_address target, uint32_t type_index);

        /// @brief Constructs a command to remove a component from an entity.
        /// @param target The address of the target entity.
        /// @param type_name The name of the component type to remove.
        remove_component_command(entity_address target, string_view type_name);

        ~remove_component_command() override = default;

        remove_component_command(const remove_component_command&) = default;
        remove_component_command(remove_component_command&&) noexcept = default;
        auto operator=(const remove_component_command&) -> remove_component_command& = default;
        auto operator=(remove_component_command&&) noexcept -> remove_component_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Retrieves the label of the command.
        /// @return The label of the command.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Retrieves the target scene of the command.
        /// @return The target scene of the command.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _target_addr;
        uint32_t _type_index = invalid_type_index;
        string _type_name;
        vector<byte> _saved_bytes;
        bool _added_override = default_added_override;
        entity_address _prefab_root_addr;
        scene::id_path _override_target_path;
    };

    /// @brief Command resetting a component's fields to default constructor values.
    class TEMPEST_EDITOR_API reset_component_command : public editor_command
    {
      public:
        static constexpr uint32_t invalid_type_index = ~uint32_t{0};

        /// @brief Constructs a command to reset a component's fields to default values.
        /// @param target The address of the target entity.
        /// @param type_index The type index of the component to reset.
        reset_component_command(entity_address target, uint32_t type_index);

        /// @brief Constructs a command to reset a component's fields to default values.
        /// @param target The address of the target entity.
        /// @param type_name The name of the component to reset.
        reset_component_command(entity_address target, string_view type_name);

        ~reset_component_command() override = default;

        reset_component_command(const reset_component_command&) = default;
        reset_component_command(reset_component_command&&) noexcept = default;
        auto operator=(const reset_component_command&) -> reset_component_command& = default;
        auto operator=(reset_component_command&&) noexcept -> reset_component_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Retrieves the label of the command.
        /// @return The label of the command.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Retrieves the target scene of the command.
        /// @return The target scene of the command.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _target_addr;
        uint32_t _type_index = invalid_type_index;
        string _type_name;
        vector<byte> _saved_bytes;
        vector<scene::field_override> _saved_field_overrides;
        entity_address _prefab_root_addr;
    };

    /// @brief Command mutating a single reflected field with drag coalescing and automatic prefab override tracking.
    class TEMPEST_EDITOR_API set_field_command : public editor_command
    {
      public:
        static constexpr bool default_captured = false;
        static constexpr bool default_is_in_prefab = false;
        static constexpr bool default_had_prev_override = false;

        /// @brief Constructs a command to set a field on a component of an entity.
        /// @param target The address of the target entity.
        /// @param component_name The name of the component containing the field.
        /// @param field_name The name of the field to set.
        /// @param new_value_bytes The new value for the field as a byte array.
        set_field_command(entity_address target, string_view component_name, string_view field_name,
                          vector<byte> new_value_bytes);

        ~set_field_command() override = default;

        set_field_command(const set_field_command&) = default;
        set_field_command(set_field_command&&) noexcept = default;
        auto operator=(const set_field_command&) -> set_field_command& = default;
        auto operator=(set_field_command&&) noexcept -> set_field_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command, restoring the previous state.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the label for the command.
        /// @return The label as a string view.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Returns the target scene for the command.
        /// @return The target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

        /// @brief Attempts to merge this command with the next command.
        /// @param next The next command to merge with.
        /// @return True if the commands were successfully merged, false otherwise.
        [[nodiscard]] auto merge_with(const editor_command& next) -> bool override;

        /// @brief Returns the target entity address for the command.
        /// @return The target entity address.
        [[nodiscard]] auto target_address() const noexcept -> const entity_address&;

        /// @brief Returns the name of the component targeted by the command.
        /// @return The component name.
        [[nodiscard]] auto component_name() const noexcept -> string_view;

        /// @brief Returns the name of the field targeted by the command.
        /// @return The field name.
        [[nodiscard]] auto field_name() const noexcept -> string_view;

        /// @brief Returns the new value of the field as bytes.
        /// @return The new value bytes.
        [[nodiscard]] auto new_value_bytes() const noexcept -> span<const byte>;

        /// @brief Returns the previous value of the field as bytes.
        /// @return The previous value bytes.
        [[nodiscard]] auto prev_value_bytes() const noexcept -> span<const byte>;

      private:
        entity_address _target_addr;
        string _component_name;
        string _field_name;
        vector<byte> _new_value_bytes;
        vector<byte> _prev_value_bytes;
        vector<byte> _prev_component_bytes;
        bool _captured_prev = default_captured;

        bool _is_in_prefab = default_is_in_prefab;
        entity_address _prefab_root_addr;
        scene::id_path _override_target_path;
        optional<scene::field_override> _prev_field_override;
        bool _had_prev_field_override = default_had_prev_override;
    };

    /// @brief Command assigning or replacing an asset reference field on an entity component with automatic prefab
    /// override tracking.
    class TEMPEST_EDITOR_API assign_asset_command : public editor_command
    {
      public:
        static constexpr bool default_had_component = false;
        static constexpr bool default_captured = false;
        static constexpr bool default_is_in_prefab = false;
        static constexpr bool default_was_added_component = false;
        static constexpr bool default_had_prev_override = false;

        /// @brief Constructs a command to assign an asset reference to an entity component field.
        /// @param target Persistent address of the target entity.
        /// @param component_name Reflected component type name or alias.
        /// @param field_name Reflected field name within the component.
        /// @param asset_guid GUID of the asset to assign.
        assign_asset_command(entity_address target, string_view component_name, string_view field_name,
                             guid asset_guid);

        ~assign_asset_command() override = default;

        assign_asset_command(const assign_asset_command&) = default;
        assign_asset_command(assign_asset_command&&) noexcept = default;
        auto operator=(const assign_asset_command&) -> assign_asset_command& = default;
        auto operator=(assign_asset_command&&) noexcept -> assign_asset_command& = default;

        /// @brief Applies the asset assignment, updating live components and prefab overrides.
        /// @param ctx Command context providing access to registry, scenes, and loggers.
        /// @pre Target entity must exist in the registry or scene.
        /// @post Target entity has the specified component with field set to asset_guid.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the asset assignment, restoring previous asset GUID or removing component.
        /// @param ctx Command context providing access to registry, scenes, and loggers.
        /// @post Target entity component field is restored or component is removed.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the user-facing command label.
        /// @return Command label string.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Returns the target scene handle for this command.
        /// @return Target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

        /// @brief Returns the target entity address.
        /// @return Persistent entity address reference.
        [[nodiscard]] auto target_address() const noexcept -> const entity_address&;

        /// @brief Returns the target component name.
        /// @return Component name string view.
        [[nodiscard]] auto component_name() const noexcept -> string_view;

        /// @brief Returns the target field name.
        /// @return Field name string view.
        [[nodiscard]] auto field_name() const noexcept -> string_view;

        /// @brief Returns the new asset GUID to assign.
        /// @return GUID of the asset.
        [[nodiscard]] auto asset_guid() const noexcept -> const guid&;

        /// @brief Returns the previous asset GUID if the component existed prior to execution.
        /// @return Optional previous asset GUID.
        [[nodiscard]] auto prev_asset_guid() const noexcept -> optional<guid>;

        /// @brief Returns whether the component was added by this command.
        /// @return True if component was absent and added, false otherwise.
        [[nodiscard]] auto was_added_component() const noexcept -> bool;

        /// @brief Returns whether the target entity resides within a prefab instance.
        /// @return True if entity is part of a prefab instance, false otherwise.
        [[nodiscard]] auto is_in_prefab() const noexcept -> bool;

        /// @brief Creates an asset assignment command targeted at core::material_component material_id.
        /// @param target Persistent address of the target entity.
        /// @param material_guid GUID of the material to assign.
        /// @return Configured assign_asset_command instance.
        static auto for_material(entity_address target, guid material_guid) -> assign_asset_command;

        /// @brief Creates an asset assignment command targeted at core::mesh_component mesh_id.
        /// @param target Persistent address of the target entity.
        /// @param mesh_guid GUID of the mesh to assign.
        /// @return Configured assign_asset_command instance.
        static auto for_mesh(entity_address target, guid mesh_guid) -> assign_asset_command;

      private:
        entity_address _target_addr;
        string _component_name;
        string _field_name;
        guid _new_asset;
        optional<guid> _prev_asset;
        vector<byte> _prev_component_bytes;
        bool _had_component = default_had_component;
        bool _captured = default_captured;
        bool _is_in_prefab = default_is_in_prefab;
        bool _was_added_component = default_was_added_component;

        entity_address _prefab_root_addr;
        scene::id_path _override_target_path;
        optional<scene::field_override> _prev_field_override;
        bool _had_prev_field_override = default_had_prev_override;
    };

    //==============================================================================
    // Prefab Authoring Commands
    //==============================================================================

    /// @brief Command reverting a prefab override (field, added component, or removed component).
    class TEMPEST_EDITOR_API revert_override_command : public editor_command
    {
      public:
        /// @brief Represents the kind of override being reverted.
        enum class override_kind : uint8_t
        {
            /// @brief Field override.
            field,
            /// @brief Added component override.
            added_component,
            /// @brief Removed component override.
            removed_component,
        };

        static constexpr override_kind default_kind = override_kind::field;
        static constexpr bool default_captured = false;

        /// @brief Constructs a command to revert a prefab override.
        /// @param instance_root The root entity address of the prefab instance.
        /// @param target The target path of the override.
        /// @param component_name The name of the component containing the override.
        /// @param field_name The name of the field being overridden.
        revert_override_command(entity_address instance_root, scene::id_path target, string_view component_name,
                                string_view field_name);

        /// @brief Constructs a command to revert a prefab override.
        /// @param instance_root The root entity address of the prefab instance.
        /// @param target The target path of the override.
        /// @param component_name The name of the component containing the override.
        /// @param kind The kind of override being reverted.
        revert_override_command(entity_address instance_root, scene::id_path target, string_view component_name,
                                override_kind kind);

        ~revert_override_command() override = default;

        revert_override_command(const revert_override_command&) = default;
        revert_override_command(revert_override_command&&) noexcept = default;
        auto operator=(const revert_override_command&) -> revert_override_command& = default;
        auto operator=(revert_override_command&&) noexcept -> revert_override_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the label of the command.
        /// @return The label as a string view.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Returns the target scene of the command.
        /// @return The target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _instance_root_addr;
        scene::id_path _target_path;
        string _component_name;
        string _field_name;
        override_kind _kind = default_kind;

        optional<scene::field_override> _saved_field_override;
        optional<scene::added_component> _saved_added_component;
        optional<scene::removed_component> _saved_removed_component;
        vector<byte> _saved_live_bytes;
        bool _captured = default_captured;
    };

    /// @brief Command applying all instance overrides and changes back to the backing prefab template file.
    class TEMPEST_EDITOR_API apply_override_command : public editor_command
    {
      public:
        static constexpr bool default_applied = false;

        /// @brief Constructs a command to apply all instance overrides to the backing prefab template file.
        /// @param instance_root The root entity address of the prefab instance.
        /// @param file_path The path to the prefab template file.
        apply_override_command(entity_address instance_root, string_view file_path);

        ~apply_override_command() override = default;

        apply_override_command(const apply_override_command&) = default;
        apply_override_command(apply_override_command&&) noexcept = default;
        auto operator=(const apply_override_command&) -> apply_override_command& = default;
        auto operator=(apply_override_command&&) noexcept -> apply_override_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the label of the command.
        /// @return The label as a string view.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Returns the target scene of the command.
        /// @return The target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _instance_root_addr;
        string _file_path;
        scene::prefab_overrides _prev_overrides;
        bool _applied = default_applied;
    };

    /// @brief Command unpacking a prefab instance into independent scene entities.
    class TEMPEST_EDITOR_API unpack_prefab_command : public editor_command
    {
      public:
        static constexpr bool default_completely = false;
        static constexpr bool default_captured = false;

        /// @brief Constructs a command to unpack a prefab instance into independent scene entities.
        /// @param instance_root The root entity address of the prefab instance.
        /// @param completely Whether to completely unpack the instance.
        explicit unpack_prefab_command(entity_address instance_root, bool completely = default_completely);

        ~unpack_prefab_command() override = default;

        unpack_prefab_command(const unpack_prefab_command&) = default;
        unpack_prefab_command(unpack_prefab_command&&) noexcept = default;
        auto operator=(const unpack_prefab_command&) -> unpack_prefab_command& = default;
        auto operator=(unpack_prefab_command&&) noexcept -> unpack_prefab_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the label of the command.
        /// @return The label as a string view.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Returns the target scene of the command.
        /// @return The target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _instance_root_addr;
        bool _completely = default_completely;

        struct member_snapshot
        {
            entity_address addr;
            ecs::entity live_entity = ecs::tombstone;
            scene::entity_local_id template_local_id;
            optional<scene::prefab_instance_component> nested_instance;
        };
        scene::prefab_instance_component _saved_instance_comp;
        vector<member_snapshot> _saved_members;
        bool _captured = default_captured;
    };

    /// @brief Command saving an entity hierarchy as a prefab template and converting live entities into an instance.
    class TEMPEST_EDITOR_API save_as_prefab_command : public editor_command
    {
      public:
        static constexpr bool default_saved = false;

        /// @brief Constructs a command to save an entity hierarchy as a prefab template and convert live entities into
        /// an instance.
        /// @param root The root entity address of the hierarchy to save.
        /// @param prefab_guid The GUID of the prefab template.
        /// @param file_path The file path to save the prefab template to.
        save_as_prefab_command(entity_address root, guid prefab_guid, string_view file_path);

        ~save_as_prefab_command() override = default;

        save_as_prefab_command(const save_as_prefab_command&) = default;
        save_as_prefab_command(save_as_prefab_command&&) noexcept = default;
        auto operator=(const save_as_prefab_command&) -> save_as_prefab_command& = default;
        auto operator=(save_as_prefab_command&&) noexcept -> save_as_prefab_command& = default;

        /// @brief Applies the command to the given context.
        /// @param ctx The command context to apply the command to.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the label of the command.
        /// @return The label as a string view.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Returns the target scene of the command.
        /// @return The target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _root_addr;
        guid _prefab_guid;
        string _file_path;
        bool _saved = default_saved;
    };

    /// @brief Command instantiating a prefab template into an active scene or dynamic session.
    class TEMPEST_EDITOR_API instantiate_prefab_command : public editor_command
    {
      public:
        static constexpr bool default_was_root = false;
        static constexpr bool default_executed = false;

        /// @brief Constructs an instantiation command targeting an optional scene or parent.
        /// @param prefab_guid GUID of the prefab template to instantiate.
        /// @param target_scene Optional scene handle to instantiate into.
        /// @param parent Optional persistent entity address of the parent entity.
        /// @param position Optional world position override for the root entity.
        instantiate_prefab_command(guid prefab_guid, scene::scene_handle target_scene = {},
                                   optional<entity_address> parent = nullopt,
                                   optional<math::vec3<float>> position = nullopt);

        /// @brief Constructs an instantiation command parented under a target entity.
        /// @param prefab_guid GUID of the prefab template to instantiate.
        /// @param parent Persistent entity address of the parent entity.
        /// @param position Optional world position override for the root entity.
        instantiate_prefab_command(guid prefab_guid, entity_address parent,
                                   optional<math::vec3<float>> position = nullopt);

        ~instantiate_prefab_command() override = default;

        instantiate_prefab_command(const instantiate_prefab_command&) = default;
        instantiate_prefab_command(instantiate_prefab_command&&) noexcept = default;
        auto operator=(const instantiate_prefab_command&) -> instantiate_prefab_command& = default;
        auto operator=(instantiate_prefab_command&&) noexcept -> instantiate_prefab_command& = default;

        /// @brief Instantiates the prefab into the registry/scene.
        /// @param ctx Command context providing access to registry, scenes, and loggers.
        /// @pre ctx.prefab_cache must not be null.
        /// @post Instantiated entity hierarchy exists with scene local IDs and registered root or parent.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts prefab instantiation, destroying the instantiated hierarchy.
        /// @param ctx Command context providing access to registry, scenes, and loggers.
        /// @post Instantiated entity hierarchy is destroyed and removed from scene roots.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the user-facing command label.
        /// @return Command label string.
        [[nodiscard]] auto label() const noexcept -> string_view override
        {
            return "Instantiate Prefab";
        }

        /// @brief Returns the target scene handle for this command.
        /// @return Target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override
        {
            return _scene;
        }

        /// @brief Returns the persistent address of the instantiated root entity.
        /// @return Persistent entity address reference.
        [[nodiscard]] auto instantiated_address() const noexcept -> const entity_address&
        {
            return _created_address;
        }

        /// @brief Returns the GUID of the instantiated prefab template.
        /// @return Prefab GUID reference.
        [[nodiscard]] auto prefab_guid() const noexcept -> const guid&
        {
            return _prefab_guid;
        }

      private:
        guid _prefab_guid;
        scene::scene_handle _scene = {};
        optional<entity_address> _parent_addr;
        optional<math::vec3<float>> _position;
        entity_address _created_address;
        destroy_entity_command::entity_snapshot _snapshot;
        bool _was_root = default_was_root;
        bool _executed = default_executed;
    };

    //==============================================================================
    // Schema Unknown Component Commands
    //==============================================================================

    /// @brief Command removing an unrecognized schema component entry from a scene record.
    class TEMPEST_EDITOR_API remove_unknown_component_command : public editor_command
    {
      public:
        static constexpr uint64_t default_entity_local_id = 0;
        static constexpr bool default_captured = false;

        /// @brief Constructs a command to remove an unknown component from a scene.
        /// @param scene The target scene handle.
        /// @param entity_local_id The local ID of the entity.
        /// @param component_name The name of the unknown component.
        remove_unknown_component_command(scene::scene_handle scene, uint64_t entity_local_id,
                                         string_view component_name);

        ~remove_unknown_component_command() override = default;

        remove_unknown_component_command(const remove_unknown_component_command&) = default;
        remove_unknown_component_command(remove_unknown_component_command&&) noexcept = default;
        auto operator=(const remove_unknown_component_command&) -> remove_unknown_component_command& = default;
        auto operator=(remove_unknown_component_command&&) noexcept -> remove_unknown_component_command& = default;

        /// @brief Applies the command in the given context.
        /// @param ctx The command context to apply the command in.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the label of the command.
        /// @return The label as a string view.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Returns the target scene of the command.
        /// @return The target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        scene::scene_handle _scene = {};
        uint64_t _entity_local_id = default_entity_local_id;
        string _component_name;
        optional<scene::unknown_component> _saved_component;
        bool _captured = default_captured;
    };

    /// @brief Command mapping an unrecognized schema component JSON payload into a concrete registered component.
    class TEMPEST_EDITOR_API map_unknown_component_command : public editor_command
    {
      public:
        static constexpr uint32_t invalid_type_index = ~uint32_t{0};
        static constexpr bool default_captured = false;

        /// @brief Constructs a command to map an unknown component to a concrete type.
        /// @param target The target entity address.
        /// @param unknown_component_name The name of the unknown component.
        /// @param target_type_index The type index of the target component.
        map_unknown_component_command(entity_address target, string_view unknown_component_name,
                                      uint32_t target_type_index);

        /// @brief Constructs a command to map an unknown component to a concrete type.
        /// @param target The target entity address.
        /// @param unknown_component_name The name of the unknown component.
        /// @param target_type_name The name of the target component type.
        map_unknown_component_command(entity_address target, string_view unknown_component_name,
                                      string_view target_type_name);

        ~map_unknown_component_command() override = default;

        map_unknown_component_command(const map_unknown_component_command&) = default;
        map_unknown_component_command(map_unknown_component_command&&) noexcept = default;
        auto operator=(const map_unknown_component_command&) -> map_unknown_component_command& = default;
        auto operator=(map_unknown_component_command&&) noexcept -> map_unknown_component_command& = default;

        /// @brief Applies the command in the given context.
        /// @param ctx The command context to apply the command in.
        auto apply(command_context& ctx) -> void override;

        /// @brief Reverts the command in the given context.
        /// @param ctx The command context to revert the command in.
        auto revert(command_context& ctx) -> void override;

        /// @brief Returns the label of the command.
        /// @return The label as a string view.
        [[nodiscard]] auto label() const noexcept -> string_view override;

        /// @brief Returns the target scene of the command.
        /// @return The target scene handle.
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _target_addr;
        string _unknown_component_name;
        uint32_t _target_type_index = invalid_type_index;
        string _target_type_name;
        optional<scene::unknown_component> _saved_unknown_component;
        bool _captured = default_captured;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_commands_entity_commands_hpp
