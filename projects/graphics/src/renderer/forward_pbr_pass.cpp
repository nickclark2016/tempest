#include "forward_pbr_pass.hpp"

#include "device.hpp"

#include <array>
#include <fstream>
#include <sstream>
#include <tuple>
#include <vector>

namespace tempest::graphics
{
    namespace
    {
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

        render_pass_handle create_render_pass(gfx_device* device, texture_handle color, texture_handle depth)
        {
            auto color_tex = device->access_texture(color);
            auto depth_tex = device->access_texture(depth);

            render_pass_attachment_info attachments = {
                .color_formats{color_tex->image_fmt},
                .depth_stencil_format{depth_tex->image_fmt},
                .color_attachment_count{1},
                .color_load{render_pass_attachment_operation::CLEAR},
                .depth_load{render_pass_attachment_operation::CLEAR},
            };

            return device->create_render_pass({
                .render_targets{1},
                .type{render_pass_type::RASTERIZATION},
                .color_outputs{color},
                .depth_stencil_texture{depth},
                .color_load{render_pass_attachment_operation::CLEAR},
                .depth_load{render_pass_attachment_operation::CLEAR},
                .name{"PBR_Forward"},
            });
        }

        std::tuple<pipeline_handle, descriptor_set_layout_handle> create_opaque_pipeline(gfx_device* device,
                                                                                         texture_handle color,
                                                                                         texture_handle depth)
        {
            attachment_blend_state color_blend = {
                .rgb{
                    .source{VK_BLEND_FACTOR_SRC_ALPHA},
                    .destination{VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
                    .operation{VK_BLEND_OP_ADD},
                },
                .alpha{
                    .source{VK_BLEND_FACTOR_ONE},
                    .destination{VK_BLEND_FACTOR_ZERO},
                    .operation{VK_BLEND_OP_ADD},
                },
                .write_mask{VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                            VK_COLOR_COMPONENT_A_BIT},
                .blend_enabled{false},
            };

            vertex_stream verts = {
                .binding{0},
                .stride{(3 + 2 + 3 + 4) * sizeof(float)},
                .input_rate{VK_VERTEX_INPUT_RATE_VERTEX},
            };

            vertex_attribute position_attrib = {
                .location{0},
                .binding{0},
                .offset{0},
                .fmt{VK_FORMAT_R32G32B32_SFLOAT},
            };

            vertex_attribute uv_attrib = {
                .location{1},
                .binding{0},
                .offset{3 * sizeof(float)},
                .fmt{VK_FORMAT_R32G32_SFLOAT},
            };

            vertex_attribute normal_attrib = {
                .location{2},
                .binding{0},
                .offset{(3 + 2) * sizeof(float)},
                .fmt{VK_FORMAT_R32G32B32_SFLOAT},
            };

            vertex_attribute tangent_attrib = {
                .location{3},
                .binding{0},
                .offset{(3 + 2 + 3) * sizeof(float)},
                .fmt{VK_FORMAT_R32G32B32A32_SFLOAT},
            };

            vertex_input_create_info vertex = {
                .streams{verts},
                .attributes{position_attrib, uv_attrib, normal_attrib, tangent_attrib},
                .stream_count{1},
                .attribute_count{4},
            };

            auto vs_spv = read_spirv("data/forward_pbr.vs.spv");
            auto fs_spv = read_spirv("data/forward_pbr.fs.spv");

            std::array<shader_stage, 5> stages = {{
                {
                    .byte_code{reinterpret_cast<std::byte*>(vs_spv.data()), vs_spv.size() * sizeof(std::uint32_t)},
                    .shader_type{VK_SHADER_STAGE_VERTEX_BIT},
                },
                {
                    .byte_code{reinterpret_cast<std::byte*>(fs_spv.data()), fs_spv.size() * sizeof(std::uint32_t)},
                    .shader_type{VK_SHADER_STAGE_FRAGMENT_BIT},
                },
            }};

            using binding_type = descriptor_set_layout_create_info::binding;

            binding_type scene = {
                .type{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                .start_binding{0},
                .binding_count{1},
                .name{"PbrLayout_Set0_Binding1_SceneData"},
            };

            binding_type material = {
                .type{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                .start_binding{1},
                .binding_count{1},
                .name{"PbrLayout_Set0_Binding1_MaterialData"},
            };

            binding_type model = {
                .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                .start_binding{2},
                .binding_count{1},
                .name{"PbrLayout_Set0_Binding1_ModelData"},
            };

            std::array<descriptor_set_layout_create_info::binding, max_descriptors_per_set> bindings = {
                scene,
                material,
                model,
            };

            auto buffer_data_layout = device->create_descriptor_set_layout({
                .bindings{bindings},
                .binding_count{3},
                .set_index{1},
                .name{"DescriptorSetLayout_PbrBufferData"},
            });

            pipeline_create_info ci = {
                .ds{
                    .depth_comparison{VK_COMPARE_OP_LESS_OR_EQUAL},
                    .depth_test_enable{true},
                    .depth_write_enable{true},
                },
                .blend{
                    .blend_states{color_blend},
                    .attachment_count{1},
                },
                .vertex_input{vertex},
                .shaders{
                    .stages{stages},
                    .stage_count{2},
                    .name{"PBR_Forward_Shaders"},
                },
                .desc_layouts{
                    buffer_data_layout,
                    device->get_bindless_texture_descriptor_set_layout(),
                },
                .active_desc_layouts{2},
                .name{"PBR_OpaquePipeline"},
            };

            return std::make_tuple(device->create_pipeline(ci), buffer_data_layout);
        }

        buffer_handle initialze_draw_parameter_buffer(gfx_device* device)
        {
            const buffer_create_info bci = {
                .type{VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
                .usage{resource_usage::STREAM},
                .size{static_cast<std::uint32_t>(forward_pbr_pass::MAX_ENTITIES_PER_FRAME *
                                                 sizeof(VkDrawIndexedIndirectCommand) *
                                                 device->num_frames_in_flight())},
                .name{"ForwardPbrPass_DrawIndirectArguments"},
            };

            return device->create_buffer(bci);
        }
    } // namespace

    forward_pbr_pass forward_pbr_pass::create(gfx_device* device, texture_handle color, texture_handle depth)
    {
        auto pass = create_render_pass(device, color, depth);
        auto [pipeline, buffer_desc_set] = create_opaque_pipeline(device, color, depth);
        return forward_pbr_pass{
            .pass{pass},
            .buffer_layout_desc{buffer_desc_set},
            .forward_pbr_pipeline{pipeline},
            .color_target{color},
            .depth_target{depth},
            .draw_parameter_buffer{initialze_draw_parameter_buffer(device)},
        };
    }

    void forward_pbr_pass::release(gfx_device* device)
    {
        device->release_buffer(draw_parameter_buffer);
        device->release_descriptor_set_layout(buffer_layout_desc);
        device->release_pipeline(forward_pbr_pipeline);
        device->release_render_pass(pass);
    }
    
    void forward_pbr_pass::render(gfx_device* device)
    {
    }
} // namespace tempest::graphics