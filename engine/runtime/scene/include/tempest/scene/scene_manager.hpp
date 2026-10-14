#ifndef tempest_scene_scene_manager_hpp
#define tempest_scene_scene_manager_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/checked.hpp>
#include <tempest/expected.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/scene_codec.hpp>
#include <tempest/scene/scene_load_handle.hpp>
#include <tempest/scene/scene_staging.hpp>
#include <tempest/scene/scene_tags.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    class asset_database;
} // namespace tempest::assets

namespace tempest::job
{
    class job_system;
} // namespace tempest::job

namespace tempest::scene
{
    /// @brief Operating modes when loading a scene document.
    enum class scene_load_mode : uint8_t
    {
        single = 0,
        additive,
    };

    /// @brief Configuration options for synchronous scene loading operations.
    struct TEMPEST_API scene_load_options
    {
        static constexpr scene_load_mode default_mode = scene_load_mode::single;
        static constexpr bool default_clear_first = false;

        scene_load_mode mode = default_mode;
        bool clear_first = default_clear_first;
    };

    /// @brief Configuration options for time-sliced asynchronous scene loading operations.
    struct TEMPEST_API scene_async_load_options
    {
        static constexpr size_t default_entities_per_frame = 100;
        static constexpr scene_load_mode default_mode = scene_load_mode::single;
        static constexpr bool default_clear_first = false;
        static constexpr bool default_strict_asset_resolution = false;

        size_t entities_per_frame = default_entities_per_frame;
        scene_load_mode mode = default_mode;
        bool clear_first = default_clear_first;
        bool strict_asset_resolution = default_strict_asset_resolution;
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
        static constexpr bool default_dirty = false;

        uint32_t generation = default_generation;
        bool occupied = default_occupied;
        bool is_dirty = default_dirty;
        string path;
        string name;
        vector<ecs::entity> root_entities;
        scene_unrecognized_data unrecognized_data;
    };

    /// @brief Type-erased base interface for asynchronous decoding and asset resolution background tasks.
    struct async_load_task_base
    {
        async_load_task_base() = default;
        async_load_task_base(const async_load_task_base&) = delete;
        async_load_task_base(async_load_task_base&&) = delete;
        virtual ~async_load_task_base() = default;
        auto operator=(const async_load_task_base&) -> async_load_task_base& = delete;
        auto operator=(async_load_task_base&&) -> async_load_task_base& = delete;
        virtual auto is_ready() -> bool = 0;
        virtual auto get_result() -> scene_staging = 0;
    };

    /// @brief Internal record tracking an in-flight or completed asynchronous scene load operation.
    struct TEMPEST_API async_load_operation
    {
        static constexpr uint32_t default_generation = 1;
        static constexpr bool default_occupied = false;
        static constexpr size_t default_cursor = 0;
        static constexpr float default_progress = 0.0f;
        static constexpr scene_load_state default_state = scene_load_state::invalid;
        static constexpr scene_handle default_scene = {};

        uint32_t generation = default_generation;
        bool occupied = default_occupied;
        scene_load_state state = default_state;
        scene_handle scene = default_scene;
        scene_async_load_options options{};
        string scene_name;
        string scene_path;
        string json_content;
        optional<scene_error> error = nullopt;
        float progress = default_progress;
        scene_staging staging{};
        size_t commit_cursor = default_cursor;
        flat_unordered_map<uint64_t, ecs::entity> local_to_entity{};
        vector<ecs::entity> committed_entities{};
        vector<ecs::entity> root_entities{};
        unique_ptr<async_load_task_base> background_task{};
    };

    /// @brief Synchronous and asynchronous scene lifecycle manager responsible for loading, unloading, saving, and
    /// entity creation.
    class TEMPEST_API scene_manager
    {
      public:
        static constexpr assets::asset_database* default_asset_database = nullptr;
        static constexpr job::job_system* default_job_system = nullptr;
        static constexpr size_t default_occupied_count = 0;
        static constexpr scene_handle default_active_scene = {};

