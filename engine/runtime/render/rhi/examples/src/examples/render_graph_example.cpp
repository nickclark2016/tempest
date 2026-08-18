#include "render_graph_example.hpp"

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

        // Pass 1: Outer Primary RGB Triangle
        constexpr auto primary_positions = array<vec2, 3>{
            vec2{0.0F, -0.70F},
            vec2{0.70F, 0.60F},
            vec2{-0.70F, 0.60F},
        };

        constexpr auto primary_colors = array<vec3, 3>{
            vec3{1.0F, 0.15F, 0.15F}, // Vivid Red
            vec3{0.15F, 1.0F, 0.15F}, // Vivid Green
            vec3{0.15F, 0.40F, 1.0F}, // Vivid Blue
        };

        // Pass 2: Inverted Center Geometric Accent Triangle
        constexpr auto accent_positions = array<vec2, 3>{
            vec2{0.0F, 0.60F},
            vec2{-0.35F, -0.05F},
            vec2{0.35F, -0.05F},
        };

        constexpr auto accent_colors = array<vec3, 3>{
            vec3{1.0F, 0.85F, 0.05F}, // Gold / Yellow
            vec3{0.05F, 0.95F, 0.95F}, // Cyan
            vec3{0.95F, 0.15F, 0.90F}, // Magenta
        };

        // Pass 3: Top Apex Center Emblem Triangle
        constexpr auto emblem_positions = array<vec2, 3>{
            vec2{0.0F, -0.60F},
            vec2{0.25F, -0.15F},
            vec2{-0.25F, -0.15F},
        };

        constexpr auto emblem_colors = array<vec3, 3>{
            vec3{1.0F, 1.0F, 1.0F},     // Pure White
            vec3{0.90F, 0.85F, 1.0F},   // Light Violet
            vec3{0.85F, 0.95F, 1.0F},   // Light Cyan
        };

        constexpr auto indices = array<uint16_t, 3>{0, 1, 2};
    } // namespace

    auto render_graph_example::init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool
    {
        _device = &dev;

        // 1. Create Primary Triangle Buffers
        const auto pos_desc = buffer_desc{
            .size = sizeof(primary_positions),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "rg_primary_positions_buffer",
        };
        _positions_buffer = dev.create_buffer(pos_desc);
        if (_positions_buffer.handle == 0 || _positions_buffer.gpu_address == 0)
        {
            return false;
        }
        std::memcpy(_positions_buffer.cpu_address, primary_positions.data(), sizeof(primary_positions));

        const auto color_desc = buffer_desc{
            .size = sizeof(primary_colors),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "rg_primary_colors_buffer",
        };
        _colors_buffer = dev.create_buffer(color_desc);
        if (_colors_buffer.handle == 0 || _colors_buffer.gpu_address == 0)
        {
            return false;
        }
        std::memcpy(_colors_buffer.cpu_address, primary_colors.data(), sizeof(primary_colors));

        // 2. Create Accent Triangle Buffers
        const auto accent_pos_desc = buffer_desc{
            .size = sizeof(accent_positions),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "rg_accent_positions_buffer",
        };
        _accent_positions_buffer = dev.create_buffer(accent_pos_desc);
        if (_accent_positions_buffer.handle == 0 || _accent_positions_buffer.gpu_address == 0)
        {
            return false;
        }
        std::memcpy(_accent_positions_buffer.cpu_address, accent_positions.data(), sizeof(accent_positions));

        const auto accent_color_desc = buffer_desc{
            .size = sizeof(accent_colors),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "rg_accent_colors_buffer",
        };
        _accent_colors_buffer = dev.create_buffer(accent_color_desc);
        if (_accent_colors_buffer.handle == 0 || _accent_colors_buffer.gpu_address == 0)
        {
            return false;
        }
        std::memcpy(_accent_colors_buffer.cpu_address, accent_colors.data(), sizeof(accent_colors));

        // 3. Create Emblem Triangle Buffers
        const auto emblem_pos_desc = buffer_desc{
            .size = sizeof(emblem_positions),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "rg_emblem_positions_buffer",
        };
        _emblem_positions_buffer = dev.create_buffer(emblem_pos_desc);
        if (_emblem_positions_buffer.handle == 0 || _emblem_positions_buffer.gpu_address == 0)
        {
            return false;
        }
        std::memcpy(_emblem_positions_buffer.cpu_address, emblem_positions.data(), sizeof(emblem_positions));

        const auto emblem_color_desc = buffer_desc{
            .size = sizeof(emblem_colors),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "rg_emblem_colors_buffer",
        };
        _emblem_colors_buffer = dev.create_buffer(emblem_color_desc);
        if (_emblem_colors_buffer.handle == 0 || _emblem_colors_buffer.gpu_address == 0)
        {
            return false;
        }
        std::memcpy(_emblem_colors_buffer.cpu_address, emblem_colors.data(), sizeof(emblem_colors));

        // 4. Create Shared Index Buffer
        const auto index_desc = buffer_desc{
            .size = sizeof(indices),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::index_buffer,
            .name = "rg_triangle_index_buffer",
        };
        _index_buffer = dev.create_buffer(index_desc);
        if (_index_buffer.handle == 0)
        {
            return false;
        }
        std::memcpy(_index_buffer.cpu_address, indices.data(), sizeof(indices));

        // 5. Initialize Render Graph
        _render_graph = make_unique<render_graph::render_graph>(1920, 1080);

        // 6. Create Graphics Pipeline
        return create_pipeline(dev, surface_format);
    }

    auto render_graph_example::create_pipeline(rhi::device& dev, rhi::render_surface_format surface_format) -> bool
    {
        if (_pipeline.handle != 0)
        {
            dev.destroy_graphics_pipeline(_pipeline);
            _pipeline = {};
        }

        _current_format = surface_format;

        const auto vs_desc = shader_module_desc{
            .stage = shader_stage::vertex,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::triangle::vs::triangle_vs_spv),
                                        sizeof(shaders::triangle::vs::triangle_vs_spv)},
            .entry_point = "VSMain",
        };

        const auto fs_desc = shader_module_desc{
            .stage = shader_stage::fragment,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::triangle::fs::triangle_fs_spv),
                                        sizeof(shaders::triangle::fs::triangle_fs_spv)},
            .entry_point = "FSMain",
        };

        const auto stages = array{vs_desc, fs_desc};
        const auto color_format = to_data_format(surface_format);
        const auto color_formats = array{color_format};

        const auto pipe_desc = graphics_pipeline_desc{
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

    auto render_graph_example::render(const frame_render_info& info) -> void
    {
        if (!_device || !_render_graph)
        {
            return;
        }

        // 1. Reset and configure surface size for dynamic resolution evaluation
        _render_graph->reset();
        _render_graph->set_surface_size(info.width, info.height);

        const auto sc_tex =
            _render_graph->import_texture(info.swapchain_texture, info.swapchain_view, rhi::image_layout::undefined);

        struct primary_pass_data
        {
            render_graph::rg_buffer_id pos_buf;
            render_graph::rg_buffer_id color_buf;
            render_graph::rg_texture_id target;
        };

        // 2. Pass 1: Primary Outer Triangle Pass (Clears swapchain to dark background)
        const auto& primary_pass = _render_graph->add_graphics_pass<primary_pass_data>(
            "PrimaryGeometryPass",
            [this, sc_tex](render_graph::pass_builder& builder, primary_pass_data& data) {
                const auto pos = builder.import_buffer(_positions_buffer);
                const auto col = builder.import_buffer(_colors_buffer);
                data.pos_buf = builder.read(pos, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.color_buf = builder.read(col, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.target = builder.set_color_attachment(
                    0, render_graph::rg_color_attachment{
                           .texture = sc_tex,
                           .load_op = rhi::load_op::clear,
                           .store_op = rhi::store_op::store,
                           .clear_value = {0.05F, 0.05F, 0.05F, 1.0F},
                       });
                builder.mark_sink();
            },
            [this]([[maybe_unused]] const primary_pass_data& data,
                   [[maybe_unused]] render_graph::pass_execution_context& ctx, rhi::command_list& pass_cmd) {
                pass_cmd.bind_pipeline(_pipeline);
                pass_cmd.bind_index_buffer(_index_buffer, index_type::uint16, 0);

                const auto constants = triangle_push_constants{
                    .positions_address = _positions_buffer.gpu_address,
                    .colors_address = _colors_buffer.gpu_address,
                };
                pass_cmd.push_constants(shader_stage::vertex, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw_indexed(3, 1, 0, 0, 0);
            });

        struct accent_pass_data
        {
            render_graph::rg_buffer_id pos_buf;
            render_graph::rg_buffer_id color_buf;
            render_graph::rg_texture_id target;
        };

        // 3. Pass 2: Inverted Accent Triangle Pass (Loads previous pass color target)
        const auto& accent_pass = _render_graph->add_graphics_pass<accent_pass_data>(
            "AccentGeometryPass",
            [this, &primary_pass](render_graph::pass_builder& builder, accent_pass_data& data) {
                const auto pos = builder.import_buffer(_accent_positions_buffer);
                const auto col = builder.import_buffer(_accent_colors_buffer);
                data.pos_buf = builder.read(pos, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.color_buf = builder.read(col, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.target = builder.set_color_attachment(
                    0, render_graph::rg_color_attachment{
                           .texture = primary_pass.target,
                           .load_op = rhi::load_op::load,
                           .store_op = rhi::store_op::store,
                       });
                builder.mark_sink();
            },
            [this]([[maybe_unused]] const accent_pass_data& data,
                   [[maybe_unused]] render_graph::pass_execution_context& ctx, rhi::command_list& pass_cmd) {
                pass_cmd.bind_pipeline(_pipeline);
                pass_cmd.bind_index_buffer(_index_buffer, index_type::uint16, 0);

                const auto constants = triangle_push_constants{
                    .positions_address = _accent_positions_buffer.gpu_address,
                    .colors_address = _accent_colors_buffer.gpu_address,
                };
                pass_cmd.push_constants(shader_stage::vertex, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw_indexed(3, 1, 0, 0, 0);
            });

        struct emblem_pass_data
        {
            render_graph::rg_buffer_id pos_buf;
            render_graph::rg_buffer_id color_buf;
            render_graph::rg_texture_id target;
        };

        // 4. Pass 3: Center Core Emblem Pass (Loads previous pass color target, Sinks DAG)
        _render_graph->add_graphics_pass<emblem_pass_data>(
            "CoreEmblemPass",
            [this, &accent_pass](render_graph::pass_builder& builder, emblem_pass_data& data) {
                const auto pos = builder.import_buffer(_emblem_positions_buffer);
                const auto col = builder.import_buffer(_emblem_colors_buffer);
                data.pos_buf = builder.read(pos, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.color_buf = builder.read(col, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.target = builder.set_color_attachment(
                    0, render_graph::rg_color_attachment{
                           .texture = accent_pass.target,
                           .load_op = rhi::load_op::load,
                           .store_op = rhi::store_op::store,
                       });
                builder.mark_sink();
            },
            [this]([[maybe_unused]] const emblem_pass_data& data,
                   [[maybe_unused]] render_graph::pass_execution_context& ctx, rhi::command_list& pass_cmd) {
                pass_cmd.bind_pipeline(_pipeline);
                pass_cmd.bind_index_buffer(_index_buffer, index_type::uint16, 0);

                const auto constants = triangle_push_constants{
                    .positions_address = _emblem_positions_buffer.gpu_address,
                    .colors_address = _emblem_colors_buffer.gpu_address,
                };
                pass_cmd.push_constants(shader_stage::vertex, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw_indexed(3, 1, 0, 0, 0);
            });

        // 5. Execute compiled multi-pass DAG directly via Render Graph
        const auto sync = render_graph::frame_sync_options{
            .wait_semaphore = info.acquire_semaphore,
            .wait_stages = rhi::pipeline_stage::attachment_output,
            .signal_semaphore = info.render_semaphore,
            .signal_stages = rhi::pipeline_stage::bottom_of_pipe,
            .timeline_semaphore = info.timeline_semaphore,
            .timeline_value = info.timeline_value,
        };
        [[maybe_unused]] const auto exec_res = _render_graph->execute(info.dev, sync);
    }

    auto render_graph_example::on_resize([[maybe_unused]] rhi::device& dev,
                                         rhi::render_surface_format surface_format, uint32_t width,
                                         uint32_t height) -> void
    {
        if (surface_format != _current_format)
        {
            create_pipeline(dev, surface_format);
        }
        if (_render_graph)
        {
            _render_graph->set_surface_size(width, height);
            _render_graph->get_allocator().on_surface_resize(dev);
        }
    }

    auto render_graph_example::shutdown(rhi::device& dev) -> void
    {
        if (_render_graph)
        {
            _render_graph->get_allocator().release_all(dev);
            _render_graph.reset();
        }

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

        if (_emblem_colors_buffer.handle != 0)
        {
            dev.destroy_buffer(_emblem_colors_buffer);
            _emblem_colors_buffer = {};
        }

        if (_emblem_positions_buffer.handle != 0)
        {
            dev.destroy_buffer(_emblem_positions_buffer);
            _emblem_positions_buffer = {};
        }

        if (_accent_colors_buffer.handle != 0)
        {
            dev.destroy_buffer(_accent_colors_buffer);
            _accent_colors_buffer = {};
        }

        if (_accent_positions_buffer.handle != 0)
        {
            dev.destroy_buffer(_accent_positions_buffer);
            _accent_positions_buffer = {};
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
