#ifndef tempest_rhi_mock_mock_commands_hpp
#define tempest_rhi_mock_mock_commands_hpp

#include <tempest/rhi.hpp>

#include <tempest/string.hpp>
#include <tempest/vector.hpp>
#include <tempest/variant.hpp>

namespace tempest::rhi::mock
{
    struct begin_command_list_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        bool one_time_submit;
    };

    struct end_command_list_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
    };

    struct submit_cmd
    {
        vector<work_queue::submit_info> infos;
        typed_rhi_handle<rhi_handle_type::fence> fence;
    };

    struct transition_image_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        vector<work_queue::image_barrier> image_barriers;
    };

    struct clear_color_image_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::image> image;
        image_layout layout;
        float r;
        float g;
        float b;
        float a;
    };

    struct blit_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::image> src;
        image_layout src_layout;
        uint32_t src_mip;
        typed_rhi_handle<rhi_handle_type::image> dst;
        image_layout dst_layout;
        uint32_t dst_mip;
    };

    struct generate_mip_chain_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::image> img;
        image_layout current_layout;
        uint32_t base_mip;
        uint32_t mip_count;
    };

    struct copy_buffer_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::buffer> src;
        typed_rhi_handle<rhi_handle_type::buffer> dst;
        size_t src_offset;
        size_t dst_offset;
        size_t byte_count;
    };

    struct fill_buffer_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::buffer> handle;
        size_t offset;
        size_t size;
        uint32_t data;
    };

    struct copy_buffer_to_image_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::buffer> src;
        typed_rhi_handle<rhi_handle_type::image> dst;
        image_layout layout;
        size_t src_offset;
        uint32_t dst_mip;
    };

    struct copy_image_to_buffer_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::image> src;
        image_layout src_layout;
        typed_rhi_handle<rhi_handle_type::buffer> dst;
        size_t dst_offset;
        uint32_t src_mip;
    };

    struct pipeline_barriers_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        vector<work_queue::image_barrier> img_barriers;
        vector<work_queue::buffer_barrier> buf_barriers;
    };

    struct begin_rendering_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        work_queue::render_pass_info render_pass_info;
    };

    struct end_rendering_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
    };

    struct bind_graphics_pipeline_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::graphics_pipeline> pipeline;
    };

    struct draw_indirect_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::buffer> indirect_buffer;
        uint32_t offset;
        uint32_t draw_count;
        uint32_t stride;
    };

    struct draw_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        uint32_t vertex_count;
        uint32_t instance_count;
        uint32_t first_vertex;
        uint32_t first_instance;
    };

    struct draw_indexed_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        uint32_t index_count;
        uint32_t instance_count;
        uint32_t first_index;
        int32_t vertex_offset;
        uint32_t first_instance;
    };

    struct bind_index_buffer_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::buffer> buffer;
        uint32_t offset;
        rhi::index_format index_type;
    };

    struct bind_vertex_buffers_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        uint32_t first_binding;
        vector<typed_rhi_handle<rhi_handle_type::buffer>> buffers;
        vector<size_t> offsets;
    };

    struct set_scissor_region_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;
        uint32_t region_index;
    };

    struct set_viewport_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        float x;
        float y;
        float width;
        float height;
        float min_depth;
        float max_depth;
        uint32_t viewport_index;
        bool flipped;
    };

    struct set_cull_mode_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        enum_mask<cull_mode> cull;
    };

    struct bind_compute_pipeline_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::compute_pipeline> pipeline;
    };

    struct dispatch_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        uint32_t x;
        uint32_t y;
        uint32_t z;
    };

    struct bind_descriptor_sets_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout;
        bind_point point;
        uint32_t first_set_index;
        vector<typed_rhi_handle<rhi_handle_type::descriptor_set>> sets;
        vector<uint32_t> dynamic_offsets;
    };

    struct push_constants_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout;
        enum_mask<rhi::shader_stage> stages;
        uint32_t offset;
        vector<byte> values;
    };

    struct push_descriptors_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout;
        bind_point point;
        uint32_t set_index;
        vector<buffer_binding_descriptor> buffers;
        vector<image_binding_descriptor> images;
        vector<sampler_binding_descriptor> samplers;
    };

    struct bind_descriptor_buffers_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout;
        bind_point point;
        uint32_t first_set_index;
        vector<typed_rhi_handle<rhi_handle_type::buffer>> buffers;
        vector<uint64_t> offsets;
    };

    struct reset_cmd
    {
        uint64_t frame_in_flight;
    };

    struct begin_debug_region_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        string name;
    };

    struct end_debug_region_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
    };

    struct set_debug_marker_cmd
    {
        typed_rhi_handle<rhi_handle_type::command_list> command_list;
        string name;
    };

    using mock_command = variant<
        begin_command_list_cmd,
        end_command_list_cmd,
        submit_cmd,
        transition_image_cmd,
        clear_color_image_cmd,
        blit_cmd,
        generate_mip_chain_cmd,
        copy_buffer_cmd,
        fill_buffer_cmd,
        copy_buffer_to_image_cmd,
        copy_image_to_buffer_cmd,
        pipeline_barriers_cmd,
        begin_rendering_cmd,
        end_rendering_cmd,
        bind_graphics_pipeline_cmd,
        draw_indirect_cmd,
        draw_cmd,
        draw_indexed_cmd,
        bind_index_buffer_cmd,
        bind_vertex_buffers_cmd,
        set_scissor_region_cmd,
        set_viewport_cmd,
        set_cull_mode_cmd,
        bind_compute_pipeline_cmd,
        dispatch_cmd,
        bind_descriptor_sets_cmd,
        push_constants_cmd,
        push_descriptors_cmd,
        bind_descriptor_buffers_cmd,
        reset_cmd,
        begin_debug_region_cmd,
        end_debug_region_cmd,
        set_debug_marker_cmd
    >;
}

#endif