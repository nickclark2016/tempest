#ifndef tempest_scene_prefab_operations_hpp
#define tempest_scene_prefab_operations_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/expected.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/string_view.hpp>

namespace tempest::scene
{
    /// @brief Error codes returned by prefab authoring and lifecycle operations.
    enum class prefab_operation_error : uint8_t
    {
        ok = 0,
        invalid_entity,
        not_a_prefab_instance,
        template_not_found,
        component_not_found,
        field_not_found,
        override_not_found,
        file_io_error,
        target_not_found,
    };

    /// @brief Authoring operations and lifecycle management API for prefabs and prefab instances.
    class TEMPEST_API prefab_operations
    {
      public:
        prefab_operations() = delete;
        prefab_operations(const prefab_operations&) = delete;
        prefab_operations(prefab_operations&&) noexcept = delete;
        ~prefab_operations() = delete;

        auto operator=(const prefab_operations&) -> prefab_operations& = delete;
        auto operator=(prefab_operations&&) noexcept -> prefab_operations& = delete;

        /// @brief Extracts a live entity hierarchy, registers it in cache, optionally saves to disk,
        /// and converts the live entities in-place into a linked prefab instance.
        static auto save_as_prefab(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                   const guid& prefab_guid, string_view file_path, prefab_cache& cache)
            -> expected<prefab_template, prefab_operation_error>;

        /// @brief Reverts a field-level override on an entity within a prefab instance back to its template value.
        static auto revert_field_override(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                          const id_path& target, string_view component_name, string_view field_name,
                                          const prefab_cache& cache) -> expected<void, prefab_operation_error>;

        /// @brief Reverts an added component override by removing the component from the live entity and override record.
        static auto revert_added_component(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                           const id_path& target, string_view component_name)
            -> expected<void, prefab_operation_error>;

        /// @brief Reverts a removed component override by restoring the component from template record onto live entity.
        static auto revert_removed_component(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                             const id_path& target, string_view component_name,
                                             const prefab_cache& cache) -> expected<void, prefab_operation_error>;

        /// @brief Applies live modifications and overrides to the backing template and disk file, clearing instance overrides.
        static auto apply_to_prefab(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                    string_view file_path, prefab_cache& cache)
            -> expected<void, prefab_operation_error>;

        /// @brief Unpacks a prefab instance, converting its immediate members to standard scene entities
        /// while preserving any nested prefab instances and their children.
        static auto unpack(ecs::basic_archetype_registry& registry, ecs::entity root_entity)
            -> expected<void, prefab_operation_error>;

        /// @brief Unpacks a prefab instance and all nested prefab instances throughout the entire hierarchy.
        static auto unpack_completely(ecs::basic_archetype_registry& registry, ecs::entity root_entity)
            -> expected<void, prefab_operation_error>;
    };
} // namespace tempest::scene

#endif // tempest_scene_prefab_operations_hpp
