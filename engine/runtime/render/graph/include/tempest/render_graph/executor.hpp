#ifndef tempest_render_graph_executor_hpp
#define tempest_render_graph_executor_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/profiler/profiler.hpp>
#include <tempest/render_graph/barrier_solver.hpp>
#include <tempest/render_graph/dag.hpp>
#include <tempest/render_graph/transient_allocator.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_graph
{
    class render_graph;

    enum class execution_error : uint8_t
    {
        compile_failed,
        queue_submit_failed,
    };

    struct frame_sync_options
    {
        optional<rhi::semaphore_handle> wait_semaphore{nullopt};
        rhi::pipeline_stage wait_stages{rhi::pipeline_stage::attachment_output};
        optional<rhi::semaphore_handle> signal_semaphore{nullopt};
        rhi::pipeline_stage signal_stages{rhi::pipeline_stage::bottom_of_pipe};
        optional<rhi::semaphore_handle> timeline_semaphore{nullopt};
        uint64_t timeline_value{0};
        optional<rhi::texture_handle> presented_texture{nullopt};
        uint32_t flight_slot_index{0};
        uint32_t frames_in_flight{2};
        profiler::profiler_session* profiler{nullptr};
    };

    struct flight_query_state
    {
        rhi::query_pool_handle timestamp_pool{};
        rhi::query_pool_handle pipeline_stats_pool{};
        uint32_t timestamp_count{0};
        uint32_t pipeline_stats_count{0};
        uint32_t recorded_timestamp_count{0};
        uint32_t recorded_pipeline_stats_count{0};
        enum_mask<rhi::pipeline_statistic_flags> pipeline_stats_mask{rhi::pipeline_statistic_flags::none};

        struct pass_query_binding
        {
            string pass_name{};
            queue_type queue{queue_type::graphics};
            uint32_t start_timestamp_idx{0};
            uint32_t end_timestamp_idx{0};
            optional<uint32_t> pipeline_stats_idx{nullopt};
            enum_mask<rhi::pipeline_statistic_flags> pipeline_stats_flags{rhi::pipeline_statistic_flags::none};
        };

        vector<pass_query_binding> recorded_passes{};
    };

    /// \brief Executes compiled render graphs onto GPU execution ports with automatic barrier insertion,
    /// dynamic render pass management, cross-queue synchronization, and automated GPU profiling.
    class TEMPEST_API render_graph_executor
    {
      public:
        render_graph_executor() = default;
        ~render_graph_executor() = default;

        render_graph_executor(const render_graph_executor&) = delete;
        render_graph_executor& operator=(const render_graph_executor&) = delete;
        render_graph_executor(render_graph_executor&&) noexcept = default;
        render_graph_executor& operator=(render_graph_executor&&) noexcept = default;

        /// \brief Execute the render graph on the target device with optional frame sync primitives.
        auto execute(rhi::device& dev, render_graph& graph, const frame_sync_options& frame_sync = {})
            -> expected<void, execution_error>;

        /// \brief Clean up allocated sync primitives and query pools on shutdown.
        void release(rhi::device& dev);

        /// \brief Get the flight query state for a specific flight slot.
        [[nodiscard]] auto get_flight_query_state(size_t slot) const noexcept -> const flight_query_state*
        {
            if (slot < _flight_query_rings.size())
            {
                return &_flight_query_rings[slot];
            }
            return nullptr;
        }

      private:
        auto get_execution_port(rhi::device& dev, queue_type queue) -> rhi::execution_port&;

        barrier_solver _barrier_solver;
        rhi::semaphore_handle _timeline_semaphore{};
        uint64_t _current_timeline_value{0};

        vector<flight_query_state> _flight_query_rings{};
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_executor_hpp
