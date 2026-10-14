#ifndef tempest_editor_core_editor_command_hpp
#define tempest_editor_core_editor_command_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/checked.hpp>
#include <tempest/dynamic_entity_id_component.hpp>
#include <tempest/event.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#ifndef TEMPEST_EDITOR_API
#    define TEMPEST_EDITOR_API
#endif

namespace tempest::editor
{
    /// @brief Serializable and persistent addressing token identifying an entity across sessions and undo/redo operations.
    struct TEMPEST_EDITOR_API entity_address
    {
        scene::scene_handle scene = {};
        scene::id_path id_path;

        auto operator==(const entity_address& rhs) const noexcept -> bool = default;
    };

    /// @brief Computes a stable entity address for the given entity, assigning a dynamic ID component if unaddressed.
    TEMPEST_EDITOR_API auto compute_entity_address(ecs::basic_archetype_registry& registry, ecs::entity ent)
        -> entity_address;

    /// @brief Computes a stable entity address for the given entity from a const registry view.
    TEMPEST_EDITOR_API auto compute_entity_address(const ecs::basic_archetype_registry& registry, ecs::entity ent)
        -> entity_address;

    /// @brief Resolves a stable entity address back to a live entity handle in the active scene and registry.
    TEMPEST_EDITOR_API auto resolve_entity_address(const ecs::basic_archetype_registry& registry,
                                                   const scene::scene_manager& scene_mgr, const entity_address& addr)
        -> ecs::entity;

    /// @brief Execution context passed to commands providing access to engine registries and subsystems.
    struct TEMPEST_EDITOR_API command_context
    {
        static constexpr scene::prefab_cache* default_prefab_cache = nullptr;
        static constexpr event::event_registry* default_events = nullptr;

        non_null<ecs::archetype_registry> registry;
        non_null<scene::scene_manager> scene_manager;
        scene::prefab_cache* prefab_cache = default_prefab_cache;
        event::event_registry* events = default_events;
    };

    /// @brief Abstract transactional command executing and reverting scene or entity mutations.
    class TEMPEST_EDITOR_API editor_command
    {
      public:
        editor_command() = default;
        virtual ~editor_command() = default;

        editor_command(const editor_command&) = default;
        editor_command(editor_command&&) noexcept = default;
        auto operator=(const editor_command&) -> editor_command& = default;
        auto operator=(editor_command&&) noexcept -> editor_command& = default;

        virtual auto apply(command_context& ctx) -> void = 0;
        virtual auto revert(command_context& ctx) -> void = 0;
        [[nodiscard]] virtual auto label() const noexcept -> string_view = 0;
        [[nodiscard]] virtual auto target_scene() const noexcept -> scene::scene_handle
        {
            return {};
        }
        [[nodiscard]] virtual auto merge_with([[maybe_unused]] const editor_command& next) -> bool
        {
            return false;
        }
    };

    /// @brief Composite command executing a sequence of sub-commands atomically in forward order and reverting in reverse order.
    class TEMPEST_EDITOR_API compound_command : public editor_command
    {
      public:
        compound_command() = default;
        explicit compound_command(string_view label);
        compound_command(vector<unique_ptr<editor_command>> commands, string_view label = {});
        ~compound_command() override = default;

        compound_command(const compound_command&) = delete;
        compound_command(compound_command&&) noexcept = default;
        auto operator=(const compound_command&) -> compound_command& = delete;
        auto operator=(compound_command&&) noexcept -> compound_command& = default;

        auto add_command(unique_ptr<editor_command> cmd) -> void;

        auto apply(command_context& ctx) -> void override;
        auto revert(command_context& ctx) -> void override;

        [[nodiscard]] auto label() const noexcept -> string_view override;
        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override;
        [[nodiscard]] auto commands() const noexcept -> span<const unique_ptr<editor_command>>;

      private:
        vector<unique_ptr<editor_command>> _commands;
        string _label;
    };
} // namespace tempest::editor

namespace tempest
{
    template <>
    struct hash<editor::entity_address>
    {
        auto operator()(const editor::entity_address& addr) const noexcept -> size_t
        {
            auto scene_hash = hash<scene::scene_handle>{}(addr.scene);
            for (const auto& id_token : addr.id_path)
            {
                const auto h_id = hash<scene::entity_local_id>{}(id_token);
                scene_hash ^= h_id + 0x9e3779b9 + (scene_hash << 6) + (scene_hash >> 2); // NOLINT -- hash_combine on integer types
            }
            return scene_hash;
        }
    };
} // namespace tempest

#endif // tempest_editor_core_editor_command_hpp
