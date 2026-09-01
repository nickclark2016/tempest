#ifndef tempest_render_system_shader_manager_hpp
#define tempest_render_system_shader_manager_hpp

#include <filesystem>
#include <tempest/api.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/optional.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_system
{
    /// @brief Strongly typed handle representing a registered shader module (e.g. vertex or fragment stage).
    struct shader_module_handle
    {
        uint32_t id{0};
        auto operator<=>(const shader_module_handle&) const = default;
    };

    /// @brief Strongly typed handle representing a registered graphics pipeline.
    struct graphics_pipeline_handle
    {
        uint32_t id{0};
        auto operator<=>(const graphics_pipeline_handle&) const = default;
    };

    /// @brief Strongly typed handle representing a registered compute pipeline.
    struct compute_pipeline_handle
    {
        uint32_t id{0};
        auto operator<=>(const compute_pipeline_handle&) const = default;
    };

    /// @brief Descriptor for registering or loading a shader module from a memory blob or disk location.
    struct shader_module_create_info
    {
        rhi::shader_stage stage{rhi::shader_stage::vertex};
        string_view entry_point{"main"};
        optional<std::filesystem::path> disk_location{nullopt};
        span<const byte> initial_bytes{};
    };

    /// @brief Structured template defining immutable non-dynamic state for a graphics pipeline.
    struct graphics_pipeline_template
    {
        span<const shader_module_handle> shader_modules{};
        span<const rhi::data_format> color_attachment_formats{};
        optional<rhi::data_format> depth_stencil_attachment_format{nullopt};
        rhi::primitive_topology primitive_topology{rhi::primitive_topology::triangle_list};
        rhi::rasterization_state rasterization_state{};
        rhi::depth_stencil_state depth_stencil_state{};
        span<const rhi::attachment_blend_state> color_attachment_blend_states{};
    };

    /// @brief Structured template defining compute pipeline state.
    struct compute_pipeline_template
    {
        shader_module_handle shader_module{};
    };

    class TEMPEST_API shader_manager
    {
      public:
        explicit shader_manager(rhi::device& dev, assets::asset_database& asset_db);
        ~shader_manager();

        shader_manager(const shader_manager&) = delete;
        shader_manager& operator=(const shader_manager&) = delete;
        shader_manager(shader_manager&&) noexcept;
        shader_manager& operator=(shader_manager&&) noexcept;

        [[nodiscard]] auto get_asset_database() noexcept -> assets::asset_database&
        {
            return *_asset_db;
        }

        [[nodiscard]] auto get_asset_database() const noexcept -> const assets::asset_database&
        {
            return *_asset_db;
        }

        /// @brief Registers a shader module from a memory blob or disk location.
        /// If already registered with the same disk_location, returns the existing handle.
        /// If new initial_bytes are provided on re-registration, updates the module in-place and rebuilds dependent
        /// pipelines.
        auto register_shader_module(const shader_module_create_info& info) -> shader_module_handle;

        /// @brief Helper to register a shader module from a disk filename.
        auto register_shader_module(string_view filename, rhi::shader_stage stage, string_view entry_point = "main")
            -> shader_module_handle;

        /// @brief Registers a graphics pipeline under the given name with its immutable template.
        auto register_graphics_pipeline(string_view name, const graphics_pipeline_template& tmpl)
            -> graphics_pipeline_handle;

        /// @brief Registers a compute pipeline under the given name with its immutable template.
        auto register_compute_pipeline(string_view name, const compute_pipeline_template& tmpl)
            -> compute_pipeline_handle;

        /// @brief Retrieves the active underlying RHI graphics pipeline handle via O(1) integer slot lookup.
        [[nodiscard]] auto get_rhi_pipeline(graphics_pipeline_handle handle) const noexcept
            -> rhi::graphics_pipeline_handle;

        /// @brief Retrieves the active underlying RHI compute pipeline handle via O(1) integer slot lookup.
        [[nodiscard]] auto get_rhi_pipeline(compute_pipeline_handle handle) const noexcept
            -> rhi::compute_pipeline_handle;

        /// @brief Finds a previously registered graphics pipeline handle by name.
        [[nodiscard]] auto find_graphics_pipeline(string_view name) const noexcept
            -> optional<graphics_pipeline_handle>;

        /// @brief Finds a previously registered compute pipeline handle by name.
        [[nodiscard]] auto find_compute_pipeline(string_view name) const noexcept -> optional<compute_pipeline_handle>;

        /// @brief Reloads the specified shader module from disk and surgically recompiles all dependent pipelines.
        auto reload_shader_module(shader_module_handle handle) -> bool;

        /// @brief Updates in-memory bytes for a shader module and surgically recompiles all dependent pipelines.
        auto update_shader_module_bytes(shader_module_handle handle, span<const byte> new_bytes) -> bool;

        /// @brief Filewatching notification: triggers reload for any registered module matching the given path.
        auto notify_file_changed(const std::filesystem::path& path) -> bool;

        /// @brief Drains retired RHI pipelines whose GPU completion timeline sync points have been reached across all
        /// queues.
        void process_deferred_retirements();

        /// @brief Destroys all cached graphics and compute pipelines and clears registries.
        void release_all();

        // Legacy / Migration compatibility helpers
        auto load_shader_bytecode(string_view shader_filename) -> vector<byte>;
        auto create_shader_module_desc(string_view shader_filename, rhi::shader_stage stage,
                                       string_view entry_point = "main") -> optional<rhi::shader_module_desc>;
        auto get_or_create_graphics_pipeline(string_view key, const rhi::graphics_pipeline_desc& desc)
            -> rhi::graphics_pipeline_handle;
        auto get_or_create_compute_pipeline(string_view key, const rhi::compute_pipeline_desc& desc)
            -> rhi::compute_pipeline_handle;

      private:
        struct shader_module_record
        {
            optional<std::filesystem::path> disk_location{nullopt};
            optional<std::filesystem::path> canonical_path{nullopt};
            vector<byte> memory_blob{};
            rhi::shader_stage stage{rhi::shader_stage::vertex};
            string entry_point{"main"};
            uint32_t revision{0};
        };

        struct graphics_pipeline_record
        {
            string name;
            rhi::graphics_pipeline_handle rhi_pipeline{};
            vector<shader_module_handle> modules;
            vector<rhi::data_format> color_attachment_formats;
            optional<rhi::data_format> depth_stencil_attachment_format{nullopt};
            rhi::primitive_topology primitive_topology{rhi::primitive_topology::triangle_list};
            rhi::rasterization_state rasterization_state{};
            rhi::depth_stencil_state depth_stencil_state{};
            vector<rhi::attachment_blend_state> color_attachment_blend_states;
        };

        struct compute_pipeline_record
        {
            string name;
            rhi::compute_pipeline_handle rhi_pipeline{};
            shader_module_handle module{};
        };

        struct retired_pipeline_entry
        {
            rhi::graphics_pipeline_handle graphics_pipeline{};
            rhi::compute_pipeline_handle compute_pipeline{};
            inplace_vector<rhi::host_sync_point, 3> required_sync_points{};
        };

        auto resolve_path(const std::filesystem::path& input_path) const -> optional<std::filesystem::path>;
        auto read_module_bytes(const shader_module_record& mod) const -> vector<byte>;
        auto compile_graphics_pipeline(const graphics_pipeline_record& rec, span<const byte> override_bytes = {},
                                       shader_module_handle override_handle = {}) -> rhi::graphics_pipeline_handle;
        auto compile_compute_pipeline(const compute_pipeline_record& rec, span<const byte> override_bytes = {})
            -> rhi::compute_pipeline_handle;
        void enqueue_pipeline_retirement(rhi::graphics_pipeline_handle gfx, rhi::compute_pipeline_handle comp);

        rhi::device* _device{nullptr};
        assets::asset_database* _asset_db{nullptr};

        vector<shader_module_record> _modules;
        vector<graphics_pipeline_record> _graphics_pipelines;
        vector<compute_pipeline_record> _compute_pipelines;

        flat_unordered_map<string, shader_module_handle> _module_paths_to_handle;
        flat_unordered_map<string, graphics_pipeline_handle> _named_graphics_pipelines;
        flat_unordered_map<string, compute_pipeline_handle> _named_compute_pipelines;

        flat_unordered_map<uint32_t, vector<graphics_pipeline_handle>> _module_to_graphics_pipelines;
        flat_unordered_map<uint32_t, vector<compute_pipeline_handle>> _module_to_compute_pipelines;

        vector<retired_pipeline_entry> _retired_pipelines;
        flat_unordered_map<string, vector<byte>> _legacy_bytecode_cache;
    };
} // namespace tempest::render_system

#endif // tempest_render_system_shader_manager_hpp
