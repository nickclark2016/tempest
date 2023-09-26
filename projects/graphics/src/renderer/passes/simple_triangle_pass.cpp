#include "simple_triangle_pass.hpp"

#include <tempest/logger.hpp>

#include <cassert>
#include <fstream>
#include <sstream>

namespace tempest::graphics
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::simple_triangle_pass"}});

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

    bool simple_triangle_pass::initialize(gfx_device& device, VkFormat _color_format, VkFormat _depth_format,
                                          descriptor_set_layout_handle meshes)
    {
        auto tri_vs_spv = read_spirv("data/simple_triangle/simple_triangle.vx.spv");
        auto tri_fs_spv = read_spirv("data/simple_triangle/simple_triangle.px.spv");

        std::array<shader_stage, 5> stages = {{
            {
                .byte_code{reinterpret_cast<std::byte*>(tri_vs_spv.data()), tri_vs_spv.size() * sizeof(std::uint32_t)},
                .shader_type{VK_SHADER_STAGE_VERTEX_BIT},
            },
            {
                .byte_code{reinterpret_cast<std::byte*>(tri_fs_spv.data()), tri_fs_spv.size() * sizeof(std::uint32_t)},
                .shader_type{VK_SHADER_STAGE_FRAGMENT_BIT},
            },
        }};

        triangle_pipeline = device.create_pipeline({
            .dynamic_render{
                ._color_format{
                    _color_format,
                },
                .active_color_attachments{1},
                ._depth_format{_depth_format},
            },
            .ds{
                .depth_comparison{VK_COMPARE_OP_LESS_OR_EQUAL},
                .depth_test_enable{true},
                .depth_write_enable{true},
            },
            .blend{
                .blend_states{},
                .attachment_count{1},
            },
            .vertex_input{},
            .shaders{
                .stages{stages},
                .stage_count{2},
                .name{"triangle_shader"},
            },
            .desc_layouts{
                meshes,
            },
            .active_desc_layouts{1},
        });

        return true;
    }

    void simple_triangle_pass::record(command_buffer& buf, texture_handle color_target, texture_handle depth_target,
                                      VkRect2D viewport, descriptor_set_handle mesh_desc)
    {
        render_attachment_descriptor color_attachments[] = {
            render_attachment_descriptor{
                .tex{color_target},
                .layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                .load{VK_ATTACHMENT_LOAD_OP_CLEAR},
                .store{VK_ATTACHMENT_STORE_OP_STORE},
                .clear{.color{0.5f, 0.1f, 0.8f, 1.0f}},
            },
        };

        render_attachment_descriptor depth_attachment{
            .tex{depth_target},
            .layout{VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL},
            .load{VK_ATTACHMENT_LOAD_OP_CLEAR},
            .store{VK_ATTACHMENT_STORE_OP_STORE},
            .clear{
                .depthStencil{
                    .depth{1.0f},
                    .stencil{0},
                },
            },
        };

        descriptor_set_handle sets[] = {mesh_desc};
        std::uint32_t offsets[] = {0, 0, 0, 0};

        buf.set_scissor_region(viewport)
            .set_viewport({
                static_cast<float>(viewport.offset.x),
                static_cast<float>(viewport.offset.y),
                static_cast<float>(viewport.extent.width),
                static_cast<float>(viewport.extent.height),
                0.0f,
                1.0f,
            })
            .bind_pipeline(triangle_pipeline)
            .begin_rendering(viewport, color_attachments, depth_attachment, std::nullopt)
            .bind_descriptor_set(sets, offsets)
            .draw(36, 1, 0, 0)
            .end_rendering();
    }

    void simple_triangle_pass::release(gfx_device& device)
    {
        device.release_pipeline(triangle_pipeline);
    }
} // namespace tempest::graphics