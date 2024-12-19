#ifndef tempest_graphics_render_graph_hpp
#define tempest_graphics_render_graph_hpp

#include "render_device.hpp"
#include "types.hpp"

#include <tempest/flat_unordered_map.hpp>
#include <tempest/functional.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/vec4.hpp>
#include <tempest/vector.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace tempest::graphics
{
    struct image_resource_state
    {
        resource_access_type type;

        vector<image_resource_handle> handles;
        image_resource_usage usage;
        pipeline_stage first_access;
        pipeline_stage last_access;
        load_op load;
        store_op store;
        math::vec4<float> clear_color;
        float clear_depth;

        uint32_t set;
        uint32_t binding;
    };

    struct buffer_resource_state
    {
        resource_access_type type;

        buffer_resource_handle buf;
        buffer_resource_usage usage;
        pipeline_stage first_access;
        pipeline_stage last_access;
        bool per_frame_memory{false};

        uint32_t set;
        uint32_t binding;
    };

    struct swapchain_resource_state
    {
        resource_access_type type;

        swapchain_resource_handle swap;
        image_resource_usage usage;
        pipeline_stage first_access;
        pipeline_stage last_access;
        load_op load;
        store_op store;
    };

    struct external_image_resource_state
    {
        resource_access_type type;
        image_resource_usage usage;
        vector<image_resource_handle> images;
        pipeline_stage stages;

        uint32_t count;
        uint32_t set;
        uint32_t binding;
    };

    struct resolve_image_state
    {
        image_resource_handle src;
        image_resource_handle dst;

        pipeline_stage first_access;
        pipeline_stage last_access;
    };

    struct external_sampler_resource_state
    {
        vector<sampler_resource_handle> samplers;
        pipeline_stage stages;

        uint32_t set;
        uint32_t binding;
    };

    class render_graph_resource_library
    {
      public:
        virtual ~render_graph_resource_library() = default;

        virtual image_resource_handle find_texture(string_view name) = 0;
        virtual image_resource_handle load(const image_desc& desc) = 0;
        virtual void add_image_usage(image_resource_handle handle, image_resource_usage usage) = 0;

        virtual buffer_resource_handle find_buffer(string_view name) = 0;
        virtual buffer_resource_handle load(const buffer_desc& desc) = 0;
        virtual void add_buffer_usage(buffer_resource_handle handle, buffer_resource_usage usage) = 0;

        virtual bool compile() = 0;
    };

    class render_graph_compiler;

    class graph_pass_builder
    {
        friend class render_graph_compiler;

        graph_pass_builder(render_graph_resource_library& lib, string_view name, queue_operation_type type);

      public:
        graph_pass_builder& add_color_attachment(image_resource_handle handle, resource_access_type access,
                                                 load_op load = load_op::LOAD, store_op store = store_op::STORE,
                                                 math::vec4<float> clear_color = math::vec4(0.0f),
                                                 pipeline_stage first_access = pipeline_stage::INFER,
                                                 pipeline_stage last_access = pipeline_stage::INFER);
        graph_pass_builder& add_external_color_attachment(swapchain_resource_handle, resource_access_type access,
                                                          load_op load = load_op::LOAD,
                                                          store_op store = store_op::STORE,
                                                          pipeline_stage first_write = pipeline_stage::INFER,
                                                          pipeline_stage last_write = pipeline_stage::INFER);
        graph_pass_builder& add_depth_attachment(image_resource_handle handle, resource_access_type access,
                                                 load_op load = load_op::LOAD, store_op store = store_op::STORE,
                                                 float clear_depth = 0.0f,
                                                 pipeline_stage first_access = pipeline_stage::INFER,
                                                 pipeline_stage last_access = pipeline_stage::INFER);
        graph_pass_builder& add_sampled_image(image_resource_handle handle, uint32_t set, uint32_t binding,
                                              pipeline_stage first_read = pipeline_stage::INFER,
                                              pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_external_sampled_image(image_resource_handle handle, uint32_t set,
                                                       uint32_t binding, pipeline_stage usage);
        graph_pass_builder& add_external_sampled_images(span<image_resource_handle> handles, uint32_t set,
                                                        uint32_t binding, pipeline_stage usage);
        graph_pass_builder& add_external_sampled_images(uint32_t count, uint32_t set, uint32_t binding,
                                                        pipeline_stage usage);
        graph_pass_builder& add_external_storage_image(image_resource_handle handle, resource_access_type access,
                                                       uint32_t set, uint32_t binding, pipeline_stage usage);
        graph_pass_builder& add_blit_target(image_resource_handle handle,
                                            pipeline_stage first_write = pipeline_stage::INFER,
                                            pipeline_stage last_write = pipeline_stage::INFER);
        graph_pass_builder& add_transfer_target(image_resource_handle handle,
                                                pipeline_stage first_write = pipeline_stage::INFER,
                                                pipeline_stage last_write = pipeline_stage::INFER)
        {
            return add_blit_target(handle, first_write, last_write);
        }
        graph_pass_builder& add_external_blit_target(swapchain_resource_handle,
                                                     pipeline_stage first_write = pipeline_stage::INFER,
                                                     pipeline_stage last_write = pipeline_stage::INFER);
        graph_pass_builder& add_blit_source(image_resource_handle handle,
                                            pipeline_stage first_read = pipeline_stage::INFER,
                                            pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_storage_image(image_resource_handle handle, resource_access_type access,
                                              uint32_t set, uint32_t binding,
                                              pipeline_stage first_access = pipeline_stage::INFER,
                                              pipeline_stage last_access = pipeline_stage::INFER);
        graph_pass_builder& add_storage_image(span<image_resource_handle> handle, resource_access_type access,
                                              uint32_t set, uint32_t binding,
                                              pipeline_stage first_access = pipeline_stage::INFER,
                                              pipeline_stage last_access = pipeline_stage::INFER);

        graph_pass_builder& add_structured_buffer(buffer_resource_handle handle, resource_access_type access,
                                                  uint32_t set, uint32_t binding,
                                                  pipeline_stage first_access = pipeline_stage::INFER,
                                                  pipeline_stage last_access = pipeline_stage::INFER);
        graph_pass_builder& add_vertex_buffer(buffer_resource_handle handle,
                                              pipeline_stage first_read = pipeline_stage::INFER,
                                              pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_index_buffer(buffer_resource_handle handle,
                                             pipeline_stage first_read = pipeline_stage::INFER,
                                             pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_constant_buffer(buffer_resource_handle handle, uint32_t set, uint32_t binding,
                                                pipeline_stage first_read = pipeline_stage::INFER,
                                                pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_indirect_argument_buffer(buffer_resource_handle handle,
                                                         pipeline_stage first_read = pipeline_stage::INFER,
                                                         pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_transfer_source_buffer(buffer_resource_handle handle,
                                                       pipeline_stage first_read = pipeline_stage::INFER,
                                                       pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_transfer_destination_buffer(buffer_resource_handle handle,
                                                            pipeline_stage first_write = pipeline_stage::INFER,
                                                            pipeline_stage last_write = pipeline_stage::INFER);
        graph_pass_builder& add_host_write_buffer(buffer_resource_handle handle,
                                                  pipeline_stage first_write = pipeline_stage::INFER,
                                                  pipeline_stage last_write = pipeline_stage::INFER);
        graph_pass_builder& add_sampler(sampler_resource_handle handle, uint32_t set, uint32_t binding,
                                        pipeline_stage usage);

        graph_pass_builder& depends_on(graph_pass_handle src);

        graph_pass_builder& resolve_image(image_resource_handle src, image_resource_handle dst,
                                          pipeline_stage first_access = pipeline_stage::INFER,
                                          pipeline_stage last_access = pipeline_stage::INFER);
        graph_pass_builder& allow_push_constants(uint32_t range);

        graph_pass_builder& on_execute(function<void(command_list&)> commands);
        graph_pass_builder& should_execute(function<bool()> fn);

        string_view name() const noexcept;

        graph_pass_builder& draw_imgui()
        {
            _draw_imgui = true;

            return *this;
        }

        bool should_draw_imgui() const noexcept
        {
            return _draw_imgui;
        }

        graph_pass_builder& draw_gpu_profile()
        {
            _draw_gpu_profile = true;

            return *this;
        }

        bool should_draw_gpu_profile() const noexcept
        {
            return _draw_gpu_profile;
        }

        inline graph_pass_handle handle() const noexcept
        {
            return _self;
        }

        inline span<const graph_pass_handle> depends_on() const noexcept
        {
            return _depends_on;
        }

        void execute(command_list& cmds)
        {
            _commands(cmds);
        }

        bool should_execute() const
        {
            return _should_execute();
        }

        span<const image_resource_state> image_usage() const noexcept
        {
            return _image_states;
        }

        span<const buffer_resource_state> buffer_usage() const noexcept
        {
            return _buffer_states;
        }

        span<const swapchain_resource_state> external_swapchain_usage() const noexcept
        {
            return _external_swapchain_states;
        }

        span<const external_image_resource_state> external_images() const noexcept
        {
            return _external_image_states;
        }

        span<const external_sampler_resource_state> external_samplers() const noexcept
        {
            return _sampler_states;
        }

        span<const resolve_image_state> resolve_images() const noexcept
        {
            return _resolve_images;
        }

        queue_operation_type operation_type() const noexcept
        {
            return _op_type;
        }

        uint32_t push_constant_range_size() const noexcept
        {
            return _push_constant_range;
        }

      private:
        render_graph_resource_library& _resource_lib;
        queue_operation_type _op_type;
        function<void(command_list&)> _commands;
        function<bool()> _should_execute = []() { return true; };
        vector<image_resource_state> _image_states;
        vector<buffer_resource_state> _buffer_states;
        vector<swapchain_resource_state> _external_swapchain_states;
        vector<external_image_resource_state> _external_image_states;
        vector<external_sampler_resource_state> _sampler_states;
        vector<graph_pass_handle> _depends_on;
        vector<resolve_image_state> _resolve_images;
        graph_pass_handle _self;
        uint32_t _push_constant_range{0};
        string _name;

        bool _draw_imgui{false};
        bool _draw_gpu_profile{false};

        void _infer();
    };

    class render_graph
    {
      public:
        virtual void update_external_sampled_images(graph_pass_handle pass, span<image_resource_handle> images,
                                                    uint32_t set, uint32_t binding, pipeline_stage stage) = 0;

        virtual ~render_graph() = default;
        virtual void execute() = 0;
        virtual void show_gpu_profiling() const = 0;
    };

    class render_graph_compiler
    {
      public:
        virtual ~render_graph_compiler() = default;

        image_resource_handle create_image(image_desc desc);
        buffer_resource_handle create_buffer(buffer_desc desc);
        graph_pass_handle add_graph_pass(string_view name, queue_operation_type type,
                                         function<void(graph_pass_builder&)> build);

        void enable_gpu_profiling(bool enabled = true);
        void enable_imgui(bool enabled = true);

        virtual unique_ptr<render_graph> compile() && = 0;

        static unique_ptr<render_graph_compiler> create_compiler(abstract_allocator* alloc, render_device* device);

      protected:
        render_device* _device;
        abstract_allocator* _alloc;
        vector<graph_pass_builder> _builders;
        unique_ptr<render_graph_resource_library> _resource_lib;
        bool _imgui_enabled = false;
        bool _gpu_profiling_enabled = false;

        explicit render_graph_compiler(abstract_allocator* alloc, render_device* device);
    };

    class dependency_graph
    {
      public:
        void add_graph_pass(uint64_t pass_id);
        void add_graph_dependency(uint64_t src_pass, uint64_t dst_pass);
        vector<uint64_t> toposort() const noexcept;

      private:
        flat_unordered_map<uint64_t, vector<uint64_t>> _adjacency_list;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_graph_hpp