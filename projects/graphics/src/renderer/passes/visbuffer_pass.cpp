#include "visbuffer_pass.hpp"

#include <tempest/logger.hpp>

#include <cassert>
#include <fstream>
#include <sstream>

namespace tempest::graphics
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::visibility_buffer"}});

        inline std::vector<uint32_t> read_spirv(const std::string& path)
        {
            std::ostringstream buf;
            std::ifstream input(path.c_str(), std::ios::ate | std::ios::binary);
            assert(input);
            size_t file_size = (size_t)input.tellg();
            std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
            input.seekg(0);
            input.read(reinterpret_cast<char*>(buffer.data()), file_size);
            return buffer;
        }
    } // namespace

    bool visibility_buffer_pass::initialize(gfx_device& device, std::uint32_t width, std::uint32_t height)
    {
        descriptor_set_layout_create_info object_data_layout = {
            .bindings{{
                {
                    .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
                    .start_binding{0},
                    .binding_count{1},
                    .name{"mesh_vertex_data_buffer_binding"},
                },
                {
                    .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
                    .start_binding{1},
                    .binding_count{1},
                    .name{"mesh_layout_data_buffer_binding"},
                },
                {
                    .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
                    .start_binding{2},
                    .binding_count{1},
                    .name{"instance_data_buffer_binding"},
                },
                {
                    .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
                    .start_binding{3},
                    .binding_count{1},
                    .name{"material_data_buffer_binding"},
                },
                {
                    .type{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
                    .start_binding{4},
                    .binding_count{1},
                    .name{"scene_data_buffer_binding"},
                },
            }},
            .binding_count{5},
            .set_index{0},
            .name{"object_data_layout"},
        };

        world_desc_layout = device.create_descriptor_set_layout(object_data_layout);

        descriptor_set_layout_create_info visbuffer_data_layout = {
            .bindings{{
                {
                    .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
                    .start_binding{0},
                    .binding_count{1},
                    .name{"material_count_buffer_binding"},
                },
                {
                    .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
                    .start_binding{1},
                    .binding_count{1},
                    .name{"material_start_buffer_binding"},
                },
                {
                    .type{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE},
                    .start_binding{2},
                    .binding_count{1},
                    .name{"visibilitY_buffer_target_binding"},
                },
                {
                    .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
                    .start_binding{3},
                    .binding_count{1},
                    .name{"pixel_xy_buffer_binding"},
                },
            }},
            .binding_count{4},
            .set_index{1},
            .name{"visbuffer_data_layout"},
        };

        vis_buffer_layout = device.create_descriptor_set_layout(visbuffer_data_layout);

        auto visbuffer_vs_spv = read_spirv("data/visbuffer/visbuffer.vx.spv");
        auto visbuffer_fs_spv = read_spirv("data/visbuffer/visbuffer.px.spv");
        auto material_count_cs_spv = read_spirv("data/visbuffer/material_count.cx.spv");
        auto material_start_cs_spv = read_spirv("data/visbuffer/material_start.cx.spv");
        auto material_pixel_sort_spv = read_spirv("data/visbuffer/material_pixel_sort.cx.spv");

        std::array<shader_stage, 5> visbuffer_stages = {{
            {
                .byte_code{reinterpret_cast<std::byte*>(visbuffer_vs_spv.data()),
                           visbuffer_vs_spv.size() * sizeof(std::uint32_t)},
                .shader_type{VK_SHADER_STAGE_VERTEX_BIT},
            },
            {
                .byte_code{reinterpret_cast<std::byte*>(visbuffer_fs_spv.data()),
                           visbuffer_fs_spv.size() * sizeof(std::uint32_t)},
                .shader_type{VK_SHADER_STAGE_FRAGMENT_BIT},
            },
        }};

        visbuffer_populate_gfx = device.create_pipeline({
            .dynamic_render{
                .color_format{visbuffer_fmt},
                .depth_format{depth_fmt},
            },
            .ds{
                .depth_comparison{VK_COMPARE_OP_LESS_OR_EQUAL},
                .depth_test_enable{VK_TRUE},
                .depth_write_enable{VK_TRUE},
            },
            .blend{
                .blend_states{},
                .attachment_count{1},
            },
            .vertex_input{},
            .shaders{
                .stages{visbuffer_stages},
                .stage_count{2},
                .name{"visibility_buffer_fill"},
            },
            .desc_layouts{{
                world_desc_layout,
                vis_buffer_layout,
            }},
            .active_desc_layouts{2},
        });

        return visbuffer_populate_gfx.index != invalid_resource_handle;
    }

    void visibility_buffer_pass::record(command_buffer& buf, descriptor_set_handle world_data,
                                        std::span<std::uint32_t> world_set_offset)
    {
    }

    void visibility_buffer_pass::release(gfx_device& device)
    {
        device.release_pipeline(visbuffer_populate_gfx);
        device.release_descriptor_set_layout(world_desc_layout);
        device.release_descriptor_set_layout(vis_buffer_layout);
    }
} // namespace tempest::graphics