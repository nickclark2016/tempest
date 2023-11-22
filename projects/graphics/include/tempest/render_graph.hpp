#ifndef tempest_graphics_render_graph_hpp
#define tempest_graphics_render_graph_hpp

#include "render_device.hpp"
#include "types.hpp"

#include <tempest/memory.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>

namespace tempest::graphics
{
    struct image_resource_state
    {
        resource_access_type type;

        image_resource_handle img;
        image_resource_usage usage;
        pipeline_stage first_access;
        pipeline_stage last_access;

        std::uint32_t previous_pass_id;
        std::uint32_t next_pass_id;
        std::uint32_t execution_queue_id;
    };

    struct buffer_resource_state
    {
        resource_access_type type;

        buffer_resource_handle buf;
        buffer_resource_usage usage;
        pipeline_stage first_access;
        pipeline_stage last_access;

        std::uint32_t previous_pass_id;
        std::uint32_t next_pass_id;
        std::uint32_t execution_queue_id;
    };

    struct swapchain_resource_state
    {
        resource_access_type type;

        swapchain_resource_handle swap;
        image_resource_usage usage;
        pipeline_stage first_access;
        pipeline_stage last_access;
    };

    class render_graph_resource_library
    {
      public:
        virtual ~render_graph_resource_library() = default;

        virtual image_resource_handle find_texture(std::string_view name) = 0;
        virtual image_resource_handle load(const image_desc& desc) = 0;
        virtual void add_image_usage(image_resource_handle handle, image_resource_usage usage) = 0;

        virtual buffer_resource_handle find_buffer(std::string_view name) = 0;
        virtual buffer_resource_handle load(const buffer_desc& desc) = 0;
        virtual void add_buffer_usage(buffer_resource_handle handle, buffer_resource_usage usage) = 0;

        virtual bool compile() = 0;
    };

    class render_graph_compiler;

    class graph_pass_builder
    {
        friend class render_graph_compiler;

        graph_pass_builder(render_graph_resource_library& lib, std::string_view name, queue_operation_type type);

      public:
        graph_pass_builder& add_color_attachment(image_resource_handle handle, resource_access_type access,
                                                 pipeline_stage first_access = pipeline_stage::INFER,
                                                 pipeline_stage last_access = pipeline_stage::INFER);
        graph_pass_builder& add_depth_attachment(image_resource_handle handle, resource_access_type access,
                                                 pipeline_stage first_access = pipeline_stage::INFER,
                                                 pipeline_stage last_access = pipeline_stage::INFER);
        graph_pass_builder& add_sampled_image(image_resource_handle handle,
                                              pipeline_stage first_read = pipeline_stage::INFER,
                                              pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_blit_target(image_resource_handle handle,
                                            pipeline_stage first_write = pipeline_stage::INFER,
                                            pipeline_stage last_write = pipeline_stage::INFER);
        graph_pass_builder& add_external_blit_target(swapchain_resource_handle,
                                                     pipeline_stage first_write = pipeline_stage::INFER,
                                                     pipeline_stage last_write = pipeline_stage::INFER);
        graph_pass_builder& add_blit_source(image_resource_handle handle,
                                            pipeline_stage first_read = pipeline_stage::INFER,
                                            pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_storage_image(image_resource_handle handle, resource_access_type access,
                                              pipeline_stage first_access = pipeline_stage::INFER,
                                              pipeline_stage last_access = pipeline_stage::INFER);

        graph_pass_builder& add_structured_buffer(buffer_resource_handle handle, resource_access_type access,
                                                  pipeline_stage first_access = pipeline_stage::INFER,
                                                  pipeline_stage last_access = pipeline_stage::INFER);
        graph_pass_builder& add_vertex_buffer(buffer_resource_handle handle,
                                              pipeline_stage first_read = pipeline_stage::INFER,
                                              pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_index_buffer(buffer_resource_handle handle,
                                             pipeline_stage first_read = pipeline_stage::INFER,
                                             pipeline_stage last_read = pipeline_stage::INFER);
        graph_pass_builder& add_constant_buffer(buffer_resource_handle handle,
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

        graph_pass_builder& depends_on(graph_pass_handle src);

        graph_pass_builder& on_execute(std::function<void(command_list&)> commands);
        graph_pass_builder& should_execute(std::function<bool()> fn);

        std::string_view name() const noexcept;

        inline graph_pass_handle handle() const noexcept
        {
            return _self;
        }

        inline std::span<const graph_pass_handle> depends_on() const noexcept
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

        std::span<const image_resource_state> image_usage() const noexcept
        {
            return _image_states;
        }

        std::span<const buffer_resource_state> buffer_usage() const noexcept
        {
            return _buffer_states;
        }

        std::span<const swapchain_resource_state> external_swapchain_usage() const noexcept
        {
            return _external_swapchain_states;
        }

        queue_operation_type operation_type() const noexcept
        {
            return _op_type;
        }

      private:
        render_graph_resource_library& _resource_lib;
        queue_operation_type _op_type;
        std::function<void(command_list&)> _commands;
        std::function<bool()> _should_execute = []() { return true; };
        std::vector<image_resource_state> _image_states;
        std::vector<buffer_resource_state> _buffer_states;
        std::vector<swapchain_resource_state> _external_swapchain_states;
        std::vector<graph_pass_handle> _depends_on;
        graph_pass_handle _self;
        std::string _name;

        void _infer();
    };

    class render_graph
    {
      public:
        virtual ~render_graph() = default;
        virtual void execute() = 0;
    };

    class render_graph_compiler
    {
      public:
        virtual ~render_graph_compiler() = default;

        image_resource_handle create_image(image_desc desc);
        buffer_resource_handle create_buffer(buffer_desc desc);
        graph_pass_handle add_graph_pass(std::string_view name, queue_operation_type type,
                                         std::function<void(graph_pass_builder&)> build);

        virtual std::unique_ptr<render_graph> compile() && = 0;

        static std::unique_ptr<render_graph_compiler> create_compiler(core::allocator* alloc, render_device* device);

      protected:
        render_device* _device;
        core::allocator* _alloc;
        std::vector<graph_pass_builder> _builders;
        std::unique_ptr<render_graph_resource_library> _resource_lib;

        explicit render_graph_compiler(core::allocator* alloc, render_device* device);
    };

    class dependency_graph
    {
      public:
        void add_graph_pass(std::uint64_t pass_id);
        void add_graph_dependency(std::uint64_t src_pass, std::uint64_t dst_pass);
        std::vector<std::uint64_t> toposort() const noexcept;

      private:
        std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> _adjacency_list;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_graph_hpp