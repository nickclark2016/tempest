#ifndef tempest_scene_prefab_instantiator_hpp
#define tempest_scene_prefab_instantiator_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/expected.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    /// @brief Error code returned during prefab template instantiation.
    enum class prefab_error : uint8_t
    {
        ok = 0,
        invalid_template,
        template_not_found,
        cyclic_nesting,
        component_not_found,
        field_not_found,
    };

    /// @brief Detailed outcome of instantiating a prefab template into an ECS world.
    struct TEMPEST_API prefab_instance_result
    {
        static constexpr ecs::entity default_root_entity = ecs::tombstone;
        static constexpr bool default_success = false;

        ecs::entity root_entity = default_root_entity;
        vector<ecs::entity> all_entities;
        flat_unordered_map<uint64_t, ecs::entity> local_to_entity;
        bool success = default_success;
        string error_message;
    };

    /// @brief Instantiates prefab templates into an ECS registry, building hierarchies and applying overrides.
    class TEMPEST_API prefab_instantiator
    {
      public:
        prefab_instantiator() = delete;
        prefab_instantiator(const prefab_instantiator&) = delete;
        prefab_instantiator(prefab_instantiator&&) noexcept = delete;
        ~prefab_instantiator() = delete;

        auto operator=(const prefab_instantiator&) -> prefab_instantiator& = delete;
        auto operator=(prefab_instantiator&&) noexcept -> prefab_instantiator& = delete;

        /// @brief Instantiates a prefab template into an ECS registry with optional overrides.
        static auto instantiate(
            const prefab_template& tmpl,
            ecs::archetype_registry& registry,
            const prefab_overrides& overrides = {},
            const prefab_cache* cache = nullptr,
            const guid& prefab_guid = {},
            entity_local_id instance_local_id = {}) -> expected<prefab_instance_result, prefab_error>;

        /// @brief Resolves a prefab template by GUID from a cache and instantiates it into an ECS registry.
        static auto instantiate(
            const guid& prefab_guid,
            const prefab_cache& cache,
            ecs::archetype_registry& registry,
            const prefab_overrides& overrides = {},
            entity_local_id instance_local_id = {}) -> expected<prefab_instance_result, prefab_error>;
    };
} // namespace tempest::scene

#endif // tempest_scene_prefab_instantiator_hpp
