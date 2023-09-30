#include <tempest/render_system.hpp>

#include "device.hpp"
#include "passes/blit_pass.hpp"
#include "passes/simple_triangle_pass.hpp"
#include "passes/visbuffer_pass.hpp"

#include <tempest/logger.hpp>
#include <tempest/mesh_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec3.hpp>

#include <optional>

namespace tempest::graphics
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::render_system"}});
    }

    struct buffer_resource_suballocator
    {
        gfx_device* device{nullptr};
        buffer_handle current_buf{.index{invalid_resource_handle}};
        buffer_handle previous_buf{.index{invalid_resource_handle}};
        buffer_create_info ci{};

        explicit buffer_resource_suballocator(const buffer_create_info& initial, gfx_device* dev);

        void release();
        void reallocate_and_wait(std::size_t new_capacity);
        core::range<std::size_t> allocate(std::size_t len);

        core::best_fit_scheme<std::size_t> scheme;
    };

    class render_system::render_system_impl
    {
      public:
        render_system_impl(const core::version& ver, iwindow& win, core::allocator& allocator);
        ~render_system_impl();

        void render();
        mesh_layout upload_mesh(const core::mesh_view& mesh);

      private:
        core::version _ver;
        iwindow& _win;
        core::allocator& _alloc;

        std::optional<gfx_device> _device;

        // Mesh
        std::optional<buffer_resource_suballocator> _vertex_buffer_allocator;
        std::optional<buffer_resource_suballocator> _mesh_buffer_allocator;
        std::optional<buffer_resource_suballocator> _instance_buffer_allocator;
        std::optional<buffer_resource_suballocator> _scene_buffer_allocator;
        descriptor_set_layout_handle _mesh_data_layout{};
        descriptor_set_handle _mesh_data_set;

        // Render Target
        texture_handle _default_depth_buffer;
        VkFormat _depth_format{VK_FORMAT_D32_SFLOAT};
        VkFormat _color_format{VK_FORMAT_R8G8B8A8_SRGB};

        // Passes
        blit_pass _blit;
        simple_triangle_pass _triangle;
        visibility_buffer_pass _visbuffer;

        // Helpers
        void _initialize_mesh_data();
        void _release_mesh_data();
        void _populate_mesh_data();
        void _initialize_depth_buffer();
        void _release_depth_buffer();
    };

    render_system::render_system(const core::version& ver, iwindow& win, core::allocator& allocator)
        : _impl{std::make_unique<render_system_impl>(ver, win, allocator)}
    {
    }

    render_system::~render_system() = default;

    void render_system::render()
    {
        _impl->render();
    }

    mesh_layout render_system::upload_mesh(const core::mesh_view& mesh)
    {
        return _impl->upload_mesh(mesh);
    }

    buffer_resource_suballocator::buffer_resource_suballocator(const buffer_create_info& initial, gfx_device* dev)
        : device{dev}, ci{initial}, scheme{initial.size}
    {
        buffer_handle handle = device->create_buffer(ci);
        if (!handle)
        {
            logger->critical("Failed to allocate buffer.");
        }

        current_buf = handle;
    }

    void buffer_resource_suballocator::release()
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

    void buffer_resource_suballocator::reallocate_and_wait(std::size_t new_capacity)
    {
        auto new_buffer_ci = ci;
        new_buffer_ci.size = static_cast<std::uint32_t>(new_capacity);

        auto new_buffer = device->create_buffer(ci);

        auto& cmds = device->get_instant_command_buffer();

        cmds.begin();

        buffer_copy_region region{
            .src_offset{0},
            .dst_offset{0},
            .len_bytes{ci.size},
        };

        buffer_copy_region regions[] = {region};

        cmds.copy_buffer(current_buf, new_buffer, regions);
        cmds.end();

        device->execute_immediate(cmds);
    }

    core::range<std::size_t> buffer_resource_suballocator::allocate(std::size_t len)
    {
        auto result = scheme.allocate(len);
        if (!result)
        {
            // reallocate buffer, extend for len
            auto current_len = scheme.max_extent() - scheme.min_extent();
            auto requested_len = current_len + len;
            reallocate_and_wait(requested_len);
            result = scheme.allocate(len);
        }
        return *result;
    }

    render_system::render_system_impl::render_system_impl(const core::version& ver, iwindow& win,
                                                          core::allocator& allocator)
        : _ver{ver}, _win{win}, _alloc{allocator}
    {
        gfx_device_create_info create_info = {
            .global_allocator{&allocator},
            .win{reinterpret_cast<glfw::window*>(&win)},
#ifdef _DEBUG
            .enable_debug{true},
#else
            .enable_debug{false}
#endif
        };

        _device.emplace(create_info);

        _vertex_buffer_allocator.emplace(
            buffer_create_info{
                .type{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
                .usage{resource_usage::DYNAMIC},
                .size{1024 * 1024 * 32},
                .name{"mesh_buffer"},
            },
            &*_device); // 32 mb initial allocation

        _instance_buffer_allocator.emplace(
            buffer_create_info{
                .type{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
                .usage{resource_usage::DYNAMIC},
                .size{1024 * 1024 * 32},
                .name{"instance_data_buffer"},
            },
            &*_device);

        _scene_buffer_allocator.emplace(
            buffer_create_info{
                .type{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
                .usage{resource_usage::STREAM},
                .size{1024 * 64 * 3},
                .name{"scene_data_buffer"},
            },
            &*_device);

        _mesh_buffer_allocator.emplace(
            buffer_create_info{
                .type{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
                .usage{resource_usage::DYNAMIC},
                .size{sizeof(mesh_layout) * 1024},
                .name{"mesh_buffer_layout"},
            },
            &*_device);

        _initialize_mesh_data();
        _initialize_depth_buffer();
        _populate_mesh_data();

        _blit.initialize(*_device, 1280, 720, _color_format);
        _triangle.initialize(*_device, _color_format, _depth_format, _mesh_data_layout);
        _visbuffer.initialize(*_device, 1280, 720);
    }

    render_system::render_system_impl::~render_system_impl()
    {
        _triangle.release(*_device);
        _blit.release(*_device);
        _visbuffer.release(*_device);

        _release_depth_buffer();
        _release_mesh_data();

        _vertex_buffer_allocator->release();
        _mesh_buffer_allocator->release();
        _instance_buffer_allocator->release();
        _scene_buffer_allocator->release();
    }

    void render_system::render_system_impl::render()
    {
        _device->start_frame();

        state_transition_descriptor prepare_render_transitions[] = {
            state_transition_descriptor{
                .texture{_blit.blit_src},
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
                .texture{_blit.blit_src},
                .first_mip{0},
                .mip_count{1},
                .base_layer{0},
                .layer_count{1},
                .src_state{resource_state::RENDER_TARGET},
                .dst_state{resource_state::TRANSFER_SRC},
            },
        };

        state_transition_descriptor prepare_pre_present_transitions[] = {
            state_transition_descriptor{
                .texture{_device->get_current_swapchain_texture()},
                .first_mip{0},
                .mip_count{1},
                .base_layer{0},
                .layer_count{1},
                .src_state{resource_state::UNDEFINED},
                .dst_state{resource_state::TRANSFER_DST},
            },
        };

        state_transition_descriptor prepare_present_transitions[] = {
            state_transition_descriptor{
                .texture{_device->get_current_swapchain_texture()},
                .first_mip{0},
                .mip_count{1},
                .base_layer{0},
                .layer_count{1},
                .src_state{resource_state::TRANSFER_DST},
                .dst_state{resource_state::PRESENT},
            },
        };
        auto& cmds = _device->get_command_buffer(queue_type::GRAPHICS, false);

        cmds.begin();

        cmds.transition_resource(prepare_pre_present_transitions, pipeline_stage::TOP, pipeline_stage::TRANSFER)
            .transition_resource(prepare_render_transitions, pipeline_stage::FRAGMENT_SHADER,
                                 pipeline_stage::FRAMEBUFFER_OUTPUT);

        _triangle.record(cmds, _blit.blit_src, _default_depth_buffer, {0, 0, 1280, 720}, _mesh_data_set);

        cmds.transition_resource(prepare_blit_transitions, pipeline_stage::FRAMEBUFFER_OUTPUT,
                                 pipeline_stage::TRANSFER);

        // TODO: Fetch swapchain size from swapchain
        _blit.record(cmds, _device->get_current_swapchain_texture(), {0, 0, 1280, 720});
        cmds.transition_resource(prepare_present_transitions, pipeline_stage::TRANSFER, pipeline_stage::END);

        cmds.end();
        _device->queue_command_buffer(cmds);
        _device->end_frame();
    }

    mesh_layout render_system::render_system_impl::upload_mesh(const core::mesh_view& mesh)
    {
        auto buffer_region = _vertex_buffer_allocator->allocate(mesh.size_bytes());

        std::size_t start_index = buffer_region.start;

        std::uint32_t interleave_size = 0;

        interleave_size += 2 * sizeof(float);
        interleave_size += 3 * sizeof(float);

        std::uint32_t tangents_offset = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t bitangents_offset = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t colors_offset = std::numeric_limits<std::uint32_t>::max();

        if (mesh.has_tangents)
        {
            tangents_offset = interleave_size;
            interleave_size += 3 * sizeof(float);
        }

        if (mesh.has_bitangents)
        {
            bitangents_offset = interleave_size;
            interleave_size += 3 * sizeof(float);
        }

        if (mesh.has_colors)
        {
            colors_offset = interleave_size;
            interleave_size = 4 * sizeof(float);
        }

        auto interleave_offset = static_cast<std::uint32_t>((mesh.vertices.size() * 3 * sizeof(float)));

        mesh_layout layout = {
            .mesh_start_offset{static_cast<std::uint32_t>(buffer_region.start)},
            .positions_offset{0},
            .interleave_offset{interleave_offset},
            .interleave_stride{interleave_size},
            .uvs_offset{0},
            .normals_offset{2 * sizeof(float)},
            .tangents_offset{tangents_offset},
            .bitangents_offset{bitangents_offset},
            .color_offset{colors_offset},
            .index_offset{interleave_offset + interleave_size * static_cast<std::uint32_t>(mesh.vertices.size())},
            .index_count{static_cast<std::uint32_t>(mesh.indices.size())},
        };

        // for now, sync buffer upload
        {
            buffer_mapping mesh_buffer_map_info{
                .offset{static_cast<std::uint32_t>(buffer_region.start)},
                .range{static_cast<std::uint32_t>(buffer_region.end - buffer_region.start)},
                .buffer{_vertex_buffer_allocator->current_buf},
            };

            void* ptr = _device->map_buffer(mesh_buffer_map_info);
            float* positions = reinterpret_cast<float*>(ptr);
            float* interleave = positions + 3 * mesh.vertices.size();

            // fill positions
            for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
            {
                const auto& vertex = mesh.vertices[i];

                positions[3 * i + 0] = vertex.position[0];
                positions[3 * i + 1] = vertex.position[1];
                positions[3 * i + 2] = vertex.position[2];
            }

            // fill uvs
            for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
            {
                const auto& vertex = mesh.vertices[i];

                std::size_t uv_start = (i * interleave_size + layout.uvs_offset) / sizeof(float);
                interleave[uv_start + 0] = vertex.uv[0];
                interleave[uv_start + 1] = vertex.uv[1];
            }

            // fill normals
            for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
            {
                const auto& vertex = mesh.vertices[i];

                std::size_t normal_start = (i * interleave_size + layout.normals_offset) / sizeof(float);
                interleave[normal_start + 0] = vertex.normal[0];
                interleave[normal_start + 1] = vertex.normal[1];
                interleave[normal_start + 2] = vertex.normal[2];
            }

            // fill tangents
            if (mesh.has_tangents)
            {
                for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
                {
                    const auto& vertex = mesh.vertices[i];

                    std::size_t tangent_start = (i * interleave_size + layout.tangents_offset) / sizeof(float);
                    interleave[tangent_start + 0] = vertex.tangent[0];
                    interleave[tangent_start + 1] = vertex.tangent[1];
                    interleave[tangent_start + 2] = vertex.tangent[2];
                }
            }

            // fill bitangents
            if (mesh.has_bitangents)
            {
                for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
                {
                    const auto& vertex = mesh.vertices[i];

                    std::size_t bitangent_start = (i * interleave_size + layout.bitangents_offset) / sizeof(float);
                    interleave[bitangent_start + 0] = vertex.bitangent[0];
                    interleave[bitangent_start + 1] = vertex.bitangent[1];
                    interleave[bitangent_start + 2] = vertex.bitangent[2];
                }
            }

            // fill tangents
            if (mesh.has_colors)
            {
                for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
                {
                    const auto& vertex = mesh.vertices[i];

                    std::size_t color_start = (i * interleave_size + layout.color_offset) / sizeof(float);
                    interleave[color_start + 0] = vertex.color[0];
                    interleave[color_start + 1] = vertex.color[1];
                    interleave[color_start + 2] = vertex.color[2];
                    interleave[color_start + 3] = vertex.color[3];
                }
            }

            // fill indices
            std::uint32_t* indices =
                reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::byte*>(ptr) + layout.index_offset);
            std::memcpy(indices, mesh.indices.data(), mesh.indices.size_bytes());

            _device->unmap_buffer(mesh_buffer_map_info);
        }

        {
            auto mesh_buffer_region = _mesh_buffer_allocator->allocate(sizeof(mesh_layout));

            buffer_mapping mesh_data_buffer_map_info{
                .offset{static_cast<std::uint32_t>(mesh_buffer_region.start)},
                .range{static_cast<std::uint32_t>(mesh_buffer_region.end - mesh_buffer_region.start)},
                .buffer{_mesh_buffer_allocator->current_buf},
            };

            void* ptr = _device->map_buffer(mesh_data_buffer_map_info);
            std::memcpy(ptr, &layout, sizeof(mesh_layout));
            _device->unmap_buffer(mesh_data_buffer_map_info);
        }

        return layout;
    }

    void render_system::render_system_impl::_initialize_mesh_data()
    {
        descriptor_set_layout_create_info::binding mesh_binding = {
            .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
            .start_binding{0},
            .binding_count{0},
            .name{"mesh_data_binding"},
        };

        descriptor_set_layout_create_info::binding mesh_layout_binding = {
            .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
            .start_binding{1},
            .binding_count{0},
            .name{"mesh_layout_binding"},
        };

        descriptor_set_layout_create_info::binding object_binding = {
            .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
            .start_binding{2},
            .binding_count{0},
            .name{"instance_object_data_binding"},
        };

        descriptor_set_layout_create_info::binding scene_binding = {
            .type{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
            .start_binding{3},
            .binding_count{0},
            .name{"scene_data_binding"},
        };

        descriptor_set_layout_create_info set0_layout_ci = {
            .bindings{mesh_binding, mesh_layout_binding, object_binding, scene_binding},
            .binding_count{4},
            .set_index{0},
            .name = {"object_data_set"},
        };

        _mesh_data_layout = _device->create_descriptor_set_layout(set0_layout_ci);
        _mesh_data_set = _device->create_descriptor_set(descriptor_set_builder("object_data_set")
                                                            .add_buffer(_vertex_buffer_allocator->current_buf, 0)
                                                            .add_buffer(_mesh_buffer_allocator->current_buf, 1)
                                                            .add_buffer(_instance_buffer_allocator->current_buf, 2)
                                                            .add_buffer(_scene_buffer_allocator->current_buf, 3)
                                                            .set_layout(_mesh_data_layout));
    }

    void render_system::render_system_impl::_release_mesh_data()
    {
        _device->release_descriptor_set(_mesh_data_set);
        _device->release_descriptor_set_layout(_mesh_data_layout);
    }

    void render_system::render_system_impl::_populate_mesh_data()
    {
        std::uint32_t indices[] = {0, 1, 2};

        std::array<core::vertex, 3> vertices = {
            core::vertex{
                .position{0.0f, 0.5f, 0.0f},
                .uv{0.0f, 0.0f},
                .normal{0.0f, 0.0f, 1.0f},
                .color{1.0f, 0.0f, 0.0f, 1.0f},
            },
            core::vertex{
                .position{0.5f, -0.5f, 0.0f},
                .uv{0.0f, 0.0f},
                .normal{0.0f, 0.0f, 1.0f},
                .color{0.0f, 1.0f, 0.0f, 1.0f},
            },
            core::vertex{
                .position{-0.5f, -0.5f, 0.0f},
                .uv{0.0f, 0.0f},
                .normal{0.0f, 0.0f, 1.0f},
                .color{0.0f, 0.0f, 1.0f, 1.0f},
            },
        };

        const core::mesh_view triangle{
            .vertices{vertices},
            .indices{indices},
            .has_tangents{false},
            .has_bitangents{false},
            .has_colors{true},
        };

        upload_mesh(triangle);

        object_payload object;

        object.transform = math::transform(math::vec3<float>(0.5f, 0.0f, 1.0f),
                                           math::vec3<float>(0.0f, 0.0f, 1.5707963267f / 2.0f), math::vec3<float>(1.0f));
        object.mesh_id = 1;

        auto proj_matrix = math::perspective(16.0f / 9.0f, 100.0f, 0.01f, 1000.0f);

        auto model_size = sizeof(math::mat4<float>) * 2;
        auto model_rng = _instance_buffer_allocator->scheme.allocate(model_size);
        buffer_mapping map_info = {
            .offset = static_cast<std::uint32_t>(model_rng->start),
            .range = static_cast<std::uint32_t>(model_rng->end - model_rng->start),
            .buffer = _instance_buffer_allocator->current_buf,
        };

        void* data = _device->map_buffer(map_info);
        std::memcpy(data, &object, sizeof(object_payload));
        _device->unmap_buffer(map_info);

        auto scene_size = sizeof(math::mat4<float>) * 3;
        auto scene_rng = _scene_buffer_allocator->scheme.allocate(scene_size);
        map_info = {
            .offset = static_cast<std::uint32_t>(scene_rng->start),
            .range = static_cast<std::uint32_t>(scene_rng->end - scene_rng->start),
            .buffer = _scene_buffer_allocator->current_buf,
        };

        auto view_matrix = math::look_at(math::vec3<float>(0.0f, 0.0f, -4.0f), math::vec3<float>(0.0f, 0.0f, 0.0f),
                                         math::vec3<float>(0.0f, 1.0f, 0.0f));
        auto view_proj = proj_matrix * view_matrix;

        math::mat4<float> camera_data[] = {
            proj_matrix,
            view_matrix,
            view_proj,
        };

        data = _device->map_buffer(map_info);
        std::memcpy(data, camera_data, sizeof(math::mat4<float>) * 3);
        _device->unmap_buffer(map_info);
    }

    void render_system::render_system_impl::_initialize_depth_buffer()
    {
        _default_depth_buffer = _device->create_texture({
            .initial_payload{},
            .width{1280},
            .height{720},
            .depth{1},
            .mipmap_count{1},
            .flags{texture_flags::RENDER_TARGET},
            .image_format{_depth_format},
            .name{"DepthTarget"},
        });

        {
            auto& cmd = _device->get_instant_command_buffer();

            cmd.begin();
            cmd.transition_to_depth_image(_default_depth_buffer);
            cmd.end();

            _device->execute_immediate(cmd);
        }
    }

    void render_system::render_system_impl::_release_depth_buffer()
    {
        _device->release_texture(_default_depth_buffer);
    }
} // namespace tempest::graphics