#ifndef tempest_scene_prefab_template_hpp
#define tempest_scene_prefab_template_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/scene_staging.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    /// @brief Identifies a nested prefab instance within a prefab entity record.
    struct TEMPEST_API nested_prefab_descriptor
    {
        guid prefab_guid{};
        entity_local_id instance_local_id{};

        auto operator==(const nested_prefab_descriptor& rhs) const noexcept -> bool = default;
        auto operator!=(const nested_prefab_descriptor& rhs) const noexcept -> bool = default;
    };

    /// @brief Staged definition of an entity within a prefab template.
    struct TEMPEST_API prefab_entity_record
    {
        static constexpr entity_local_id default_local_id = {};

        entity_local_id local_id = default_local_id;
        string name;
        optional<size_t> parent_index = nullopt;
        vector<staged_component> components;
        optional<nested_prefab_descriptor> nested_instance = nullopt;
    };

    /// @brief Immutable template containing entity records for instantiating prefabs into an ECS world.
    class TEMPEST_API prefab_template
    {
      public:
        prefab_template() = default;
        explicit prefab_template(vector<prefab_entity_record> records);
        prefab_template(const prefab_template&) = default;
        prefab_template(prefab_template&&) noexcept = default;
        ~prefab_template() = default;

        auto operator=(const prefab_template&) -> prefab_template& = default;
        auto operator=(prefab_template&&) noexcept -> prefab_template& = default;

        [[nodiscard]] auto entity_count() const noexcept -> size_t;
        [[nodiscard]] auto is_empty() const noexcept -> bool;
        [[nodiscard]] auto get_records() const noexcept -> span<const prefab_entity_record>;
        [[nodiscard]] auto get_record(size_t index) const -> const prefab_entity_record*;
        [[nodiscard]] auto find_record(entity_local_id local_id) const -> const prefab_entity_record*;
        [[nodiscard]] auto find_index(entity_local_id local_id) const noexcept -> optional<size_t>;
        [[nodiscard]] auto get_root_indices() const noexcept -> span<const size_t>;
        [[nodiscard]] auto get_root_record() const noexcept -> const prefab_entity_record*;

        /// @brief Creates a prefab template from a collection of staged entities.
        static auto from_staged_entities(span<const staged_entity> entities) -> prefab_template;

        /// @brief Creates a prefab template from an ECS entity hierarchy.
        static auto from_entity(const ecs::archetype_registry& registry, ecs::entity root) -> prefab_template;

      private:
        auto _rebuild_indices() -> void;

        vector<prefab_entity_record> _records;
        flat_unordered_map<uint64_t, size_t> _local_to_index;
        vector<size_t> _root_indices;
    };

    /// @brief Programmatic builder for constructing prefab templates.
    class TEMPEST_API prefab_template_builder
    {
      public:
        auto add_entity(entity_local_id local_id, string name = {}, optional<size_t> parent_index = nullopt) -> size_t;
        auto add_component(size_t entity_index, staged_component comp) -> void;
        auto add_component(size_t entity_index, const ecs::component_type_info* type_info, span<const byte> data)
            -> void;
        auto set_nested_prefab(size_t entity_index, const guid& prefab_guid, entity_local_id instance_local_id)
            -> void;
        auto build() -> prefab_template;

      private:
        vector<prefab_entity_record> _records;
    };
} // namespace tempest::scene

#endif // tempest_scene_prefab_template_hpp
