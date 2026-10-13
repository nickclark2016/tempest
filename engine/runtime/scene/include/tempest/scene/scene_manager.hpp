#ifndef tempest_scene_scene_manager_hpp
#define tempest_scene_scene_manager_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/checked.hpp>
#include <tempest/expected.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/scene_codec.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    class asset_database;
} // namespace tempest::assets

namespace tempest::scene
{
    /// @brief Operating modes when loading a scene document.
    enum class scene_load_mode : uint8_t
    {
        single = 0,
        additive,
    };

    /// @brief Configuration options for scene loading operations.
    struct TEMPEST_API scene_load_options
    {
        static constexpr scene_load_mode default_mode = scene_load_mode::single;
        static constexpr bool default_clear_first = false;

        scene_load_mode mode = default_mode;
        bool clear_first = default_clear_first;
    };

    /// @brief Error codes returned by scene management and loading operations.
    enum class scene_error : uint8_t
    {
        file_not_found = 0,
        io_error,
        invalid_json,
        deserialization_failed,
        invalid_handle,
        already_loaded,
    };

    /// @brief Internal slot record tracking metadata, roots, and schema data for a loaded scene.
    struct TEMPEST_API scene_record
    {
        static constexpr uint32_t default_generation = 1;
        static constexpr bool default_occupied = false;

        uint32_t generation = default_generation;
        bool occupied = default_occupied;
        string path;
        string name;
        vector<ecs::entity> root_entities;
        scene_unrecognized_data unrecognized_data;
    };

    /// @brief Synchronous scene lifecycle manager responsible for loading, unloading, saving, and entity creation.
    class TEMPEST_API scene_manager
    {
      public:
        static constexpr assets::asset_database* default_asset_database = nullptr;
        static constexpr size_t default_occupied_count = 0;
        static constexpr scene_handle default_active_scene = {};

        explicit scene_manager(ecs::archetype_registry& registry);
        scene_manager(ecs::archetype_registry& registry, assets::asset_database* assets);
        scene_manager(const scene_manager&) = delete;
        scene_manager(scene_manager&&) noexcept = delete;
        ~scene_manager();

        auto operator=(const scene_manager&) -> scene_manager& = delete;
        auto operator=(scene_manager&&) noexcept -> scene_manager& = delete;

        [[nodiscard]] auto is_valid(scene_handle handle) const noexcept -> bool;
        [[nodiscard]] auto get_scene_count() const noexcept -> size_t;
        [[nodiscard]] auto get_root_entities(scene_handle handle) const -> optional<span<const ecs::entity>>;
        [[nodiscard]] auto get_scene_path(scene_handle handle) const -> optional<string_view>;
        [[nodiscard]] auto get_scene_name(scene_handle handle) const -> optional<string_view>;

        auto set_active_scene(scene_handle handle) -> bool;
        [[nodiscard]] auto get_active_scene() const noexcept -> scene_handle;

        auto create_entity(scene_handle handle, string_view name = {}) -> ecs::entity;

        auto load_sync(const filesystem::path& path, const scene_load_options& options = {})
            -> expected<scene_handle, scene_error>;
        auto load_from_string_sync(string_view json_content, string_view scene_name = {},
                                   const scene_load_options& options = {}) -> expected<scene_handle, scene_error>;

        auto unload(scene_handle handle) -> bool;
        auto unload_all() -> void;

        auto save_sync(scene_handle handle, const filesystem::path& path,
                       const scene_serialize_options& options = {}) -> expected<void, scene_error>;
        auto save_to_string(scene_handle handle, const scene_serialize_options& options = {})
            -> expected<string, scene_error>;

        [[nodiscard]] auto get_asset_database() const noexcept -> assets::asset_database*
        {
            return _assets;
        }

      private:
        auto allocate_slot() -> scene_handle;
        auto free_slot(uint32_t index) -> void;

        non_null<ecs::archetype_registry> _registry;
        [[maybe_unused]] assets::asset_database* _assets = default_asset_database;
        vector<scene_record> _scenes;
        vector<uint32_t> _free_slots;
        size_t _occupied_count = default_occupied_count;
        scene_handle _active_scene = default_active_scene;
    };
} // namespace tempest::scene

#endif // tempest_scene_scene_manager_hpp
