#ifndef tempest_graphics_frame_graph_hpp
#define tempest_graphics_frame_graph_hpp

#include <tempest/concepts.hpp>
#include <tempest/enum.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/functional.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/tuple.hpp>
#include <tempest/utility.hpp>
#include <tempest/variant.hpp>
#include <tempest/vector.hpp>

namespace tempest::graphics
{
    class graph_builder;
    class graph_executor;

    enum class work_type
    {
        unknown,
        graphics,
        compute,
        transfer,
    };

    struct base_graph_resource_handle
    {
        uint64_t handle : 48 = {};
        uint64_t version : 11 = {};
        uint64_t type : 5 = {};

        constexpr base_graph_resource_handle() = default;

        constexpr base_graph_resource_handle(uint64_t handle, uint8_t version, rhi::rhi_handle_type type)
            : handle(handle), version(version), type(static_cast<uint8_t>(type))
        {
        }

        constexpr base_graph_resource_handle(uint64_t handle, uint8_t version, uint8_t type)
            : handle(handle), version(version), type(type)
        {
        }
    };

    template <rhi::rhi_handle_type T>
    struct graph_resource_handle : base_graph_resource_handle
    {
        using base_graph_resource_handle::base_graph_resource_handle;
    };

    template <rhi::rhi_handle_type T>
    inline constexpr rhi::rhi_handle_type get_resource_type(const graph_resource_handle<T>& handle)
    {
        return static_cast<rhi::rhi_handle_type>(handle.type);
    }

    inline constexpr rhi::rhi_handle_type get_resource_type(const base_graph_resource_handle& handle)
    {
        return static_cast<rhi::rhi_handle_type>(handle.type);
    }

    struct scheduled_resource_access
    {
        base_graph_resource_handle handle;
        enum_mask<rhi::pipeline_stage> stages;
        enum_mask<rhi::memory_access> accesses;
        rhi::image_layout layout;
    };

    class task_builder
    {
      public:
        void read(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle);
        void read(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle,
                  enum_mask<rhi::pipeline_stage> read_hints, enum_mask<rhi::memory_access> access_hints);

        void read(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout);
        void read(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout,
                  enum_mask<rhi::pipeline_stage> read_hints, enum_mask<rhi::memory_access> access_hints);

        void read(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle, rhi::image_layout layout);
        void read(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle, rhi::image_layout layout,
                  enum_mask<rhi::pipeline_stage> read_hints, enum_mask<rhi::memory_access> access_hints);

        void write(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle);
        void write(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle,
                   enum_mask<rhi::pipeline_stage> write_hints, enum_mask<rhi::memory_access> access_hints);

        void write(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout);
        void write(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout,
                   enum_mask<rhi::pipeline_stage> write_hints, enum_mask<rhi::memory_access> access_hints);

        void write(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle, rhi::image_layout layout);
        void write(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle, rhi::image_layout layout,
                   enum_mask<rhi::pipeline_stage> write_hints, enum_mask<rhi::memory_access> access_hints);

        void read_write(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle);
        void read_write(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle,
                        enum_mask<rhi::pipeline_stage> read_hints, enum_mask<rhi::memory_access> read_access_hints,
                        enum_mask<rhi::pipeline_stage> write_hints, enum_mask<rhi::memory_access> write_access_hints);

        void read_write(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout);
        void read_write(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout,
                        enum_mask<rhi::pipeline_stage> read_hints, enum_mask<rhi::memory_access> read_access_hints,
                        enum_mask<rhi::pipeline_stage> write_hints, enum_mask<rhi::memory_access> write_access_hints);

        void read_write(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle, rhi::image_layout layout);
        void read_write(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle, rhi::image_layout layout,
                        enum_mask<rhi::pipeline_stage> read_hints, enum_mask<rhi::memory_access> read_access_hints,
                        enum_mask<rhi::pipeline_stage> write_hints, enum_mask<rhi::memory_access> write_access_hints);

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
      public:
        rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> find_buffer(
            graph_resource_handle<rhi::rhi_handle_type::buffer> handle) const;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::image> find_image(
            graph_resource_handle<rhi::rhi_handle_type::image> handle) const;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::image> find_image(
            graph_resource_handle<rhi::rhi_handle_type::render_surface> handle) const;

