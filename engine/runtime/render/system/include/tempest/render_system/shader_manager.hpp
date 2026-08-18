#ifndef tempest_render_system_shader_manager_hpp
#define tempest_render_system_shader_manager_hpp

#include <tempest/api.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/rhi.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_system
{
    class TEMPEST_API shader_manager
    {
      public:
        explicit shader_manager(rhi::device& dev, string_view shader_dir = "shaders");
        ~shader_manager();

        shader_manager(const shader_manager&) = delete;
        shader_manager& operator=(const shader_manager&) = delete;
        shader_manager(shader_manager&&) noexcept;
        shader_manager& operator=(shader_manager&&) noexcept;

        /// @brief Loads SPIR-V bytecode from the specified filename in the shader directory.
        /// @return Pointer to internal cached bytecode span, or empty span if load fails.
        auto load_shader_bytecode(string_view shader_filename) -> span<const byte>;

        /// @brief Helper to construct a shader module descriptor from a loaded shader bytecode.
        auto create_shader_module_desc(string_view shader_filename, rhi::shader_stage stage,
                                       string_view entry_point = "main") -> optional<rhi::shader_module_desc>;

        /// @brief Gets an existing graphics pipeline or creates and caches a new one under the given key.
        auto get_or_create_graphics_pipeline(string_view key, const rhi::graphics_pipeline_desc& desc)
            -> rhi::graphics_pipeline_handle;

        /// @brief Gets an existing compute pipeline or creates and caches a new one under the given key.
        auto get_or_create_compute_pipeline(string_view key, const rhi::compute_pipeline_desc& desc)
            -> rhi::compute_pipeline_handle;

        /// @brief Destroys all cached graphics and compute pipelines.
        void release_all();

      private:
        rhi::device* _device{nullptr};
        string _shader_dir;
        flat_unordered_map<string, vector<byte>> _bytecode_cache;
        flat_unordered_map<string, rhi::graphics_pipeline_handle> _graphics_pipelines;
        flat_unordered_map<string, rhi::compute_pipeline_handle> _compute_pipelines;
    };
} // namespace tempest::render_system

#endif // tempest_render_system_shader_manager_hpp
