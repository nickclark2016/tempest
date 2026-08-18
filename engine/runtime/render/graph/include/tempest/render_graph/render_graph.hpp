#ifndef tempest_render_graph_render_graph_hpp
#define tempest_render_graph_render_graph_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/functional.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_graph/context.hpp>
#include <tempest/render_graph/dag.hpp>
#include <tempest/render_graph/executor.hpp>
#include <tempest/render_graph/pass_builder.hpp>
#include <tempest/render_graph/transient_allocator.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_graph
{
    class TEMPEST_API render_graph
    {
      public:
        explicit render_graph(uint32_t surface_width, uint32_t surface_height) noexcept
            : _surface_width{surface_width}, _surface_height{surface_height}
        {
        }
        ~render_graph() = default;

        render_graph(const render_graph&) = delete;
        render_graph& operator=(const render_graph&) = delete;
        render_graph(render_graph&&) noexcept = default;
        render_graph& operator=(render_graph&&) noexcept = default;

        void set_surface_size(uint32_t width, uint32_t height) noexcept;
        [[nodiscard]] auto get_surface_width() const noexcept -> uint32_t;
        [[nodiscard]] auto get_surface_height() const noexcept -> uint32_t;

        auto create_texture(const rg_texture_desc& desc) -> rg_texture_id;
        auto create_buffer(const rg_buffer_desc& desc) -> rg_buffer_id;

        auto import_texture(rhi::texture_handle handle, rhi::texture_view_handle view,
                            rhi::image_layout initial_layout = rhi::image_layout::undefined) -> rg_texture_id;
        auto import_texture(rhi::texture_handle handle,
                            rhi::image_layout initial_layout = rhi::image_layout::undefined) -> rg_texture_id;
        auto import_buffer(rhi::buffer_handle handle) -> rg_buffer_id;

        [[nodiscard]] auto resolve_texture_size(rg_texture_id tex) const -> resolved_size;

        template <typename PassData, typename SetupFn, typename ExecuteFn>
        auto add_graphics_pass(string_view name, SetupFn&& setup, ExecuteFn&& execute) -> const PassData&
        {
            return add_pass_internal<PassData>(name, queue_type::graphics, tempest::forward<SetupFn>(setup),
                                               tempest::forward<ExecuteFn>(execute));
        }

        template <typename PassData, typename SetupFn, typename ExecuteFn>
        auto add_compute_pass(string_view name, SetupFn&& setup, ExecuteFn&& execute) -> const PassData&
        {
            return add_pass_internal<PassData>(name, queue_type::async_compute, tempest::forward<SetupFn>(setup),
                                               tempest::forward<ExecuteFn>(execute));
        }

        template <typename PassData, typename SetupFn, typename ExecuteFn>
        auto add_transfer_pass(string_view name, SetupFn&& setup, ExecuteFn&& execute) -> const PassData&
        {
            return add_pass_internal<PassData>(name, queue_type::async_transfer, tempest::forward<SetupFn>(setup),
                                               tempest::forward<ExecuteFn>(execute));
        }

        auto add_present_pass(rg_texture_id src_tex, rhi::texture_handle swapchain_tex) -> void;
        auto add_clear_temporal_pass(temporal_texture& tex, rhi::clear_color_value clear_value = {}) -> void;

        void track_temporal_resource(temporal_texture* tex);
        [[nodiscard]] auto get_tracked_temporal_resources() const noexcept -> span<temporal_texture* const>;

        auto compile() -> expected<compiled_dag, dag_compile_error>;

        auto execute(rhi::device& dev, const frame_sync_options& frame_sync = {})
            -> expected<void, execution_error>;

        void reset();

        [[nodiscard]] auto get_compiler() const noexcept -> const dag_compiler&
        {
            return _compiler;
        }

        [[nodiscard]] auto get_compiler() noexcept -> dag_compiler&
        {
            return _compiler;
        }

        [[nodiscard]] auto get_allocator() const noexcept -> const transient_allocator&
        {
            return _allocator;
        }

        [[nodiscard]] auto get_allocator() noexcept -> transient_allocator&
        {
            return _allocator;
        }

        [[nodiscard]] auto get_physical_texture(uint32_t id) const noexcept -> const physical_texture_allocation*
        {
            return _allocator.get_texture(id);
        }

        [[nodiscard]] auto get_physical_buffer(uint32_t id) const noexcept -> const physical_buffer_allocation*
        {
            return _allocator.get_buffer(id);
        }

      private:
        struct erased_pass_data
        {
            void* ptr{nullptr};
            function_ref<void(void*)> deleter;

            erased_pass_data(void* p, function_ref<void(void*)> d) noexcept : ptr{p}, deleter{d}
            {
            }

            ~erased_pass_data()
            {
                if (ptr)
                {
                    deleter(ptr);
                }
            }

            erased_pass_data(const erased_pass_data&) = delete;
            erased_pass_data& operator=(const erased_pass_data&) = delete;
            erased_pass_data(erased_pass_data&& other) noexcept : ptr{other.ptr}, deleter{other.deleter}
            {
                other.ptr = nullptr;
            }
            erased_pass_data& operator=(erased_pass_data&& other) noexcept
            {
                if (this != &other)
                {
                    if (ptr)
                    {
                        deleter(ptr);
                    }
                    ptr = other.ptr;
                    deleter = other.deleter;
                    other.ptr = nullptr;
                }
                return *this;
            }
        };

        template <typename PassData>
        static void delete_pass_data(void* ptr)
        {
            delete static_cast<PassData*>(ptr);
        }

        template <typename PassData, typename SetupFn, typename ExecuteFn>
        auto add_pass_internal(string_view name, queue_type queue, SetupFn&& setup, ExecuteFn&& execute)
            -> const PassData&
        {
            auto data_ptr = new PassData();
            _pass_data_storage.push_back(erased_pass_data{data_ptr, &delete_pass_data<PassData>});

            auto node = pass_node{
                .name = string{name.data(), name.size()},
                .pass_index = 0,
                .queue = queue,
                .texture_accesses = {},
                .buffer_accesses = {},
                .texture_outputs = {},
                .buffer_outputs = {},
                .texture_fallbacks = {},
                .buffer_fallbacks = {},
                .is_sink = false,
                .enable_condition = [] { return true; },
                .execute_fn = [data_ptr, exec = tempest::forward<ExecuteFn>(execute)](
                                  pass_execution_context& ctx, rhi::command_list& cmd) {
                    exec(*data_ptr, ctx, cmd);
                },
            };

            auto builder = pass_builder{this, &node};
            setup(builder, *data_ptr);

            _compiler.add_pass(tempest::move(node));
            return *data_ptr;
        }

        dag_compiler _compiler;
        transient_allocator _allocator;
        render_graph_executor _executor;
        uint32_t _surface_width{0};
        uint32_t _surface_height{0};
        vector<erased_pass_data> _pass_data_storage;
        vector<temporal_texture*> _tracked_temporal_resources;
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_render_graph_hpp