        void bind_descriptor_buffers(rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout,
                                     rhi::bind_point point, uint32_t first_set,
                                     span<const rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>> buffers,
                                     span<const uint64_t> offsets);

        void bind_descriptor_buffers(rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout,
                                     rhi::bind_point point, uint32_t first_set,
                                     span<const graph_resource_handle<rhi::rhi_handle_type::buffer>> buffers);

        void push_descriptors(rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout,
                              rhi::bind_point point, uint32_t set_idx,
                              span<const rhi::buffer_binding_descriptor> buffers,
                              span<const rhi::image_binding_descriptor> images,
                              span<const rhi::sampler_binding_descriptor> samplers);

        template <typename T>
        void push_constants(rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout,
                            enum_mask<rhi::shader_stage> stages, uint32_t offset, const T& data)
        {
            _raw_push_constants(layout, stages, offset,
                                span<const byte>(reinterpret_cast<const byte*>(&data), sizeof(T)));
        }

      protected:
        task_execution_context(graph_executor* executor,
                               rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> cmd_list,
                               rhi::work_queue* queue)
            : _executor(executor), _cmd_list(cmd_list), _queue(queue)
        {
        }

        void _raw_push_constants(rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout,
                                 enum_mask<rhi::shader_stage> stages, uint32_t offset, span<const byte> data);

        graph_executor* _executor = nullptr;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> _cmd_list;
        rhi::work_queue* _queue = nullptr;
    };

    class graphics_task_execution_context : public task_execution_context
    {
      public:
        void begin_render_pass(const rhi::work_queue::render_pass_info& info);
        void end_render_pass();

        void set_viewport(float x, float y, float width, float height, float min_depth, float max_depth,
                          bool flipped = true);
        void set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        void set_cull_mode(enum_mask<rhi::cull_mode> mode);

        void bind_pipeline(rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline);
        void bind_index_buffer(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> index_buffer, rhi::index_format type,
                               uint64_t offset);

        void draw_indirect(rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> indirect_buffer, uint32_t offset,
                           uint32_t draw_count, uint32_t stride);
        void draw_indirect(graph_resource_handle<rhi::rhi_handle_type::buffer> indirect_buffer, uint32_t offset,
                           uint32_t draw_count, uint32_t stride);

        void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0,
                  uint32_t first_instance = 0);

      private:
        friend class graph_executor;

