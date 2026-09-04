#ifndef tempest_render_graph_dag_hpp
#define tempest_render_graph_dag_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/functional.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_graph
{
    class pass_execution_context;

    enum class access_type : uint8_t
    {
        read,
        write,
    };

    enum class attachment_type : uint8_t
    {
        none = 0,
        color,
        depth_stencil,
        resolve,
    };

    struct texture_access
    {
        rg_texture_id texture{};
        access_type type = access_type::read;
        enum_mask<rhi::pipeline_stage> stages;
        enum_mask<rhi::resource_access> access;
        rhi::image_layout layout = rhi::image_layout::general;
        rg_subresource_range subresource{};
        rhi::load_op load_op = rhi::load_op::dont_care;
        rhi::store_op store_op = rhi::store_op::dont_care;
        rhi::clear_color_value clear_color{};
        rhi::clear_depth_stencil_value clear_depth_stencil{};
        attachment_type attachment = attachment_type::none;
    };

    struct buffer_access
    {
        static constexpr uint64_t whole_size = numeric_limits<uint64_t>::max();

        rg_buffer_id buffer{};
        access_type type = access_type::read;
        enum_mask<rhi::pipeline_stage> stages;
        enum_mask<rhi::resource_access> access;
        uint64_t offset = 0;
        uint64_t size = whole_size;
    };

    struct pass_node
    {
        string name;
        uint32_t pass_index = 0;
        queue_type queue = queue_type::graphics;

        vector<texture_access> texture_accesses;
        vector<buffer_access> buffer_accesses;

        vector<rg_texture_id> texture_outputs;
        vector<rg_buffer_id> buffer_outputs;

        flat_unordered_map<rg_texture_id, rg_texture_id> texture_fallbacks;
        flat_unordered_map<rg_buffer_id, rg_buffer_id> buffer_fallbacks;

        bool is_sink = false;
        enum_mask<rhi::pipeline_statistic_flags> pipeline_statistics{rhi::pipeline_statistic_flags::none};
        function<bool()> enable_condition = [] { return true; };
        function<void(pass_execution_context&, rhi::command_list&)> execute_fn;
    };

    struct resource_lifetime
    {
        static constexpr uint32_t invalid_pass = numeric_limits<uint32_t>::max();

        uint32_t first_pass = invalid_pass;
        uint32_t last_pass = invalid_pass;

        [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
        {
            return first_pass != invalid_pass && last_pass != invalid_pass;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return is_valid();
        }
    };

    enum class dag_compile_error : uint8_t
    {
        cycle_detected,
        no_active_passes,
    };

    struct compiled_dag
    {
        vector<uint32_t> sorted_pass_indices;
        flat_unordered_map<rg_texture_id, rg_texture_id> resolved_texture_aliases;
        flat_unordered_map<rg_buffer_id, rg_buffer_id> resolved_buffer_aliases;
        flat_unordered_map<uint32_t, resource_lifetime> texture_lifetimes;
        flat_unordered_map<uint32_t, resource_lifetime> buffer_lifetimes;
    };

    struct registered_texture
    {
        uint32_t id = 0;
        rg_texture_desc desc;
        bool is_imported = false;
        rhi::texture_handle imported_handle{};
        rhi::texture_view_handle imported_view{};
        rhi::image_layout initial_layout = rhi::image_layout::undefined;
    };

    struct registered_buffer
    {
        uint32_t id = 0;
        rg_buffer_desc desc;
        bool is_imported = false;
        rhi::buffer_handle imported_handle{};
    };

    class TEMPEST_API dag_compiler
    {
      public:
        dag_compiler() = default;

        auto register_texture(const rg_texture_desc& desc) -> rg_texture_id;
        auto register_buffer(const rg_buffer_desc& desc) -> rg_buffer_id;

        auto import_texture(rhi::texture_handle handle, rhi::texture_view_handle view,
                            rhi::image_layout initial_layout = rhi::image_layout::undefined) -> rg_texture_id;
        auto import_texture(rhi::texture_handle handle, rhi::image_layout initial_layout = rhi::image_layout::undefined)
            -> rg_texture_id;
        auto import_buffer(rhi::buffer_handle handle) -> rg_buffer_id;

        auto add_pass(pass_node node) -> uint32_t;

        [[nodiscard]] auto get_passes() const noexcept -> span<const pass_node>
        {
            return _passes;
        }

        [[nodiscard]] auto get_pass(uint32_t index) const noexcept -> const pass_node&
        {
            return _passes[index];
        }

        [[nodiscard]] auto get_registered_textures() const noexcept -> span<const registered_texture>
        {
            return _textures;
        }

        [[nodiscard]] auto get_registered_buffers() const noexcept -> span<const registered_buffer>
        {
            return _buffers;
        }

        [[nodiscard]] auto compile() const -> expected<compiled_dag, dag_compile_error>;

        void reset();

      private:
        vector<registered_texture> _textures;
        vector<registered_buffer> _buffers;
        vector<pass_node> _passes;
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_dag_hpp
