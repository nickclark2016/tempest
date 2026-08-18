#ifndef tempest_render_graph_barrier_solver_hpp
#define tempest_render_graph_barrier_solver_hpp

#include <tempest/api.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/render_graph/dag.hpp>
#include <tempest/render_graph/transient_allocator.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_graph
{
    /// \brief Execution barriers and transitions required before a pass begins recording commands.
    struct pass_sync_plan
    {
        uint32_t pass_index = 0;
        queue_type queue = queue_type::graphics;
        vector<rhi::texture_barrier> texture_barriers;
        vector<rhi::buffer_barrier> buffer_barriers;
    };

    /// \brief Group of contiguous passes executing on a single queue family.
    struct queue_sync_batch
    {
        queue_type queue = queue_type::graphics;
        vector<uint32_t> pass_indices;
    };

    /// \brief Complete synchronization plan for an entire compiled frame graph.
    struct solved_synchronization
    {
        vector<pass_sync_plan> pass_plans;
        vector<queue_sync_batch> queue_batches;
    };

    /// \brief Automatic barrier deduction and hazard resolution engine.
    class TEMPEST_API barrier_solver
    {
      public:
        barrier_solver() = default;
        ~barrier_solver() = default;

        barrier_solver(const barrier_solver&) = delete;
        barrier_solver& operator=(const barrier_solver&) = delete;
        barrier_solver(barrier_solver&&) noexcept = default;
        barrier_solver& operator=(barrier_solver&&) noexcept = default;

        /// \brief Solve all pipeline barriers, layout transitions, and hazard sync points for a compiled DAG.
        [[nodiscard]] auto solve(const compiled_dag& dag, span<const pass_node> all_passes,
                                 const transient_allocator& allocator, span<const registered_texture> registered_textures)
            -> solved_synchronization;

        /// \brief Clear recorded cross-frame persistent states on shutdown or device reset.
        void clear_persistent_states() noexcept
        {
            _persistent_texture_states.clear();
        }

        /// \brief Manually record external state transition (e.g. swapchain present layout).
        void set_texture_state(uint64_t handle, enum_mask<rhi::pipeline_stage> stages,
                               enum_mask<rhi::resource_access> access, rhi::image_layout layout,
                               queue_type queue = queue_type::graphics) noexcept
        {
            _persistent_texture_states[handle] = texture_state_record{
                .stages = stages,
                .access = access,
                .layout = layout,
                .queue = queue,
                .pass_index = 0,
            };
        }

      private:
        struct texture_state_record
        {
            enum_mask<rhi::pipeline_stage> stages;
            enum_mask<rhi::resource_access> access;
            rhi::image_layout layout;
            queue_type queue;
            uint32_t pass_index;
        };

        flat_unordered_map<uint64_t, texture_state_record> _persistent_texture_states;

        struct buffer_state_record
        {
            enum_mask<rhi::pipeline_stage> stages;
            enum_mask<rhi::resource_access> access;
            queue_type queue;
            uint32_t pass_index;
        };
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_barrier_solver_hpp
