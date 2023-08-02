#include "renderer_impl.hpp"

#include <tempest/logger.hpp>

#include <fstream>
#include <sstream>

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

    void irenderer::impl::set_up()
    {
        vertex_buffer_allocator.emplace(
            buffer_create_info{
                .type{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
                .usage{resource_usage::DYNAMIC},
                .size{1024 * 1024 * 32},
                .name{"mesh_buffer"},
            },
            device.get()); // 32 mb initial allocation

        instance_buffer_allocator.emplace(
            buffer_create_info{
                .type{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
                .usage{resource_usage::DYNAMIC},
                .size{1024 * 1024 * 32},
                .name{"instance_data_buffer"},
            },
            device.get());

        scene_buffer_allocator.emplace(
            buffer_create_info{
                .type{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
                .usage{resource_usage::STREAM},
                .size{1024 * 64 * 3},
                .name{"scene_data_buffer"},
            },
            device.get());

        _create_mesh_buffers();
        _create_blit_pipeline();
        _create_triangle_pipeline();
        // pbr_forward.emplace(forward_pbr_pass::create(device.get(), color_target, depth_target));
    }

    void irenderer::impl::render()
    {
        device->start_frame();

        auto& cmds = device->get_command_buffer(queue_type::GRAPHICS, false);

        cmds.begin();

        texture_barrier color_target_barriers[] = {
            {
                .tex{color_target},
            },
        };

        descriptor_set_handle mesh_sets[] = {mesh_data_set};
        uint32_t offsets[] = {0, 0, 0};

        state_transition_descriptor prepare_render_transitions[] = {
            state_transition_descriptor{
                .texture{color_target},
                .first_mip{0},
                .mip_count{1},
                .base_layer{0},
                .layer_count{1},
                .src_state{resource_state::UNDEFINED},
                .dst_state{resource_state::RENDER_TARGET},
            },
        };

        state_transition_descriptor prepare_blit_transitions[] = {
            state_transition_descriptor{
                .texture{color_target},
                .first_mip{0},
                .mip_count{1},
                .base_layer{0},
                .layer_count{1},
                .src_state{resource_state::RENDER_TARGET},
                .dst_state{resource_state::FRAGMENT_SHADER_RESOURCE},
            },
        };

        state_transition_descriptor prepare_pre_present_transitions[] = {
            state_transition_descriptor{
                .texture{device->get_current_swapchain_texture()},
                .first_mip{0},
                .mip_count{1},
                .base_layer{0},
                .layer_count{1},
                .src_state{resource_state::UNDEFINED},
                .dst_state{resource_state::RENDER_TARGET},
            },
        };

        state_transition_descriptor prepare_present_transitions[] = {
            state_transition_descriptor{
                .texture{device->get_current_swapchain_texture()},
                .first_mip{0},
                .mip_count{1},
                .base_layer{0},
                .layer_count{1},
                .src_state{resource_state::RENDER_TARGET},
                .dst_state{resource_state::PRESENT},
            },
        };

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

        render_attachment_descriptor swapchain_attachments[] = {
            render_attachment_descriptor{
                .tex{device->get_current_swapchain_texture()},
                .layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                .load{VK_ATTACHMENT_LOAD_OP_CLEAR},
                .store{VK_ATTACHMENT_STORE_OP_STORE},
            },
        };

        cmds.transition_resource(prepare_pre_present_transitions, pipeline_stage::TOP,
                                 pipeline_stage::FRAMEBUFFER_OUTPUT)
            .transition_resource(prepare_render_transitions, pipeline_stage::FRAGMENT_SHADER,
                                 pipeline_stage::FRAMEBUFFER_OUTPUT)
            .set_scissor_region({{0, 0}, {1280, 720}})
            .set_viewport({0, 0, 1280, 720, 0.0, 1.0})
            .bind_pipeline(triangle_pipeline)
            .begin_rendering({{0, 0}, {1280, 720}}, color_attachments, depth_attachment, std::nullopt)
            .bind_descriptor_set({mesh_sets}, {offsets})
            .draw(3, 1, 0, 0)
            .end_rendering()
            .transition_resource(prepare_blit_transitions, pipeline_stage::FRAMEBUFFER_OUTPUT,
                                 pipeline_stage::FRAGMENT_SHADER);

        descriptor_set_handle sets_to_bind[] = {blit_desc_set};

        cmds.set_clear_color(0.0f, 0.0f, 0.0f, 1.0f)
            .set_clear_depth_stencil(1.0f, 0)
            .use_default_scissor()
            .use_default_viewport(false)
            .bind_pipeline(blit_pipeline)
            .begin_rendering({{0, 0}, {1280, 720}}, swapchain_attachments, std::nullopt, std::nullopt)
            .bind_descriptor_set({sets_to_bind}, {})
            .draw(6, 1, 0, 0)
            .end_rendering()
            .transition_resource(prepare_present_transitions, pipeline_stage::FRAMEBUFFER_OUTPUT, pipeline_stage::END);

        cmds.end();

        device->queue_command_buffer(cmds);

        // pbr_forward->render(this->device.get());

        device->end_frame();
    }

    void irenderer::impl::clean_up()
    {
        device->release_descriptor_set(mesh_data_set);
        device->release_descriptor_set_layout(mesh_data_layout);

        vertex_buffer_allocator->release();
        instance_buffer_allocator->release();
        scene_buffer_allocator->release();

        // pbr_forward->release(device.get());
        // pbr_forward = std::nullopt;

        device->release_pipeline(blit_pipeline);
        device->release_pipeline(triangle_pipeline);
        device->release_texture(color_target);
        device->release_texture(depth_target);
        device->release_sampler(default_sampler);
        device->release_descriptor_set(blit_desc_set);
        device->release_descriptor_set_layout(blit_desc_set_layout);
    }

    void irenderer::impl::_create_triangle_pipeline()
    {
        depth_target = device->create_texture({
            .initial_payload{},
            .width{1280},
            .height{720},
            .depth{1},
            .mipmap_count{1},
            .flags{texture_flags::RENDER_TARGET},
            .image_format{VK_FORMAT_D32_SFLOAT},
            .name{"DepthTarget"},
        });

        // transition texture
        {
            auto& cmd = device->get_instant_command_buffer();

            cmd.begin();
            cmd.transition_to_depth_image(depth_target);
            cmd.end();

            device->execute_immediate(cmd);
        }

        auto tri_vs_spv = read_spirv("data/pbr/pbr.vx.spv");
        auto tri_fs_spv = read_spirv("data/pbr/pbr.px.spv");

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

        descriptor_set_layout_create_info::binding mesh_binding = {
            .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
            .start_binding{0},
            .binding_count{0},
            .name{"mesh_data_binding"},
        };

        descriptor_set_layout_create_info::binding object_binding = {
            .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
            .start_binding{1},
            .binding_count{0},
            .name{"instance_object_data_binding"},
        };

        descriptor_set_layout_create_info::binding scene_binding = {
            .type{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
            .start_binding{2},
            .binding_count{0},
            .name{"scene_data_binding"},
        };

        descriptor_set_layout_create_info set0_layout_ci = {
            .bindings{mesh_binding, object_binding, scene_binding},
            .binding_count{3},
            .set_index{0},
            .name = {"object_data_set"},
        };

        mesh_data_layout = device->create_descriptor_set_layout(set0_layout_ci);
        mesh_data_set = device->create_descriptor_set(descriptor_set_builder("object_data_set")
                                                          .add_buffer(vertex_buffer_allocator->current_buf, 0)
                                                          .add_buffer(instance_buffer_allocator->current_buf, 1)
                                                          .add_buffer(scene_buffer_allocator->current_buf, 2)
                                                          .set_layout(mesh_data_layout));

        triangle_pipeline = device->create_pipeline({
            .dynamic_render_state{
                dynamic_render_state{
                    .color_format{
                        color_target_format,
                    },
                    .active_color_attachments{1},
                    .depth_format{VK_FORMAT_D32_SFLOAT},
                },
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
                mesh_data_layout,
            },
            .active_desc_layouts{1},
        });
    }

    void irenderer::impl::_create_blit_pipeline()
    {
        {
            color_target = device->create_texture({
                .initial_payload{},
                .width{1280},
                .height{720},
                .depth{1},
                .mipmap_count{1},
                .flags{texture_flags::RENDER_TARGET},
                .image_format{color_target_format},
                .name{"BlitPipeline_ColorSource"},
            });

            // transition texture

            auto& cmd = device->get_instant_command_buffer();

            cmd.begin();
            cmd.transition_to_color_image(color_target);
            cmd.end();

            device->execute_immediate(cmd);
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

        blit_desc_set_layout = device->create_descriptor_set_layout({
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

        blit_pipeline = device->create_pipeline({
            .dynamic_render_state{
                dynamic_render_state{
                    .color_format{device->get_swapchain_attachment_info().color_formats[0]},
                    .active_color_attachments{0},
                },
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
                .name{"blit_shader"},
            },
            .desc_layouts{blit_desc_set_layout},
            .active_desc_layouts{1},
        });

        default_sampler = device->create_sampler({
            .u_address{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
            .v_address{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
            .w_address{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
            .name{"BlitPass_Sampler"},
        });

        blit_desc_set = device->create_descriptor_set(descriptor_set_builder("BlitPass_DescriptorSet")
                                                          .set_layout(blit_desc_set_layout)
                                                          .add_image(color_target, 0)
                                                          .add_sampler(default_sampler, 1));
    }

    void irenderer::impl::_create_mesh_buffers()
    {
        float positions[] = {0.0f, 0.5f, 0.0f, 0.5f, -0.5f, 0.0f, -0.5f, -0.5f, 0.0f};

        auto positions_size = sizeof(float) * 9;
        auto rng = vertex_buffer_allocator->scheme.allocate(positions_size);

        buffer_mapping map_info = {
            .offset = static_cast<std::uint32_t>(rng->start),
            .range = static_cast<std::uint32_t>(rng->end - rng->start),
            .buffer = vertex_buffer_allocator->current_buf,
        };

        void* data = device->map_buffer(map_info);
        std::memcpy(data, positions, rng->end - rng->start);
        device->unmap_buffer(map_info);

        model_matrix = math::transform(math::vec3<float>(0.5f, 0.5f, 1.0f),
                                       math::vec3<float>(0.0f, 0.0f, 1.5707963267f), math::vec3<float>(1.0f));
        proj_matrix = math::perspective(16.0f / 9.0f, 100.0f, 0.01f, 1000.0f);

        auto model_size = sizeof(math::mat4<float>) * 2;
        auto model_rng = instance_buffer_allocator->scheme.allocate(model_size);
        map_info = {
            .offset = static_cast<std::uint32_t>(model_rng->start),
            .range = static_cast<std::uint32_t>(model_rng->end - model_rng->start),
            .buffer = instance_buffer_allocator->current_buf,
        };

        data = device->map_buffer(map_info);
        std::memcpy(data, &model_matrix, sizeof(math::mat4<float>));
        device->unmap_buffer(map_info);

        auto scene_size = sizeof(math::mat4<float>) * 3;
        auto scene_rng = scene_buffer_allocator->scheme.allocate(scene_size);
        map_info = {
            .offset = static_cast<std::uint32_t>(scene_rng->start),
            .range = static_cast<std::uint32_t>(scene_rng->end - scene_rng->start),
            .buffer = scene_buffer_allocator->current_buf,
        };

        auto view_matrix = math::look_at(math::vec3<float>(0.0f, 0.0f, -1.0f), math::vec3<float>(0.0f, 0.0f, 0.0f),
                                         math::vec3<float>(0.0f, 1.0f, 0.0f));
        auto view_proj = proj_matrix * view_matrix;

        math::mat4<float> camera_data[] = {
            proj_matrix,
            view_matrix,
            view_proj,
        };

        data = device->map_buffer(map_info);
        std::memcpy(data, camera_data, sizeof(math::mat4<float>) * 3);
        device->unmap_buffer(map_info);
    }

    buffer_suballocator::buffer_suballocator(const buffer_create_info& initial, gfx_device* dev)
        : device{dev}, ci{initial}, scheme{initial.size}
    {
        buffer_handle handle = device->create_buffer(ci);
        if (!handle)
        {
            logger->critical("Failed to allocate buffer.");
        }

        current_buf = handle;
    }

    void buffer_suballocator::release()
    {
        device->release_buffer(current_buf);
        if (previous_buf)
        {
            device->release_buffer(previous_buf);
        }

        current_buf = {.index{invalid_resource_handle}};
        previous_buf = {.index{invalid_resource_handle}};

        scheme.release_all();
    }

    void buffer_suballocator::reallocate_and_wait(std::size_t new_capacity)
    {
    }
} // namespace tempest::graphics