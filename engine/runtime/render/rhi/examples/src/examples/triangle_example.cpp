#include "triangle_example.hpp"

#include <cstring>
#include <tempest/array.hpp>
#include <tempest/span.hpp>

namespace shaders::triangle
{
    namespace vs
    {
#include <triangle.vert.h>
    } // namespace vs

    namespace fs
    {
#include <triangle.frag.h>
    } // namespace fs
} // namespace shaders::triangle

namespace tempest::rhi::examples
{
    namespace
    {
        struct vec2
        {
            float x;
            float y;
        };

        struct vec3
        {
            float r;
            float g;
            float b;
        };

        struct triangle_push_constants
        {
            uint64_t positions_address;
            uint64_t colors_address;
        };

        constexpr auto positions = array<vec2, 3>{
            vec2{0.0f, -0.6f},
            vec2{0.6f, 0.6f},
            vec2{-0.6f, 0.6f},
        };

        // Linear RGB primary colors for sRGB correct rendering
        constexpr auto colors = array<vec3, 3>{
            vec3{1.0f, 0.0f, 0.0f}, // Red
            vec3{0.0f, 1.0f, 0.0f}, // Green
            vec3{0.0f, 0.0f, 1.0f}, // Blue
        };

        constexpr auto indices = array<uint16_t, 3>{0, 1, 2};
    } // namespace

    auto triangle_example::init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool
    {
        // 1. Create storage buffer for vertex positions
        auto pos_desc = buffer_desc{
            .size = sizeof(positions),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "triangle_positions_buffer",
        };
        _positions_buffer = dev.create_buffer(pos_desc);
        if (_positions_buffer.handle == 0 || _positions_buffer.gpu_address == 0)
        {
            return false;
        }
        std::memcpy(_positions_buffer.cpu_address, positions.data(), sizeof(positions));

        // 2. Create storage buffer for vertex colors
        auto color_desc = buffer_desc{
            .size = sizeof(colors),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "triangle_colors_buffer",
        };
        _colors_buffer = dev.create_buffer(color_desc);
        if (_colors_buffer.handle == 0 || _colors_buffer.gpu_address == 0)
        {
            return false;
        }
        std::memcpy(_colors_buffer.cpu_address, colors.data(), sizeof(colors));

        // 3. Create index buffer
        auto index_desc = buffer_desc{
            .size = sizeof(indices),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::index_buffer,
            .name = "triangle_index_buffer",
        };
        _index_buffer = dev.create_buffer(index_desc);
        if (_index_buffer.handle == 0)
        {
            return false;
        }
        std::memcpy(_index_buffer.cpu_address, indices.data(), sizeof(indices));

        // 4. Create graphics pipeline
        return create_pipeline(dev, surface_format);
    }

