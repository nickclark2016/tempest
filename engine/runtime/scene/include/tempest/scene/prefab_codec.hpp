#ifndef tempest_scene_prefab_codec_hpp
#define tempest_scene_prefab_codec_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/expected.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/json.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>

namespace tempest::scene
{
    /// @brief Error codes returned by prefab template encoding and decoding routines.
    enum class prefab_codec_error : uint8_t
    {
        ok = 0,
        invalid_json,
        invalid_root_object,
        unsupported_format,
        version_mismatch,
        missing_entities,
        invalid_entity_record,
        cyclic_hierarchy,
        depth_exceeded,
        root_transform_forbidden,
    };

    /// @brief Formatting and encoding configuration options for prefab template serialization.
    struct TEMPEST_API prefab_serialize_options
    {
        static constexpr json_format default_format = json_format::pretty;

        json_format format = default_format;
    };

    /// @brief Outcome of deserializing a .tprefab document into a prefab_template.
    struct TEMPEST_API prefab_deserialize_result
    {
        static constexpr bool default_success = false;

        prefab_template template_record;
        guid prefab_guid;
        bool success = default_success;
        string error_message;
    };

    /// @brief Serialization and deserialization codec for Tempest prefab documents (.tprefab).
    class TEMPEST_API prefab_codec
    {
      public:
        prefab_codec() = delete;
        prefab_codec(const prefab_codec&) = delete;
        prefab_codec(prefab_codec&&) noexcept = delete;
        ~prefab_codec() = delete;

        auto operator=(const prefab_codec&) -> prefab_codec& = delete;
        auto operator=(prefab_codec&&) noexcept -> prefab_codec& = delete;

        /// @brief Serializes an immutable prefab template to a .tprefab JSON document.
        static auto serialize(const prefab_template& tmpl, const guid& prefab_guid = {},
                              const prefab_serialize_options& options = {}) -> string;

        /// @brief Extracts and serializes an ECS entity hierarchy to a .tprefab JSON document.
        static auto serialize(const ecs::archetype_registry& registry, ecs::entity root_entity,
                              const guid& prefab_guid = {}, const prefab_serialize_options& options = {}) -> string;

        /// @brief Deserializes a .tprefab JSON document into an immutable prefab_template.
        static auto deserialize(string_view json_content, const ecs::component_type_registry& type_registry)
            -> expected<prefab_deserialize_result, prefab_codec_error>;
    };
} // namespace tempest::scene

#endif // tempest_scene_prefab_codec_hpp
