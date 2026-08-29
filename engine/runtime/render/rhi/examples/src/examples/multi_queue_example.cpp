#include "multi_queue_example.hpp"

#include <cmath>
#include <cstring>
#include <stdint.h> // for uint32_t for shaders
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

namespace shaders::animate
{
    namespace cs
    {
#include <animate.comp.h>
    } // namespace cs
} // namespace shaders::animate

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

        struct animate_push_constants
        {
            uint64_t input_positions_address;
            uint64_t output_positions_address;
            uint64_t output_colors_address;
            float time;
            uint32_t count;
        };

        // 3-blade symmetric star geometry (9 vertices, 3 triangles)
        constexpr auto template_positions = array<vec2, 9>{
            // Blade 1 (Top)
            vec2{0.0F, -0.65F},
            vec2{0.22F, -0.05F},
            vec2{-0.22F, -0.05F},
            // Blade 2 (Bottom-Right)
            vec2{0.56F, 0.35F},
            vec2{0.05F, 0.25F},
            vec2{0.18F, -0.15F},
            // Blade 3 (Bottom-Left)
            vec2{-0.56F, 0.35F},
            vec2{-0.18F, -0.15F},
            vec2{-0.05F, 0.25F},
        };

        constexpr auto indices = array<uint16_t, 9>{
            0, 1, 2, 3, 4, 5, 6, 7, 8,
        };
    } // namespace

    auto multi_queue_example::init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool
    {
        _device = &dev;

        // 1. Create Staging Upload Buffer for template geometry
        const auto staging_desc = buffer_desc{
            .size = sizeof(template_positions),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::transfer_src,
            .name = "mq_staging_buffer",
        };
        _staging_buffer = dev.create_buffer(staging_desc);
        if (_staging_buffer.handle == 0)
        {
            return false;
        }
        std::memcpy(_staging_buffer.cpu_address, template_positions.data(), sizeof(template_positions));

        // 2. Create GPU Base Positions Buffer (Transfer Destination)
        const auto base_pos_desc = buffer_desc{
            .size = sizeof(template_positions),
            .memory_usage = memory_usage::device_only,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address | buffer_usage::transfer_dst,
            .name = "mq_base_positions_buffer",
        };
        _base_positions_buffer = dev.create_buffer(base_pos_desc);
        if (_base_positions_buffer.handle == 0 || _base_positions_buffer.gpu_address == 0)
        {
            return false;
        }

        // 3. Create Dynamic Output Positions Buffer (Compute RW -> Vertex Shader Read)
        const auto dyn_pos_desc = buffer_desc{
            .size = sizeof(template_positions),
            .memory_usage = memory_usage::device_only,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "mq_dynamic_positions_buffer",
        };
        _dynamic_positions_buffer = dev.create_buffer(dyn_pos_desc);
        if (_dynamic_positions_buffer.handle == 0 || _dynamic_positions_buffer.gpu_address == 0)
        {
            return false;
        }

        // 4. Create Dynamic Output Colors Buffer (Compute RW -> Vertex Shader Read)
        const auto dyn_col_desc = buffer_desc{
            .size = sizeof(vec3) * template_positions.size(),
            .memory_usage = memory_usage::device_only,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            .name = "mq_dynamic_colors_buffer",
        };
        _dynamic_colors_buffer = dev.create_buffer(dyn_col_desc);
        if (_dynamic_colors_buffer.handle == 0 || _dynamic_colors_buffer.gpu_address == 0)
        {
            return false;
        }

        // 5. Create Index Buffer
        const auto index_desc = buffer_desc{
            .size = sizeof(indices),
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::index_buffer,
            .name = "mq_index_buffer",
        };
        _index_buffer = dev.create_buffer(index_desc);
        if (_index_buffer.handle == 0)
        {
            return false;
        }
        std::memcpy(_index_buffer.cpu_address, indices.data(), sizeof(indices));

        // 6. Initialize Render Graph
        _render_graph = make_unique<render_graph::render_graph>(1920, 1080);

        // 7. Create Graphics and Compute Pipelines
        return create_pipeline(dev, surface_format) && create_compute_pipeline(dev);
    }

    auto multi_queue_example::create_pipeline(rhi::device& dev, rhi::render_surface_format surface_format) -> bool
    {
        if (_graphics_pipeline.handle != 0)
        {
            dev.destroy_graphics_pipeline(_graphics_pipeline);
            _graphics_pipeline = {};
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

        _graphics_pipeline = dev.create_graphics_pipeline(pipe_desc);
        return _graphics_pipeline.handle != 0;
    }

    auto multi_queue_example::create_compute_pipeline(rhi::device& dev) -> bool
    {
        if (_compute_pipeline.handle != 0)
        {
            dev.destroy_compute_pipeline(_compute_pipeline);
            _compute_pipeline = {};
        }

        const auto cs_desc = shader_module_desc{
            .stage = shader_stage::compute,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::animate::cs::animate_cs_spv),
                                        sizeof(shaders::animate::cs::animate_cs_spv)},
            .entry_point = "AnimateCSMain",
        };

        const auto pipe_desc = compute_pipeline_desc{
            .shader_module = cs_desc,
        };

        _compute_pipeline = dev.create_compute_pipeline(pipe_desc);
        return _compute_pipeline.handle != 0;
    }

    auto multi_queue_example::render(const frame_render_info& info) -> void
    {
        if (!_device || !_render_graph)
        {
            return;
        }

        _time += 0.016F;

        _render_graph->reset();
        _render_graph->set_surface_size(info.width, info.height);

        const auto sc_tex =
            _render_graph->import_texture(info.swapchain_texture, info.swapchain_view, rhi::image_layout::undefined);

        struct transfer_pass_data
        {
            render_graph::rg_buffer_id staging;
            render_graph::rg_buffer_id base_pos;
        };

        const auto& transfer_pass = _render_graph->add_transfer_pass<transfer_pass_data>(
            "AsyncGeometryUploadPass",
            [this](render_graph::pass_builder& builder, transfer_pass_data& data) {
                const auto stg = builder.import_buffer(_staging_buffer);
                const auto dst = builder.import_buffer(_base_positions_buffer);
                data.staging = builder.read(stg, rhi::pipeline_stage::copy, rhi::resource_access::read);
                data.base_pos = builder.write(dst, rhi::pipeline_stage::copy, rhi::resource_access::write);
                builder.mark_sink();
            },
            [this]([[maybe_unused]] const transfer_pass_data& data,
                   [[maybe_unused]] render_graph::pass_execution_context& ctx, rhi::command_list& pass_cmd) {
                const auto copy_region = buffer_copy_region{
                    .src_offset = 0,
                    .dst_offset = 0,
                    .size = sizeof(template_positions),
                };
                pass_cmd.copy_buffer(_staging_buffer, _base_positions_buffer,
                                     span<const buffer_copy_region>{&copy_region, 1});
            });

        struct compute_pass_data
        {
            render_graph::rg_buffer_id input_pos;
            render_graph::rg_buffer_id output_pos;
            render_graph::rg_buffer_id output_col;
        };

        const auto& compute_pass = _render_graph->add_compute_pass<compute_pass_data>(
            "AsyncVertexAnimatePass",
            [this, &transfer_pass](render_graph::pass_builder& builder, compute_pass_data& data) {
                data.input_pos =
                    builder.read(transfer_pass.base_pos, rhi::pipeline_stage::compute, rhi::resource_access::read);
                const auto dyn_pos = builder.import_buffer(_dynamic_positions_buffer);
                const auto dyn_col = builder.import_buffer(_dynamic_colors_buffer);
                data.output_pos = builder.write(dyn_pos, rhi::pipeline_stage::compute, rhi::resource_access::write);
                data.output_col = builder.write(dyn_col, rhi::pipeline_stage::compute, rhi::resource_access::write);
                builder.mark_sink();
            },
            [this]([[maybe_unused]] const compute_pass_data& data,
                   [[maybe_unused]] render_graph::pass_execution_context& ctx, rhi::command_list& pass_cmd) {
                pass_cmd.bind_pipeline(_compute_pipeline);
                const auto anim_constants = animate_push_constants{
                    .input_positions_address = _base_positions_buffer.gpu_address,
                    .output_positions_address = _dynamic_positions_buffer.gpu_address,
                    .output_colors_address = _dynamic_colors_buffer.gpu_address,
                    .time = _time,
                    .count = static_cast<uint32_t>(template_positions.size()),
                };
                pass_cmd.push_constants(
                    shader_stage::compute, 0,
                    span<const byte>{reinterpret_cast<const byte*>(&anim_constants), sizeof(anim_constants)});
                pass_cmd.dispatch(1, 1, 1);
            });

        struct raster_pass_data
        {
            render_graph::rg_buffer_id pos;
            render_graph::rg_buffer_id col;
            render_graph::rg_texture_id target;
        };

        _render_graph->add_graphics_pass<raster_pass_data>(
            "StarRasterPass",
            [this, &compute_pass, sc_tex](render_graph::pass_builder& builder, raster_pass_data& data) {
                data.pos =
                    builder.read(compute_pass.output_pos, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.col =
                    builder.read(compute_pass.output_col, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.target = builder.set_color_attachment(0, render_graph::rg_color_attachment{
                                                                  .texture = sc_tex,
                                                                  .load_op = rhi::load_op::clear,
                                                                  .store_op = rhi::store_op::store,
                                                                  .clear_value = {0.05F, 0.05F, 0.05F, 1.0F},
                                                              });
                builder.mark_sink();
            },
            [this]([[maybe_unused]] const raster_pass_data& data,
                   [[maybe_unused]] render_graph::pass_execution_context& ctx, rhi::command_list& pass_cmd) {
                pass_cmd.bind_pipeline(_graphics_pipeline);
                pass_cmd.bind_index_buffer(_index_buffer, index_type::uint16, 0);

                const auto constants = triangle_push_constants{
                    .positions_address = _dynamic_positions_buffer.gpu_address,
                    .colors_address = _dynamic_colors_buffer.gpu_address,
                };
                pass_cmd.push_constants(shader_stage::vertex, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw_indexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
            });

        const auto sync = render_graph::frame_sync_options{
            .wait_semaphore = info.acquire_semaphore,
            .wait_stages = rhi::pipeline_stage::attachment_output,
            .signal_semaphore = info.render_semaphore,
            .signal_stages = rhi::pipeline_stage::bottom_of_pipe,
            .timeline_semaphore = info.timeline_semaphore,
            .timeline_value = info.timeline_value,
            .presented_texture = info.swapchain_texture,
        };
        [[maybe_unused]] const auto exec_res = _render_graph->execute(info.dev, sync);
    }

    auto multi_queue_example::on_resize([[maybe_unused]] rhi::device& dev, rhi::render_surface_format surface_format,
                                        uint32_t width, uint32_t height) -> void
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

    auto multi_queue_example::shutdown(rhi::device& dev) -> void
    {
        if (_render_graph)
        {
            _render_graph->get_allocator().release_all(dev);
            _render_graph.reset();
        }

        if (_compute_pipeline.handle != 0)
        {
            dev.destroy_compute_pipeline(_compute_pipeline);
            _compute_pipeline = {};
        }

        if (_graphics_pipeline.handle != 0)
        {
            dev.destroy_graphics_pipeline(_graphics_pipeline);
            _graphics_pipeline = {};
        }

        if (_index_buffer.handle != 0)
        {
            dev.destroy_buffer(_index_buffer);
            _index_buffer = {};
        }

        if (_dynamic_colors_buffer.handle != 0)
        {
            dev.destroy_buffer(_dynamic_colors_buffer);
            _dynamic_colors_buffer = {};
        }

        if (_dynamic_positions_buffer.handle != 0)
        {
            dev.destroy_buffer(_dynamic_positions_buffer);
            _dynamic_positions_buffer = {};
        }

        if (_base_positions_buffer.handle != 0)
        {
            dev.destroy_buffer(_base_positions_buffer);
            _base_positions_buffer = {};
        }

        if (_staging_buffer.handle != 0)
        {
            dev.destroy_buffer(_staging_buffer);
            _staging_buffer = {};
        }
    }
} // namespace tempest::rhi::examples
