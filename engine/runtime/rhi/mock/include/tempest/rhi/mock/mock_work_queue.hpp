#ifndef tempest_rhi_mock_mock_work_queue_hpp
#define tempest_rhi_mock_mock_work_queue_hpp

#include <tempest/api.hpp>
#include <tempest/rhi/mock/mock_commands.hpp>

namespace tempest::rhi::mock
{
    class TEMPEST_API mock_work_queue final : public rhi::work_queue
    {
      public:
        mock_work_queue() = default;
        mock_work_queue(const mock_work_queue&) = delete;
        mock_work_queue(mock_work_queue&&) = delete;

        ~mock_work_queue() override = default;

        mock_work_queue& operator=(const mock_work_queue&) = delete;
        mock_work_queue& operator=(mock_work_queue&&) = delete;

        size_t get_history_count() const noexcept
        {
            return _history.size();
        }

        span<const mock_command> get_history(size_t start_index = 0) const noexcept
        {
            if (start_index >= _history.size())
            {
                return {};
            }
            return span<const mock_command>(_history.data() + start_index, _history.size() - start_index);
        }

        typed_rhi_handle<rhi_handle_type::command_list> get_next_command_list() noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::command_list>{_next_handle++, 0};
        }

        bool submit(span<const submit_info> infos,
                    typed_rhi_handle<rhi_handle_type::fence> fence =
                        typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept override
        {
            _history.push_back(submit_cmd{vector<submit_info>(infos.begin(), infos.end()), fence});
            return true;
        }

        vector<present_result> present(const present_info& info) noexcept override
        {
            vector<present_result> results;
            results.resize(info.swapchain_images.size(), present_result::success);
            return results;
        }

        void begin_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                bool one_time_submit) noexcept override
        {
            _history.push_back(begin_command_list_cmd{command_list, one_time_submit});
        }

        void end_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept override
        {
            _history.push_back(end_command_list_cmd{command_list});
        }

