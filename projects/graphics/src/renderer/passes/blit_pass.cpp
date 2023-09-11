#include "blit_pass.hpp"

#include "../device.hpp"

#include <tempest/logger.hpp>

#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

namespace tempest::graphics
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::renderer_impl"}});

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

    bool blit_pass::initialize(gfx_device& device, std::uint16_t width, std::uint16_t height, VkFormat blit_src_format)
    {
        blit_src = device.create_texture({
            .width{width},
            .height{height},
            .depth{1},
            .mipmap_count{1},
            .flags{texture_flags::RENDER_TARGET},
            .image_format{blit_src_format},
            .name{"BlitPipeline_BlitColorSrc"},
        });

        // transition image

        {
            auto& cmd = device.get_instant_command_buffer();
            cmd.begin();
            cmd.transition_to_color_image(blit_src);
            cmd.end();
            device.execute_immediate(cmd);
        }

        auto vs_spv = read_spirv("data/blit/blit.vx.spv");
        auto fs_spv = read_spirv("data/blit/blit.px.spv");

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

        image_input_layout = device.create_descriptor_set_layout({
            .bindings{
                descriptor_set_layout_create_info::binding{
                    .type{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE},
                    .start_binding{0},
                    .binding_count{1},
                    .name{"BlitPass_Image"},
                },
                descriptor_set_layout_create_info::binding{
                    .type{VK_DESCRIPTOR_TYPE_SAMPLER},
                    .start_binding{1},
                    .binding_count{1},
                    .name{"BlitPass_Sampler"},
                },
            },
            .binding_count{2},
            .set_index{0},
            .name{"BlitPass_DescSet0"},
        });

        blit_pipeline = device.create_pipeline({
            .dynamic_render{
                ._color_format{device.get_swapchain_format()},
                .active_color_attachments{0},
            },
            .ds{
                .depth_comparison{VK_COMPARE_OP_LESS_OR_EQUAL},
                .depth_test_enable{false},
                .depth_write_enable{false},
            },
            .blend{
                .blend_states{},
                .attachment_count{1},
            },
            .vertex_input{},
            .shaders{
                .stages{stages},
                .stage_count{2},
                .name{"blit_shader"},
            },
            .desc_layouts{image_input_layout},
            .active_desc_layouts{1},
        });

        blit_sampler = device.create_sampler({
            .u_address{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
            .v_address{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
            .w_address{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
            .name{"BlitPass_Sampler"},
        });

        image_inputs = device.create_descriptor_set(descriptor_set_builder("BlitPass_DescriptorSet")
                                                        .set_layout(image_input_layout)
                                                        .add_image(blit_src, 0)
                                                        .add_sampler(blit_sampler, 1));
        return false;
    }

    void blit_pass::record(command_buffer& buf, texture_handle blit_dst, VkRect2D viewport)
    {
        render_attachment_descriptor color_targets[] = {
            {
                .tex{blit_dst},
                .layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                .load{VK_ATTACHMENT_LOAD_OP_CLEAR},
                .store{VK_ATTACHMENT_STORE_OP_STORE},
            },
        };

        // descriptor_set_handle sets_to_bind[] = {image_inputs};

        buf.blit_image(blit_src, blit_dst);

        //buf.set_clear_color(0.0f, 0.0f, 0.0f, 1.0f)
        //    .set_clear_depth_stencil(1.0f, 0)
        //    .use_default_scissor()
        //    .use_default_viewport(false)
        //    .bind_pipeline(blit_pipeline)
        //    .begin_rendering(viewport, color_targets, std::nullopt, std::nullopt)
        //    .bind_descriptor_set(sets_to_bind, {})
        //    .draw(6, 1, 0, 0)
        //    .end_rendering();
    }

    void blit_pass::release(gfx_device& device)
    {
        device.release_descriptor_set(image_inputs);
        device.release_descriptor_set_layout(image_input_layout);
        device.release_sampler(blit_sampler);
        device.release_texture(blit_src);
        device.release_pipeline(blit_pipeline);
    }

    void blit_pass::resize_blit_source(gfx_device& device, std::uint16_t width, std::uint16_t height,
                                       VkFormat blit_src_format)
    {
        device.release_texture(blit_src);

        blit_src = device.create_texture({
            .width{width},
            .height{height},
            .depth{1},
            .mipmap_count{1},
            .flags{texture_flags::RENDER_TARGET},
            .image_format{blit_src_format},
            .name{"BlitPipeline_BlitColorSrc"},
        });

        // transition image

        {
            auto& cmd = device.get_instant_command_buffer();
            cmd.begin();
            cmd.transition_to_color_image(blit_src);
            cmd.end();
            device.execute_immediate(cmd);
        }

        device.release_descriptor_set(image_inputs);

        image_inputs = device.create_descriptor_set(descriptor_set_builder("BlitPass_DescriptorSet")
                                                        .set_layout(image_input_layout)
                                                        .add_image(blit_src, 0)
                                                        .add_sampler(blit_sampler, 1));
    }

    void blit_pass::transition_to_present(command_buffer& buf, texture_handle blit_dst)
    {
        state_transition_descriptor present_transitions[] = {
            state_transition_descriptor{
                .texture{blit_dst},
                .first_mip{0},
                .mip_count{1},
                .base_layer{0},
                .layer_count{1},
                .src_state{resource_state::RENDER_TARGET},
                .dst_state{resource_state::PRESENT},
            },
        };

        buf.transition_resource(present_transitions, pipeline_stage::FRAMEBUFFER_OUTPUT, pipeline_stage::END);
    }
} // namespace tempest::graphics