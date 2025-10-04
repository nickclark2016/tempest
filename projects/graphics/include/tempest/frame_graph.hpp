#ifndef tempest_graphics_frame_graph_hpp
#define tempest_graphics_frame_graph_hpp

#include <tempest/concepts.hpp>
#include <tempest/functional.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/variant.hpp>
#include <tempest/vector.hpp>

namespace tempest::graphics
{
    class graph_builder;

    enum class work_type
    {
        graphics,
        compute,
        transfer,
    };

    struct graph_resource_handle
    {
        uint64_t handle : 48 = {};
        uint64_t version : 11 = {};
        uint64_t type : 5 = {};

        constexpr graph_resource_handle() = default;
        graph_resource_handle(const graph_resource_handle&) = delete;
        graph_resource_handle(graph_resource_handle&&) noexcept = default;
        ~graph_resource_handle() = default;

        constexpr graph_resource_handle(uint64_t handle, uint8_t version, rhi::rhi_handle_type type)
            : handle(handle), version(version), type(static_cast<uint8_t>(type))
        {
        }

        graph_resource_handle& operator=(const graph_resource_handle&) = delete;
        graph_resource_handle& operator=(graph_resource_handle&&) noexcept = default;
    };

    inline constexpr rhi::rhi_handle_type get_resource_type(const graph_resource_handle& handle)
    {
        return static_cast<rhi::rhi_handle_type>(handle.type);
    }

    struct scheduled_resource_access
    {
        graph_resource_handle handle;
        enum_mask<rhi::pipeline_stage> stages;
        enum_mask<rhi::memory_access> accesses;
    };

    class task_builder
    {
      public:
        void read(graph_resource_handle& handle);
        void read(graph_resource_handle& handle, enum_mask<rhi::pipeline_stage> read_hints,
                  enum_mask<rhi::memory_access> access_hints);
        void write(graph_resource_handle& handle);
        void write(graph_resource_handle& handle, enum_mask<rhi::pipeline_stage> write_hints,
                   enum_mask<rhi::memory_access> access_hints);
        void read_write(graph_resource_handle& handle);
        void read_write(graph_resource_handle& handle, enum_mask<rhi::pipeline_stage> read_hints,
                        enum_mask<rhi::memory_access> read_access_hints, enum_mask<rhi::pipeline_stage> write_hints,
                        enum_mask<rhi::memory_access> write_access_hints);

      private:
        friend class graph_builder;

        vector<scheduled_resource_access> accesses;
    };

    class graphics_task_builder : public task_builder
    {
    };

    class compute_task_builder : public task_builder
    {
      public:
        void prefer_async();

      private:
        friend class graph_builder;

        bool _prefer_async = false;
    };

    class transfer_task_builder : public task_builder
    {
      public:
        void prefer_async();

      private:
        friend class graph_builder;

        bool _prefer_async = false;
    };

    class task_execution_context
    {
    };

    class graphics_task_execution_context : public task_execution_context
    {
    };

    class compute_task_execution_context : public task_execution_context
    {
    };

    class transfer_task_execution_context : public task_execution_context
    {
    };

    struct scheduled_pass
    {
        string name;
        work_type type;
        vector<scheduled_resource_access> accesses;
        vector<graph_resource_handle> outputs;

        function<void(task_execution_context&)> execution_context;
    };

    struct ownership_transfer
    {
        graph_resource_handle handle;

        work_type src_queue;
        work_type dst_queue;

        enum_mask<rhi::pipeline_stage> src_stages;
        enum_mask<rhi::pipeline_stage> dst_stages;
        enum_mask<rhi::memory_access> src_accesses;
        enum_mask<rhi::memory_access> dst_accesses;

        uint64_t wait_value;
        uint64_t signal_value;
    };

    struct timeline_reference
    {
        work_type type;
        uint64_t queue_index;
        uint64_t value;
        enum_mask<rhi::pipeline_stage> stages;
    };

    struct submit_instructions
    {
        work_type type;
        uint32_t queue_index;

        vector<scheduled_pass> passes;
        vector<ownership_transfer> released_resources;
        vector<ownership_transfer> acquired_resources;

        vector<timeline_reference> waits;
        vector<timeline_reference> signals;
    };

    using external_resource =
        variant<rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>, rhi::typed_rhi_handle<rhi::rhi_handle_type::image>,
                rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>>;
    using internal_resource = variant<rhi::buffer_desc, rhi::image_desc>;

    struct scheduled_resource
    {
        graph_resource_handle handle;
        variant<external_resource, rhi::buffer_desc, rhi::image_desc> creation_info;
        bool per_frame;
        bool temporal;
        bool render_target;
        bool presentable;
    };

    struct graph_execution_plan
    {
        vector<scheduled_resource> resources;
        vector<submit_instructions> submissions;
    };

    struct queue_configuration
    {
        uint32_t graphics_queues = 0;
        uint32_t compute_queues = 0;
        uint32_t transfer_queues = 0;
    };

    struct pass_entry
    {
        string name;
        work_type type;
        function<void(task_execution_context&)> execution_context;
        bool async = false;

        vector<scheduled_resource_access> resource_accesses;
        vector<graph_resource_handle> outputs; // Resources written in this pass, subset of resource_accesses
    };