        void transition_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                              span<const image_barrier> image_barriers) noexcept override
        {
            _history.push_back(transition_image_cmd{
                command_list, vector<image_barrier>(image_barriers.begin(), image_barriers.end())});
        }

        void clear_color_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                               typed_rhi_handle<rhi_handle_type::image> image, image_layout layout, float red,
                               float green, float blue, float alpha) noexcept override
        {
            _history.push_back(clear_color_image_cmd{
                .command_list = command_list,
                .image = image,
                .layout = layout,
                .r = red,
                .g = green,
                .b = blue,
                .a = alpha,
            });
        }

        void blit(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::image> src, image_layout src_layout, uint32_t src_mip,
                  typed_rhi_handle<rhi_handle_type::image> dst, image_layout dst_layout,
                  uint32_t dst_mip) noexcept override
        {
            _history.push_back(blit_cmd{command_list, src, src_layout, src_mip, dst, dst_layout, dst_mip});
        }

        void generate_mip_chain(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                typed_rhi_handle<rhi_handle_type::image> img, image_layout current_layout,
                                uint32_t base_mip = 0, uint32_t mip_count = numeric_limits<uint32_t>::max()) override
        {
            _history.push_back(generate_mip_chain_cmd{command_list, img, current_layout, base_mip, mip_count});
        }

        void copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::buffer> src, typed_rhi_handle<rhi_handle_type::buffer> dst,
                  size_t src_offset = 0, size_t dst_offset = 0,
                  size_t byte_count = numeric_limits<size_t>::max()) noexcept override
        {
            _history.push_back(copy_buffer_cmd{command_list, src, dst, src_offset, dst_offset, byte_count});
        }

        void fill(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::buffer> handle, size_t offset, size_t size,
                  uint32_t data) noexcept override
        {
            _history.push_back(fill_buffer_cmd{command_list, handle, offset, size, data});
        }

        void copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::buffer> src, typed_rhi_handle<rhi_handle_type::image> dst,
                  image_layout layout, size_t src_offset = 0, uint32_t dst_mip = 0) noexcept override
        {
            _history.push_back(copy_buffer_to_image_cmd{command_list, src, dst, layout, src_offset, dst_mip});
        }

        void copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::image> src, image_layout src_layout,
                  typed_rhi_handle<rhi_handle_type::buffer> dst, size_t dst_offset = 0,
                  uint32_t src_mip = 0) noexcept override
        {
            _history.push_back(copy_image_to_buffer_cmd{command_list, src, src_layout, dst, dst_offset, src_mip});
        }

        void pipeline_barriers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                               span<const image_barrier> img_barriers,
                               span<const buffer_barrier> buf_barriers) noexcept override
        {
            _history.push_back(pipeline_barriers_cmd{command_list,
                                                     vector<image_barrier>(img_barriers.begin(), img_barriers.end()),
                                                     vector<buffer_barrier>(buf_barriers.begin(), buf_barriers.end())});
        }

        void begin_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                             const render_pass_info& render_pass_info) noexcept override
        {
            _history.push_back(begin_rendering_cmd{command_list, render_pass_info});
        }

        void end_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept override
        {
            _history.push_back(end_rendering_cmd{command_list});
        }

        void bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::graphics_pipeline> pipeline) noexcept override
        {
            _history.push_back(bind_graphics_pipeline_cmd{command_list, pipeline});
        }

        void draw(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::buffer> indirect_buffer, uint32_t offset, uint32_t draw_count,
                  uint32_t stride) noexcept override
        {
            _history.push_back(draw_indirect_cmd{command_list, indirect_buffer, offset, draw_count, stride});
        }

        void draw(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t vertex_count,
                  uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) noexcept override
        {
            _history.push_back(draw_cmd{command_list, vertex_count, instance_count, first_vertex, first_instance});
        }

        void draw(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t index_count,
                  uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
                  uint32_t first_instance) noexcept override
        {
            _history.push_back(draw_indexed_cmd{command_list, index_count, instance_count, first_index, vertex_offset,
                                                first_instance});
        }

        void bind_index_buffer(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                               typed_rhi_handle<rhi_handle_type::buffer> buffer, uint32_t offset,
                               rhi::index_format index_type) noexcept override
        {
            _history.push_back(bind_index_buffer_cmd{command_list, buffer, offset, index_type});
        }

        void bind_vertex_buffers(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t first_binding,
                                 span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers,
                                 span<const size_t> offsets) noexcept override
        {
            _history.push_back(bind_vertex_buffers_cmd{
                command_list, first_binding,
                vector<typed_rhi_handle<rhi_handle_type::buffer>>(buffers.begin(), buffers.end()),
                vector<size_t>(offsets.begin(), offsets.end())});
        }

        void set_scissor_region(typed_rhi_handle<rhi_handle_type::command_list> command_list, int32_t xpos,
                                int32_t ypos, uint32_t width, uint32_t height,
                                uint32_t region_index = 0) noexcept override
        {
            _history.push_back(set_scissor_region_cmd{
                .command_list = command_list,
                .x = xpos,
                .y = ypos,
                .width = width,
                .height = height,
                .region_index = region_index,
            });
        }

        void set_viewport(typed_rhi_handle<rhi_handle_type::command_list> command_list, float xpos, float ypos,
                          float width, float height, float min_depth, float max_depth, uint32_t viewport_index = 0,
                          bool flipped = false) noexcept override
        {
            _history.push_back(set_viewport_cmd{
                .command_list = command_list,
                .x = xpos,
                .y = ypos,
                .width = width,
                .height = height,
                .min_depth = min_depth,
                .max_depth = max_depth,
                .viewport_index = viewport_index,
                .flipped = flipped,
            });
        }

        void set_cull_mode(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                           enum_mask<cull_mode> cull) noexcept override
        {
            _history.push_back(set_cull_mode_cmd{command_list, cull});
        }

        void bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::compute_pipeline> pipeline) noexcept override
        {
            _history.push_back(bind_compute_pipeline_cmd{command_list, pipeline});
        }

        void dispatch(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t xpos, uint32_t ypos,
                      uint32_t zpos) noexcept override
        {
            _history.push_back(dispatch_cmd{
                .command_list = command_list,
                .x = xpos,
                .y = ypos,
                .z = zpos,
            });
        }

        void bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout, bind_point point,
                  uint32_t first_set_index, span<const typed_rhi_handle<rhi_handle_type::descriptor_set>> sets,
                  span<const uint32_t> dynamic_offsets = {}) noexcept override
        {
            _history.push_back(bind_descriptor_sets_cmd{
                command_list, pipeline_layout, point, first_set_index,
                vector<typed_rhi_handle<rhi_handle_type::descriptor_set>>(sets.begin(), sets.end()),
                vector<uint32_t>(dynamic_offsets.begin(), dynamic_offsets.end())});
        }

        void push_constants(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                            typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                            enum_mask<rhi::shader_stage> stages, uint32_t offset,
                            span<const byte> values) noexcept override
        {
            _history.push_back(push_constants_cmd{command_list, pipeline_layout, stages, offset,
                                                  vector<byte>(values.begin(), values.end())});
        }

        void push_descriptors(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                              typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout, bind_point point,
                              uint32_t set_index, span<const buffer_binding_descriptor> buffers,
                              span<const image_binding_descriptor> images,
                              span<const sampler_binding_descriptor> samplers) noexcept override
        {
            _history.push_back(
                push_descriptors_cmd{command_list, pipeline_layout, point, set_index,
                                     vector<buffer_binding_descriptor>(buffers.begin(), buffers.end()),
                                     vector<image_binding_descriptor>(images.begin(), images.end()),
                                     vector<sampler_binding_descriptor>(samplers.begin(), samplers.end())});
        }

        void bind_descriptor_buffers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                     typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                                     bind_point point, uint32_t first_set_index,
                                     span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers,
                                     span<const uint64_t> offsets) noexcept override
        {
            _history.push_back(bind_descriptor_buffers_cmd{
                command_list, pipeline_layout, point, first_set_index,
                vector<typed_rhi_handle<rhi_handle_type::buffer>>(buffers.begin(), buffers.end()),
                vector<uint64_t>(offsets.begin(), offsets.end())});
        }

        void reset(uint64_t frame_in_flight) override
        {
            _history.push_back(reset_cmd{frame_in_flight});
        }

        void begin_debug_region(typed_rhi_handle<rhi_handle_type::command_list> command_list, string_view name) override
        {
            _history.push_back(begin_debug_region_cmd{command_list, string(name)});
        }

        void end_debug_region(typed_rhi_handle<rhi_handle_type::command_list> command_list) override
        {
            _history.push_back(end_debug_region_cmd{command_list});
        }

        void set_debug_marker(typed_rhi_handle<rhi_handle_type::command_list> command_list, string_view name) override
        {
            _history.push_back(set_debug_marker_cmd{command_list, string(name)});
        }

      private:
        vector<mock_command> _history;
        uint32_t _next_handle = 1;
    };
} // namespace tempest::rhi::mock

#endif