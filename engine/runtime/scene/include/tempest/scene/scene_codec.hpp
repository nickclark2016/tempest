#ifndef tempest_scene_scene_codec_hpp
#define tempest_scene_scene_codec_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/json.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    /// @brief Formatting and encoding options for scene serialization.
    struct TEMPEST_API scene_serialize_options
    {
        static constexpr json_format default_format = json_format::pretty;

        json_format format = default_format;
    };

    /// @brief Output result of deserializing a scene document into an entity registry.
    struct TEMPEST_API scene_deserialize_result
    {
        static constexpr bool default_success = false;

        vector<ecs::entity> root_entities;
        vector<ecs::entity> all_entities;
        scene_unrecognized_data unrecognized_data;
        bool success = default_success;
        string error_message;
    };

    /// @brief Serialization and deserialization codec for Tempest scene documents (.tscene).
    class TEMPEST_API scene_codec
    {
      public:
        /// @brief Serializes a hierarchy of root entities and their descendants to a .tscene JSON string.
        /// @param registry Archetype registry containing the entities and components.
        /// @param root_entities Collection of root entities to serialize in order.
        /// @param unrecognized_data Side-table of unknown components/fields to reinject.
        /// @param options Formatting and serialization configuration options.
        /// @return Serialized JSON string representation of the scene.
        static auto serialize(ecs::basic_archetype_registry& registry,
                              span<const ecs::entity> root_entities,
                              const scene_unrecognized_data& unrecognized_data = {},
                              const scene_serialize_options& options = {}) -> string;

        /// @brief Overload accepting a vector of root entities.
        static auto serialize(ecs::basic_archetype_registry& registry,
                              const vector<ecs::entity>& root_entities,
                              const scene_unrecognized_data& unrecognized_data = {},
                              const scene_serialize_options& options = {}) -> string
        {
            return serialize(registry, span<const ecs::entity>{root_entities.data(), root_entities.size()},
                             unrecognized_data, options);
        }

        /// @brief Deserializes a .tscene JSON document into the specified registry.
        /// @param registry Archetype registry into which entities and components are instantiated.
        /// @param scene Generational scene handle to associate with instantiated entities.
        /// @param json_content Raw JSON content string of the scene document.
        /// @return Result containing roots, all instantiated entities, unrecognized data, and status.
        static auto deserialize(ecs::basic_archetype_registry& registry,
                                scene_handle scene,
                                string_view json_content) -> scene_deserialize_result;
    };
} // namespace tempest::scene

#endif // tempest_scene_scene_codec_hpp