    struct resource_entry
    {
        string name;
        graph_resource_handle handle;

        variant<external_resource, internal_resource> resource;

        bool per_frame = false;
        bool temporal = false;
        bool render_target = false;
        bool presentable = false;
    };

    class graph_builder
    {
      public:
        // Import assets external to the graph
        graph_resource_handle import_buffer(string name, rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer);
        graph_resource_handle import_image(string name, rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image);
        graph_resource_handle import_render_surface(
            string name, rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> surface);

        // Handle transient resources
        graph_resource_handle create_per_frame_buffer(rhi::buffer_desc desc);
        graph_resource_handle create_per_frame_image(rhi::image_desc desc);

        // Handle temporal resources
        graph_resource_handle create_temporal_buffer(rhi::buffer_desc desc);
        graph_resource_handle create_temporal_image(rhi::image_desc desc);

        // Handle render targets
        graph_resource_handle create_render_target(rhi::image_desc desc);

        // Create passes
        void create_graphics_pass(string name, invocable<graphics_task_builder&> auto&& setup,
                                  invocable<graphics_task_execution_context&> auto&& record);

        void create_compute_pass(string name, invocable<compute_task_builder&> auto&& setup,
                                 invocable<compute_task_execution_context&> auto&& record);

        void create_transfer_pass(string name, invocable<transfer_task_builder&> auto&& setup,
                                  invocable<transfer_task_execution_context&> auto&& record);

        graph_execution_plan compile(queue_configuration cfg) &&;

      private:
        vector<resource_entry> _resources;
        vector<pass_entry> _passes;
        uint64_t _next_resource_id = 1;

        void _create_pass_entry(string name, work_type type, function<void(task_execution_context&)> execution_context,
                                task_builder& builder, bool async);
    };

    class graph_compiler
    {
      public:
        graph_compiler(vector<resource_entry> resources, vector<pass_entry> passes, queue_configuration cfg);

        graph_execution_plan compile();

      private:
        struct live_set
        {
            vector<size_t> pass_indices;
            vector<size_t> resource_indices;
        };

        struct dependency_edge
        {
            size_t producer_pass_index;
            size_t consumer_pass_index;
            graph_resource_handle resource;
            enum_mask<rhi::pipeline_stage> producer_stages;
            enum_mask<rhi::pipeline_stage> consumer_stages;
            enum_mask<rhi::memory_access> producer_access;
            enum_mask<rhi::memory_access> consumer_access;
        };

        struct dependency_graph
        {
            vector<dependency_edge> edges;
            vector<size_t> passes;
            vector<size_t> resources;
        };

        struct submit_batch
        {
            work_type type;
            vector<size_t> pass_indices;
        };

        vector<resource_entry> _resources;
        vector<pass_entry> _passes;
        queue_configuration _cfg;

        live_set _gather_live_set() const;
        dependency_graph _build_dependency_graph(const live_set& live) const;
        vector<size_t> _topo_sort_kahns(const dependency_graph& graph) const;
        flat_unordered_map<size_t, work_type> _assign_queue_type(const live_set& live) const;

        bool _requires_split(size_t pass_idx, work_type queue,
                             const flat_unordered_map<size_t, work_type>& queue_assignment,
                             const flat_unordered_map<uint64_t, work_type>& acquired_resource_handles) const;
        vector<submit_batch> _create_submit_batches(
            span<const size_t> topo_order, const flat_unordered_map<size_t, work_type>& queue_assignments) const;
        graph_execution_plan _build_execution_plan(span<const submit_batch> batches,
                                                   span<const size_t> resource_indices);
    };

    inline void graph_builder::create_compute_pass(string name, invocable<compute_task_builder&> auto&& setup,
                                                   invocable<compute_task_execution_context&> auto&& record)
    {
        compute_task_builder builder;
        setup(builder);

        _create_pass_entry(
            name, work_type::compute,
            [record = tempest::move(record)](task_execution_context& ctx) {
                record(static_cast<compute_task_execution_context&>(ctx));
            },
            builder, builder._prefer_async);
    }

    inline void graph_builder::create_transfer_pass(string name, invocable<transfer_task_builder&> auto&& setup,
                                                    invocable<transfer_task_execution_context&> auto&& record)
    {
        transfer_task_builder builder;
        setup(builder);

        _create_pass_entry(
            name, work_type::transfer,
            [record = tempest::move(record)](task_execution_context& ctx) {
                record(static_cast<transfer_task_execution_context&>(ctx));
            },
            builder, builder._prefer_async);
    }

    inline void graph_builder::create_graphics_pass(string name, invocable<graphics_task_builder&> auto&& setup,
                                                    invocable<graphics_task_execution_context&> auto&& record)
    {
        graphics_task_builder builder;
        setup(builder);

        _create_pass_entry(
            name, work_type::graphics,
            [record = tempest::move(record)](task_execution_context& ctx) {
                record(static_cast<graphics_task_execution_context&>(ctx));
            },
            builder, false);
    }

    class graph_executor
    {
      public:
      private:
    };
} // namespace tempest::graphics

#endif // tempest_graphics_frame_graph_hpp
