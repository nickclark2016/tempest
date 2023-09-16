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
    }

    void blit_pass::release(gfx_device& device)
    {
        device.release_texture(blit_src);
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