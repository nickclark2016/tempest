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

        create_entity_command() = default;
        explicit create_entity_command(string_view name);
        create_entity_command(scene::scene_handle target_scene, string_view name = {});
        ~create_entity_command() override = default;

        create_entity_command(const create_entity_command&) = default;
        create_entity_command(create_entity_command&&) noexcept = default;
        auto operator=(const create_entity_command&) -> create_entity_command& = default;
        auto operator=(create_entity_command&&) noexcept -> create_entity_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;
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
        enum class mode : uint8_t
        {
            recursive,
            reparent_children,
        };

        static constexpr mode default_mode = mode::recursive;
        static constexpr bool default_was_root = false;
        static constexpr bool default_captured = false;

        struct component_data
        {
            uint32_t type_index = 0;
            vector<byte> bytes;
        };

        struct entity_node
        {
            string name;
            scene::scene_handle scene = {};
            optional<scene::entity_local_id> scene_local_id;
            optional<scene::entity_local_id> dynamic_local_id;
            optional<scene::entity_local_id> prefab_template_local_id;
            optional<scene::prefab_instance_component> prefab_instance;
            vector<component_data> components;
            vector<size_t> children;
        };

        struct entity_snapshot
        {
            vector<entity_node> nodes;
            size_t root_index = 0;
        };

        explicit destroy_entity_command(entity_address target, mode destroy_mode = default_mode);
        ~destroy_entity_command() override = default;

        destroy_entity_command(const destroy_entity_command&) = default;
        destroy_entity_command(destroy_entity_command&&) noexcept = default;
        auto operator=(const destroy_entity_command&) -> destroy_entity_command& = default;
        auto operator=(destroy_entity_command&&) noexcept -> destroy_entity_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

        struct reparented_child
        {
            entity_address addr;
            ecs::entity entity = ecs::tombstone;
        };

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

        explicit duplicate_entity_command(entity_address target);
        ~duplicate_entity_command() override = default;

        duplicate_entity_command(const duplicate_entity_command&) = default;
        duplicate_entity_command(duplicate_entity_command&&) noexcept = default;
        auto operator=(const duplicate_entity_command&) -> duplicate_entity_command& = default;
        auto operator=(duplicate_entity_command&&) noexcept -> duplicate_entity_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;
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

        rename_entity_command(entity_address target, string_view new_name);
        ~rename_entity_command() override = default;

        rename_entity_command(const rename_entity_command&) = default;
        rename_entity_command(rename_entity_command&&) noexcept = default;
        auto operator=(const rename_entity_command&) -> rename_entity_command& = default;
        auto operator=(rename_entity_command&&) noexcept -> rename_entity_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;
        [[nodiscard]] auto old_name() const noexcept -> string_view;
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

        reparent_entity_command(entity_address child, optional<entity_address> new_parent = nullopt,
                                optional<entity_address> insert_before = nullopt,
                                bool preserve_world_transform = default_preserve_world);
        ~reparent_entity_command() override = default;

        reparent_entity_command(const reparent_entity_command&) = default;
        reparent_entity_command(reparent_entity_command&&) noexcept = default;
        auto operator=(const reparent_entity_command&) -> reparent_entity_command& = default;
        auto operator=(reparent_entity_command&&) noexcept -> reparent_entity_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
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

        add_component_command(entity_address target, uint32_t type_index);
        add_component_command(entity_address target, string_view type_name);
        ~add_component_command() override = default;

        add_component_command(const add_component_command&) = default;
        add_component_command(add_component_command&&) noexcept = default;
        auto operator=(const add_component_command&) -> add_component_command& = default;
        auto operator=(add_component_command&&) noexcept -> add_component_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
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

        remove_component_command(entity_address target, uint32_t type_index);
        remove_component_command(entity_address target, string_view type_name);
        ~remove_component_command() override = default;

        remove_component_command(const remove_component_command&) = default;
        remove_component_command(remove_component_command&&) noexcept = default;
        auto operator=(const remove_component_command&) -> remove_component_command& = default;
        auto operator=(remove_component_command&&) noexcept -> remove_component_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
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

        reset_component_command(entity_address target, uint32_t type_index);
        reset_component_command(entity_address target, string_view type_name);
        ~reset_component_command() override = default;

        reset_component_command(const reset_component_command&) = default;
        reset_component_command(reset_component_command&&) noexcept = default;
        auto operator=(const reset_component_command&) -> reset_component_command& = default;
        auto operator=(reset_component_command&&) noexcept -> reset_component_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
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

        set_field_command(entity_address target, string_view component_name, string_view field_name,
                          vector<byte> new_value_bytes);
        ~set_field_command() override = default;

        set_field_command(const set_field_command&) = default;
        set_field_command(set_field_command&&) noexcept = default;
        auto operator=(const set_field_command&) -> set_field_command& = default;
        auto operator=(set_field_command&&) noexcept -> set_field_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;
        [[nodiscard]] auto merge_with(const editor_command& next) -> bool override;

        [[nodiscard]] auto target_address() const noexcept -> const entity_address&;
        [[nodiscard]] auto component_name() const noexcept -> string_view;
        [[nodiscard]] auto field_name() const noexcept -> string_view;
        [[nodiscard]] auto new_value_bytes() const noexcept -> span<const byte>;
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

    //==============================================================================
    // Prefab Authoring Commands
    //==============================================================================

    /// @brief Command reverting a prefab override (field, added component, or removed component).
    class TEMPEST_EDITOR_API revert_override_command : public editor_command
    {
      public:
        enum class override_kind : uint8_t
        {
            field,
            added_component,
            removed_component,
        };

        static constexpr override_kind default_kind = override_kind::field;
        static constexpr bool default_captured = false;

        revert_override_command(entity_address instance_root, scene::id_path target, string_view component_name,
                                string_view field_name);
        revert_override_command(entity_address instance_root, scene::id_path target, string_view component_name,
                                override_kind kind);
        ~revert_override_command() override = default;

        revert_override_command(const revert_override_command&) = default;
        revert_override_command(revert_override_command&&) noexcept = default;
        auto operator=(const revert_override_command&) -> revert_override_command& = default;
        auto operator=(revert_override_command&&) noexcept -> revert_override_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
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

        apply_override_command(entity_address instance_root, string_view file_path);
        ~apply_override_command() override = default;

        apply_override_command(const apply_override_command&) = default;
        apply_override_command(apply_override_command&&) noexcept = default;
        auto operator=(const apply_override_command&) -> apply_override_command& = default;
        auto operator=(apply_override_command&&) noexcept -> apply_override_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
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

        explicit unpack_prefab_command(entity_address instance_root, bool completely = default_completely);
        ~unpack_prefab_command() override = default;

        unpack_prefab_command(const unpack_prefab_command&) = default;
        unpack_prefab_command(unpack_prefab_command&&) noexcept = default;
        auto operator=(const unpack_prefab_command&) -> unpack_prefab_command& = default;
        auto operator=(unpack_prefab_command&&) noexcept -> unpack_prefab_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
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

        save_as_prefab_command(entity_address root, guid prefab_guid, string_view file_path);
        ~save_as_prefab_command() override = default;

        save_as_prefab_command(const save_as_prefab_command&) = default;
        save_as_prefab_command(save_as_prefab_command&&) noexcept = default;
        auto operator=(const save_as_prefab_command&) -> save_as_prefab_command& = default;
        auto operator=(save_as_prefab_command&&) noexcept -> save_as_prefab_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;

      private:
        entity_address _root_addr;
        guid _prefab_guid;
        string _file_path;
        bool _saved = default_saved;
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

        remove_unknown_component_command(scene::scene_handle scene, uint64_t entity_local_id,
                                         string_view component_name);
        ~remove_unknown_component_command() override = default;

        remove_unknown_component_command(const remove_unknown_component_command&) = default;
        remove_unknown_component_command(remove_unknown_component_command&&) noexcept = default;
        auto operator=(const remove_unknown_component_command&) -> remove_unknown_component_command& = default;
        auto operator=(remove_unknown_component_command&&) noexcept -> remove_unknown_component_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
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

        map_unknown_component_command(entity_address target, string_view unknown_component_name,
                                      uint32_t target_type_index);
        map_unknown_component_command(entity_address target, string_view unknown_component_name,
                                      string_view target_type_name);
        ~map_unknown_component_command() override = default;

        map_unknown_component_command(const map_unknown_component_command&) = default;
        map_unknown_component_command(map_unknown_component_command&&) noexcept = default;
        auto operator=(const map_unknown_component_command&) -> map_unknown_component_command& = default;
        auto operator=(map_unknown_component_command&&) noexcept -> map_unknown_component_command& = default;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
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
