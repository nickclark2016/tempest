#include "renderer_impl.hpp"

#include <fstream>
#include <sstream>

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
    } // namespace

    void irenderer::impl::set_up()
    {
        auto tri_vs_spv = read_spirv("data/triangle.vs.spv");
        auto tri_fs_spv = read_spirv("data/triangle.fs.spv");

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

        triangle_pipeline = device->create_pipeline({
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
            .output{device->get_swapchain_attachment_info()},
        });
    }

    void irenderer::impl::render()
    {
        device->start_frame();

        auto& cmds = device->get_command_buffer(queue_type::GRAPHICS, false);

        cmds.begin();

        cmds.set_clear_color(1.0f, 0.0f, 1.0f, 1.0f)
            .set_clear_depth_stencil(1.0f, 0)
            .use_default_scissor()
            .use_default_viewport()
            .bind_render_pass(device->get_swapchain_pass())
            .bind_pipeline(triangle_pipeline)
            .draw(3, 1, 0, 0);

        cmds.end();

        device->queue_command_buffer(cmds);

        device->end_frame();
    }

    void irenderer::impl::clean_up()
    {
    }
} // namespace tempest::graphics