    auto triangle_example::create_pipeline(rhi::device& dev, rhi::render_surface_format surface_format) -> bool
    {
        if (_pipeline.handle != 0)
        {
            dev.destroy_graphics_pipeline(_pipeline);
            _pipeline = {};
        }

        _current_format = surface_format;

        auto vs_desc = shader_module_desc{
            .stage = shader_stage::vertex,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::triangle::vs::triangle_vs_spv),
                                        sizeof(shaders::triangle::vs::triangle_vs_spv)},
            .entry_point = "VSMain",
        };

        auto fs_desc = shader_module_desc{
            .stage = shader_stage::fragment,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::triangle::fs::triangle_fs_spv),
                                        sizeof(shaders::triangle::fs::triangle_fs_spv)},
            .entry_point = "FSMain",
        };

        auto stages = array{vs_desc, fs_desc};
        auto color_format = to_data_format(surface_format);
        auto color_formats = array{color_format};

        auto pipe_desc = graphics_pipeline_desc{
            .shader_modules = span<const shader_module_desc>{stages.data(), stages.size()},
            .color_attachment_formats = span<const data_format>{color_formats.data(), color_formats.size()},
            .primitive_topology = primitive_topology::triangle_list,
            .rasterization_state =
                {
                    .polygon_mode = polygon_mode::fill,
                    .cull_mode = cull_mode::none,
                    .front_face = vertex_winding_order::counter_clockwise,
                },
            .depth_stencil_state =
                {
                    .depth_test_enable = false,
                    .depth_write_enable = false,
                },
        };

        _pipeline = dev.create_graphics_pipeline(pipe_desc);
        return _pipeline.handle != 0;
    }

    auto triangle_example::render(const frame_render_info& info) -> void
    {
        auto& graphics_port = info.dev.get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
        cmd.begin();

        const auto init_barrier = texture_barrier{
            .texture = info.swapchain_texture,
            .src =
                {
                    .stages = pipeline_stage::attachment_output,
                    .access = resource_access::none,
                    .layout = image_layout::undefined,
                },
            .dst =
                {
                    .stages = pipeline_stage::attachment_output,
                    .access = resource_access::write,
                    .layout = image_layout::general,
                },
        };
        cmd.pipeline_barrier(span<const texture_barrier>{&init_barrier, 1}, {});

        const auto color_att = color_attachment{
            .view = info.swapchain_view,
            .load_op = load_op::clear,
            .store_op = store_op::store,
            .clear_value =
                clear_color_value{
                    .r = 0.05F,
                    .g = 0.05F,
                    .b = 0.05F,
                    .a = 1.0F,
                },
        };
        cmd.begin_render_pass(span<const color_attachment>{&color_att, 1}, nullopt, info.width, info.height);
        cmd.set_viewport(0.0F, 0.0F, static_cast<float>(info.width), static_cast<float>(info.height), 0.0F, 1.0F);
        cmd.set_scissor(0, 0, info.width, info.height);

        cmd.bind_pipeline(_pipeline);
        cmd.bind_index_buffer(_index_buffer, index_type::uint16, 0);

        const auto constants = triangle_push_constants{
            .positions_address = _positions_buffer.gpu_address,
            .colors_address = _colors_buffer.gpu_address,
        };
        cmd.push_constants(shader_stage::vertex, 0,
                           span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

        cmd.draw_indexed(3, 1, 0, 0, 0);
        cmd.end_render_pass();

        const auto present_barrier = texture_barrier{
            .texture = info.swapchain_texture,
            .src =
                {
                    .stages = pipeline_stage::attachment_output,
                    .access = resource_access::write,
                    .layout = image_layout::general,
                },
            .dst =
                {
                    .stages = pipeline_stage::bottom_of_pipe,
                    .access = resource_access::none,
                    .layout = image_layout::present,
                },
        };
        cmd.pipeline_barrier(span<const texture_barrier>{&present_barrier, 1}, {});
        cmd.end();

        const auto wait_point = device_sync_point{
            .semaphore = info.acquire_semaphore,
            .value = 0,
            .stages = pipeline_stage::attachment_output,
        };
        const auto signal_render = device_sync_point{
            .semaphore = info.render_semaphore,
            .value = 0,
            .stages = pipeline_stage::bottom_of_pipe,
        };
        const auto signal_timeline = device_sync_point{
            .semaphore = info.timeline_semaphore,
            .value = info.timeline_value,
            .stages = pipeline_stage::bottom_of_pipe,
        };
        const auto signal_syncs = array{signal_render, signal_timeline};

        const auto* cmd_ptr = &cmd;
        [[maybe_unused]] const auto submit_res = graphics_port.submit(
            span<const command_list*>{&cmd_ptr, 1}, span<const device_sync_point>{&wait_point, 1},
            span<const device_sync_point>{signal_syncs.data(), signal_syncs.size()});
    }

    auto triangle_example::on_resize(rhi::device& dev, rhi::render_surface_format surface_format,
                                     [[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height) -> void
    {
        if (surface_format != _current_format)
        {
            create_pipeline(dev, surface_format);
        }
    }

    auto triangle_example::shutdown(rhi::device& dev) -> void
    {
        if (_pipeline.handle != 0)
        {
            dev.destroy_graphics_pipeline(_pipeline);
            _pipeline = {};
        }

        if (_index_buffer.handle != 0)
        {
            dev.destroy_buffer(_index_buffer);
            _index_buffer = {};
        }

        if (_colors_buffer.handle != 0)
        {
            dev.destroy_buffer(_colors_buffer);
            _colors_buffer = {};
        }

        if (_positions_buffer.handle != 0)
        {
            dev.destroy_buffer(_positions_buffer);
            _positions_buffer = {};
        }
    }
} // namespace tempest::rhi::examples