        using task_execution_context::task_execution_context;
    };

    class compute_task_execution_context : public task_execution_context
    {
      public:
        void bind_pipeline(rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline);

        void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);

      private:
        friend class graph_executor;

        using task_execution_context::task_execution_context;
    };

    class transfer_task_execution_context : public task_execution_context
    {
      public:
        void clear_color(const graph_resource_handle<rhi::rhi_handle_type::image>& image, float r, float g, float b,
                         float a);
        void clear_color(const graph_resource_handle<rhi::rhi_handle_type::render_surface>& image, float r, float g,
                         float b, float a);

        void copy_buffer_to_buffer(const graph_resource_handle<rhi::rhi_handle_type::buffer>& src,
                                   const graph_resource_handle<rhi::rhi_handle_type::buffer>& dst, uint64_t src_offset,
                                   uint64_t dst_offset, uint64_t size);

        void fill_buffer(const graph_resource_handle<rhi::rhi_handle_type::buffer>& dst, uint64_t offset, uint64_t size,
                         uint32_t data);

        void blit(const graph_resource_handle<rhi::rhi_handle_type::image>& src,
                  const graph_resource_handle<rhi::rhi_handle_type::image>& dst);
        void blit(const graph_resource_handle<rhi::rhi_handle_type::image>& src,
                  const graph_resource_handle<rhi::rhi_handle_type::render_surface>& dst);

      private:
        friend class graph_executor;

        using task_execution_context::task_execution_context;
    };

    struct scheduled_pass
    {
        string name;
        work_type type = work_type::unknown;
        vector<scheduled_resource_access> accesses;
        vector<base_graph_resource_handle> outputs;

        function<void(task_execution_context&)> execution_context;
    };

    struct ownership_transfer
    {
        base_graph_resource_handle handle;

        work_type src_queue;
        work_type dst_queue;

        enum_mask<rhi::pipeline_stage> src_stages;
        enum_mask<rhi::pipeline_stage> dst_stages;
        enum_mask<rhi::memory_access> src_accesses;
        enum_mask<rhi::memory_access> dst_accesses;

        uint64_t wait_value;
        uint64_t signal_value;

        rhi::image_layout src_layout;
        rhi::image_layout dst_layout;
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
        base_graph_resource_handle handle;
        variant<external_resource, rhi::buffer_desc, rhi::image_desc> creation_info;
        bool per_frame;
        bool temporal;
        bool render_target;
        bool presentable;
    };

    struct queue_configuration
    {
        uint32_t graphics_queues = 0;
        uint32_t compute_queues = 0;
        uint32_t transfer_queues = 0;
    };

    struct graph_execution_plan
    {
        vector<scheduled_resource> resources;
        vector<submit_instructions> submissions;
        queue_configuration queue_cfg;
    };

    struct pass_entry
    {
        string name;
        work_type type;
        function<void(task_execution_context&)> execution_context;
        bool async = false;

        vector<scheduled_resource_access> resource_accesses;
        vector<base_graph_resource_handle> outputs; // Resources written in this pass, subset of resource_accesses
    };

    struct resource_entry
    {
        string name;
        base_graph_resource_handle handle;

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
        graph_resource_handle<rhi::rhi_handle_type::buffer> import_buffer(
            string name, rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer);
        graph_resource_handle<rhi::rhi_handle_type::image> import_image(
            string name, rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image);
        graph_resource_handle<rhi::rhi_handle_type::render_surface> import_render_surface(
            string name, rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> surface);

        // Handle transient resources
        graph_resource_handle<rhi::rhi_handle_type::buffer> create_per_frame_buffer(rhi::buffer_desc desc);
        graph_resource_handle<rhi::rhi_handle_type::image> create_per_frame_image(rhi::image_desc desc);

        // Handle temporal resources
        graph_resource_handle<rhi::rhi_handle_type::buffer> create_temporal_buffer(rhi::buffer_desc desc);
        graph_resource_handle<rhi::rhi_handle_type::image> create_temporal_image(rhi::image_desc desc);

        // Handle persistent resources
        graph_resource_handle<rhi::rhi_handle_type::buffer> create_buffer(rhi::buffer_desc desc);
        graph_resource_handle<rhi::rhi_handle_type::image> create_image(rhi::image_desc desc);

        // Handle render targets
        graph_resource_handle<rhi::rhi_handle_type::image> create_render_target(rhi::image_desc desc);

        // Create passes
        template <typename... ExecTs>
        void create_graphics_pass(
            string name, invocable<graphics_task_builder&> auto&& setup,
            invocable_no_capture<graphics_task_execution_context&, unwrap_reference_t<ExecTs>...> auto&& record,
            ExecTs&&... exec_args);

        template <typename... ExecTs>
        void create_compute_pass(
            string name, invocable<compute_task_builder&> auto&& setup,
            invocable_no_capture<compute_task_execution_context&, unwrap_reference_t<ExecTs>...> auto&& record,
            ExecTs&&... exec_args);

        template <typename... ExecTs>
        void create_transfer_pass(
            string name, invocable<transfer_task_builder&> auto&& setup,
            invocable_no_capture<transfer_task_execution_context&, unwrap_reference_t<ExecTs>...> auto&& record,
            ExecTs&&... exec_args);

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
            base_graph_resource_handle resource;
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

    template <typename... ExecTs>
    inline void graph_builder::create_compute_pass(
        string name, invocable<compute_task_builder&> auto&& setup,
        invocable_no_capture<compute_task_execution_context&, unwrap_reference_t<ExecTs>...> auto&& record,
        ExecTs&&... exec_args)
    {
        compute_task_builder builder;
        setup(builder);

        if constexpr (sizeof...(ExecTs) == 0)
        {
            _create_pass_entry(
                name, work_type::compute,
                [record = tempest::move(record)](task_execution_context& ctx) {
                    auto compute_ctx = static_cast<compute_task_execution_context&>(ctx);
                    record(compute_ctx);
                },
                builder, builder._prefer_async);
        }
        else
        {
            auto tup = tempest::make_tuple(tempest::forward<ExecTs>(exec_args)...);

            _create_pass_entry(
                name, work_type::compute,
                [record = tempest::move(record), args = tempest::move(tup)](task_execution_context& ctx) {
                    auto compute_ctx = static_cast<compute_task_execution_context&>(ctx);
                    tempest::apply(
                        [&](auto&&... unpacked) {
                            record(compute_ctx, tempest::forward<decltype(unpacked)>(unpacked)...);
                        },
                        args);
                },
                builder, builder._prefer_async);
        }
    }

    template <typename... ExecTs>
    inline void graph_builder::create_transfer_pass(
        string name, invocable<transfer_task_builder&> auto&& setup,
        invocable_no_capture<transfer_task_execution_context&, unwrap_reference_t<ExecTs>...> auto&& record,
        ExecTs&&... exec_args)
    {
        transfer_task_builder builder;
        setup(builder);

        if constexpr (sizeof...(ExecTs) == 0)
        {
            _create_pass_entry(
                name, work_type::transfer,
                [record = tempest::move(record)](task_execution_context& ctx) {
                    auto& transfer_ctx = static_cast<transfer_task_execution_context&>(ctx);
                    record(transfer_ctx);
                },
                builder, builder._prefer_async);
        }
        else
        {
            auto tup = tempest::make_tuple(tempest::forward<ExecTs>(exec_args)...);

            _create_pass_entry(
                name, work_type::transfer,
                [record = tempest::move(record), args = tempest::move(tup)](task_execution_context& ctx) {
                    auto& transfer_ctx = static_cast<transfer_task_execution_context&>(ctx);
                    tempest::apply(
                        [&](auto&&... unpacked) {
                            record(transfer_ctx, tempest::forward<decltype(unpacked)>(unpacked)...);
                        },
                        args);
                },
                builder, builder._prefer_async);
        }
    }

    template <typename... ExecTs>
    inline void graph_builder::create_graphics_pass(
        string name, invocable<graphics_task_builder&> auto&& setup,
        invocable_no_capture<graphics_task_execution_context&, unwrap_reference_t<ExecTs>...> auto&& record,
        ExecTs&&... exec_args)
    {
        graphics_task_builder builder;
        setup(builder);

        if constexpr (sizeof...(ExecTs) == 0)
        {
            _create_pass_entry(
                name, work_type::graphics,
                [record = tempest::move(record)](task_execution_context& ctx) {
                    auto graphics_ctx = static_cast<graphics_task_execution_context&>(ctx);
                    record(graphics_ctx);
                },
                builder, false);
        }
        else
        {
            auto tup = tempest::make_tuple(tempest::forward<ExecTs>(exec_args)...);
            _create_pass_entry(
                name, work_type::graphics,
                [record = tempest::move(record), args = tempest::move(tup)](task_execution_context& ctx) {
                    auto graphics_ctx = static_cast<graphics_task_execution_context&>(ctx);
                    tempest::apply(
                        [&](auto&&... unpacked) {
                            record(graphics_ctx, tempest::forward<decltype(unpacked)>(unpacked)...);
                        },
                        args);
                },
                builder, false);
        }
    }

    struct execution_fence
    {
        rhi::typed_rhi_handle<rhi::rhi_handle_type::fence> fence =
            rhi::typed_rhi_handle<rhi::rhi_handle_type::fence>::null_handle;
        bool queue_used = false;
    };

    class graph_executor
    {
      public:
        explicit graph_executor(rhi::device& device);

        void execute();
        void set_execution_plan(graph_execution_plan plan);

        // Access RHI handles
        rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> get_buffer(const base_graph_resource_handle& handle) const;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::image> get_image(const base_graph_resource_handle& handle) const;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> get_render_surface(
            const base_graph_resource_handle& handle) const;
        uint64_t get_current_frame_resource_offset(graph_resource_handle<rhi::rhi_handle_type::buffer> buffer) const;
        uint64_t get_resource_size(graph_resource_handle<rhi::rhi_handle_type::buffer> buffer) const;

      private:
        rhi::device* _device;
        optional<graph_execution_plan> _plan;

        struct buffer_usage
        {
            uint64_t offset;
            uint64_t range;
        };

        struct image_usage
        {
            uint32_t base_mip;
            uint32_t mip_levels;
            uint32_t base_array_layer;
            uint32_t array_layers;
            rhi::image_layout layout;
        };

        struct resource_usage
        {
            work_type queue;
            uint32_t queue_index;
            enum_mask<rhi::pipeline_stage> stages;
            enum_mask<rhi::memory_access> accesses;
            variant<buffer_usage, image_usage> usage;
            uint64_t timeline_value;
        };

        struct write_barrier_details
        {
            enum_mask<rhi::pipeline_stage> write_stages;
            enum_mask<rhi::memory_access> write_accesses;

            enum_mask<rhi::pipeline_stage> read_stages_seen;
            enum_mask<rhi::memory_access> read_accesses_seen;
        };

        struct per_frame_in_flight_usage
        {
            flat_unordered_map<uint64_t, resource_usage> resource_states; // handle -> state
        };

        flat_unordered_map<uint64_t, resource_usage> _current_resource_states; // handle -> state
        vector<per_frame_in_flight_usage> _in_flight_usages;
        flat_unordered_map<uint64_t, write_barrier_details> _write_barriers; // handle -> details

        // Owned resources
        flat_unordered_map<uint64_t, rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>> _owned_buffers;
        flat_unordered_map<uint64_t, rhi::typed_rhi_handle<rhi::rhi_handle_type::image>> _owned_images;

        // Unowned resources
        vector<pair<uint64_t, rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>>> _external_surfaces;
        flat_unordered_map<uint64_t, rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>>
            _all_buffers; // External + Owned
        flat_unordered_map<uint64_t, rhi::typed_rhi_handle<rhi::rhi_handle_type::image>>
            _all_images; // External + Owned

        struct per_frame_fences
        {
            flat_unordered_map<work_type, execution_fence> frame_complete_fence;
        };

        vector<per_frame_fences> _per_frame_fences;

        // Queue Timelines
        struct timeline_sem
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore> sem;
            uint64_t value = 0;
        };

        flat_unordered_map<work_type, vector<timeline_sem>> _queue_timelines;

        flat_unordered_map<uint64_t, rhi::typed_rhi_handle<rhi::rhi_handle_type::image>> _current_swapchain_images;

        size_t _current_frame = 0;

        void _construct_owned_resources();
        void _destroy_owned_resources();

        using acquired_swapchains = vector<pair<rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>,
                                                rhi::swapchain_image_acquire_info_result>>;

        acquired_swapchains _acquire_swapchain_images();
        void _wait_for_swapchain_acquire(const acquired_swapchains& acquired);
        void _execute_plan(const acquired_swapchains& acquired);
        void _present_swapchain_images(const acquired_swapchains& acquired);

        optional<const scheduled_resource&> _find_resource(const base_graph_resource_handle& handle) const;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_frame_graph_hpp