        explicit scene_manager(ecs::archetype_registry& registry);
        scene_manager(ecs::archetype_registry& registry, assets::asset_database* assets,
                      job::job_system* jobs = default_job_system);
        scene_manager(const scene_manager&) = delete;
        scene_manager(scene_manager&&) noexcept = delete;
        ~scene_manager();

        auto operator=(const scene_manager&) -> scene_manager& = delete;
        auto operator=(scene_manager&&) noexcept -> scene_manager& = delete;

        [[nodiscard]] auto is_valid(scene_handle handle) const noexcept -> bool;
        [[nodiscard]] auto get_scene_count() const noexcept -> size_t;
        [[nodiscard]] auto get_root_entities(scene_handle handle) const -> optional<span<const ecs::entity>>;
        auto add_root_entity(scene_handle handle, ecs::entity ent) -> bool;
        auto remove_root_entity(scene_handle handle, ecs::entity ent) -> bool;
        [[nodiscard]] auto get_scene_path(scene_handle handle) const -> optional<string_view>;
        [[nodiscard]] auto get_scene_name(scene_handle handle) const -> optional<string_view>;
        [[nodiscard]] auto get_unrecognized_data(scene_handle handle) const -> const scene_unrecognized_data*;
        auto get_unrecognized_data(scene_handle handle) -> scene_unrecognized_data*;

        auto set_active_scene(scene_handle handle) -> bool;
        [[nodiscard]] auto get_active_scene() const noexcept -> scene_handle;

        [[nodiscard]] auto is_dirty(scene_handle handle) const noexcept -> bool;
        auto set_dirty(scene_handle handle, bool dirty) -> bool;

        auto create_entity(scene_handle handle, string_view name = {}) -> ecs::entity;

        auto load_sync(const filesystem::path& path, const scene_load_options& options = {})
            -> expected<scene_handle, scene_error>;
        auto load_from_string_sync(string_view json_content, string_view scene_name = {},
                                   const scene_load_options& options = {}) -> expected<scene_handle, scene_error>;

        auto load_async(const filesystem::path& path, const scene_async_load_options& options = {})
            -> scene_load_handle;
        auto load_from_string_async(string_view json_content, string_view scene_name = {},
                                    const scene_async_load_options& options = {}) -> scene_load_handle;

        auto tick() -> void;
        auto cancel_load(scene_load_handle handle) -> void;

        [[nodiscard]] auto is_load_handle_valid(scene_load_handle handle) const noexcept -> bool;
        [[nodiscard]] auto get_load_state(scene_load_handle handle) const noexcept -> scene_load_state;
        [[nodiscard]] auto get_load_progress(scene_load_handle handle) const noexcept -> float;
        [[nodiscard]] auto get_load_scene(scene_load_handle handle) const noexcept -> scene_handle;
        [[nodiscard]] auto get_load_error(scene_load_handle handle) const noexcept -> optional<scene_error>;

        auto unload(scene_handle handle) -> bool;
        auto unload_all() -> void;

        auto save_sync(scene_handle handle, const filesystem::path& path, const scene_serialize_options& options = {})
            -> expected<void, scene_error>;
        auto save_to_string(scene_handle handle, const scene_serialize_options& options = {})
            -> expected<string, scene_error>;

        [[nodiscard]] auto get_asset_database() const noexcept -> assets::asset_database*
        {
            return _assets;
        }

        [[nodiscard]] auto get_job_system() const noexcept -> job::job_system*
        {
            return _jobs;
        }

      private:
        auto allocate_slot() -> scene_handle;
        auto free_slot(uint32_t index) -> void;

        auto allocate_async_slot() -> scene_load_handle;
        auto free_async_slot(uint32_t index) -> void;

        non_null<ecs::archetype_registry> _registry;
        assets::asset_database* _assets = default_asset_database;
        job::job_system* _jobs = default_job_system;
        vector<scene_record> _scenes;
        vector<uint32_t> _free_slots;
        size_t _occupied_count = default_occupied_count;
        scene_handle _active_scene = default_active_scene;

        vector<async_load_operation> _async_ops;
        vector<uint32_t> _free_async_slots;
    };
} // namespace tempest::scene

#endif // tempest_scene_scene_manager_hpp
