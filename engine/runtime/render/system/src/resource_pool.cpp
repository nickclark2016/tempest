#include <tempest/render_system/resource_pool.hpp>

#include <cstring>

namespace tempest::render_system
{
    resource_pool::resource_pool(rhi::device& dev, resource_pool_config cfg)
        : _device{&dev}, _cfg{cfg}
    {
        _init_buffers();
        _init_samplers();
    }

    resource_pool::~resource_pool()
    {
        release_all();
    }

    resource_pool::resource_pool(resource_pool&& other) noexcept
        : _device{other._device}, _cfg{other._cfg},
          _vertex_buffer{other._vertex_buffer},
          _mesh_table_buffer{other._mesh_table_buffer},
          _material_table_buffer{other._material_table_buffer},
          _staging_buffer{other._staging_buffer},
          _vertex_bytes_allocated{other._vertex_bytes_allocated},
          _mesh_count{other._mesh_count},
          _material_count{other._material_count},
          _mesh_indices{tempest::move(other._mesh_indices)},
          _mesh_layouts{tempest::move(other._mesh_layouts)},
          _material_indices{tempest::move(other._material_indices)},
          _materials{tempest::move(other._materials)},
          _textures{tempest::move(other._textures)},
          _linear_sampler{other._linear_sampler},
          _point_sampler{other._point_sampler},
          _linear_sampler_descriptor{other._linear_sampler_descriptor},
          _point_sampler_descriptor{other._point_sampler_descriptor},
          _scene_constants_buffer{other._scene_constants_buffer},
          _object_buffer{other._object_buffer},
          _instance_buffer{other._instance_buffer},
          _draw_commands_buffer{other._draw_commands_buffer}
    {
        other._device = nullptr;
        other._vertex_buffer = {};
        other._mesh_table_buffer = {};
        other._material_table_buffer = {};
        other._staging_buffer = {};
        other._linear_sampler = {};
        other._point_sampler = {};
        other._scene_constants_buffer = {};
        other._object_buffer = {};
        other._instance_buffer = {};
        other._draw_commands_buffer = {};
    }

    resource_pool& resource_pool::operator=(resource_pool&& other) noexcept
    {
        if (this != &other)
        {
            release_all();
            _device = other._device;
            _cfg = other._cfg;
            _vertex_buffer = other._vertex_buffer;
            _mesh_table_buffer = other._mesh_table_buffer;
            _material_table_buffer = other._material_table_buffer;
            _staging_buffer = other._staging_buffer;
            _vertex_bytes_allocated = other._vertex_bytes_allocated;
            _mesh_count = other._mesh_count;
            _material_count = other._material_count;
            _mesh_indices = tempest::move(other._mesh_indices);
            _mesh_layouts = tempest::move(other._mesh_layouts);
            _material_indices = tempest::move(other._material_indices);
            _materials = tempest::move(other._materials);
            _textures = tempest::move(other._textures);
            _linear_sampler = other._linear_sampler;
            _point_sampler = other._point_sampler;
            _linear_sampler_descriptor = other._linear_sampler_descriptor;
            _point_sampler_descriptor = other._point_sampler_descriptor;
            _scene_constants_buffer = other._scene_constants_buffer;
            _object_buffer = other._object_buffer;
            _instance_buffer = other._instance_buffer;
            _draw_commands_buffer = other._draw_commands_buffer;

            other._device = nullptr;
            other._vertex_buffer = {};
            other._mesh_table_buffer = {};
            other._material_table_buffer = {};
            other._staging_buffer = {};
            other._linear_sampler = {};
            other._point_sampler = {};
            other._scene_constants_buffer = {};
            other._object_buffer = {};
            other._instance_buffer = {};
            other._draw_commands_buffer = {};
        }
        return *this;
    }

    void resource_pool::_init_buffers()
    {
        if (!_device)
        {
            return;
        }

        // 1. Persistent Device-Local Buffers (VRAM)
        _vertex_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = _cfg.initial_vertex_buffer_size,
            .memory_usage = rhi::memory_usage::device_only,
            .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::index_buffer | rhi::buffer_usage::device_address | rhi::buffer_usage::transfer_dst,
            .name = "VertexPullBuffer",
        });

        _mesh_table_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(mesh_layout) * _cfg.max_mesh_count,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address,
            .name = "MeshTableBuffer",
        });

        _material_table_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(material_payload) * _cfg.max_material_count,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address,
            .name = "MaterialTableBuffer",
        });

        _staging_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = _cfg.staging_buffer_size,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::transfer_src,
            .name = "ResourceStagingBuffer",
        });

        // 2. Dynamic Per-Frame Upload Buffers (Host-Visible / Coherent)
        _scene_constants_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(scene_constants) * _cfg.frames_in_flight,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address,
            .name = "SceneConstantsBuffer",
        });

        _object_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(object_payload) * _cfg.max_object_count,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address,
            .name = "ObjectPayloadBuffer",
        });

        _instance_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(uint32_t) * _cfg.max_instance_count,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address,
            .name = "InstanceBuffer",
        });

        _draw_commands_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(indexed_indirect_command) * _cfg.max_draw_command_count,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::indirect_buffer | rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address,
            .name = "DrawCommandsBuffer",
        });
    }

    void resource_pool::_init_samplers()
    {
        if (!_device)
        {
            return;
        }

        _linear_sampler = _device->create_sampler(rhi::sampler_desc{
            .min_filter = rhi::filter_mode::linear,
            .mag_filter = rhi::filter_mode::linear,
            .mipmap_mode = rhi::mipmap_mode::linear,
            .address_u = rhi::address_mode::repeat,
            .address_v = rhi::address_mode::repeat,
            .address_w = rhi::address_mode::repeat,
            .name = "LinearRepeatSampler",
        });
        _linear_sampler_descriptor = _device->allocate_descriptor(rhi::descriptor_type::sampler);
        _device->write_sampler_descriptor(_linear_sampler_descriptor, _linear_sampler);

        _point_sampler = _device->create_sampler(rhi::sampler_desc{
            .min_filter = rhi::filter_mode::nearest,
            .mag_filter = rhi::filter_mode::nearest,
            .mipmap_mode = rhi::mipmap_mode::nearest,
            .address_u = rhi::address_mode::repeat,
            .address_v = rhi::address_mode::repeat,
            .address_w = rhi::address_mode::repeat,
            .name = "PointRepeatSampler",
        });
        _point_sampler_descriptor = _device->allocate_descriptor(rhi::descriptor_type::sampler);
        _device->write_sampler_descriptor(_point_sampler_descriptor, _point_sampler);
    }

    void resource_pool::load_meshes(span<const guid> mesh_ids, const core::mesh_registry& registry,
                                    render_graph::render_graph& graph)
    {
        struct mesh_upload_pass_data
        {
            render_graph::rg_buffer_id staging;
            render_graph::rg_buffer_id vtx_buf;
            render_graph::rg_buffer_id mesh_table;
        };

        auto newly_loaded_layouts = vector<mesh_layout>{};
        auto staging_byte_offset = size_t{0};
        auto* staging_ptr = static_cast<byte*>(_staging_buffer.cpu_address);

        for (const auto& id : mesh_ids)
        {
            if (_mesh_indices.contains(id))
            {
                continue;
            }

            auto mesh_opt = registry.find(id);
            if (!mesh_opt.has_value())
            {
                continue;
            }

            const auto& m = mesh_opt.value();
            const auto vtx_count = static_cast<uint32_t>(m.vertices.size());
            const auto idx_count = static_cast<uint32_t>(m.indices.size());

            // Vertex layout offsets
            const auto mesh_start = static_cast<uint32_t>(_vertex_bytes_allocated);
            const auto pos_offset = uint32_t{0};
            const auto pos_size = vtx_count * 3 * sizeof(float);

            const auto interleave_offset = pos_offset + pos_size;
            // UV(2) + Normal(3) + Tangent(4) + Color(4)
            const auto stride = static_cast<uint32_t>((2 + 3 + 4 + 4) * sizeof(float));
            const auto interleave_size = vtx_count * stride;

            const auto idx_offset = interleave_offset + interleave_size;
            const auto idx_size = idx_count * sizeof(uint32_t);

            const auto total_mesh_bytes = pos_size + interleave_size + idx_size;

            if (_vertex_bytes_allocated + total_mesh_bytes > _cfg.initial_vertex_buffer_size ||
                staging_byte_offset + total_mesh_bytes > _cfg.staging_buffer_size)
            {
                break; // Buffer capacity exceeded
            }

            // Pack into staging buffer
            auto* current_staging = staging_ptr + staging_byte_offset;

            // 1. Positions
            for (uint32_t i = 0; i < vtx_count; ++i)
            {
                auto* pos_dst = reinterpret_cast<float*>(current_staging + pos_offset + i * 3 * sizeof(float));
                pos_dst[0] = m.vertices[i].position.x;
                pos_dst[1] = m.vertices[i].position.y;
                pos_dst[2] = m.vertices[i].position.z;
            }

            // 2. Interleaved: uv, normal, tangent, color
            for (uint32_t i = 0; i < vtx_count; ++i)
            {
                auto* attr_dst = reinterpret_cast<float*>(current_staging + interleave_offset + i * stride);
                // UV0 (offset 0)
                attr_dst[0] = m.vertices[i].uv.x;
                attr_dst[1] = m.vertices[i].uv.y;
                // Normal (offset 2 * 4 = 8)
                attr_dst[2] = m.vertices[i].normal.x;
                attr_dst[3] = m.vertices[i].normal.y;
                attr_dst[4] = m.vertices[i].normal.z;
                // Tangent (offset 5 * 4 = 20)
                attr_dst[5] = m.vertices[i].tangent.x;
                attr_dst[6] = m.vertices[i].tangent.y;
                attr_dst[7] = m.vertices[i].tangent.z;
                attr_dst[8] = m.vertices[i].tangent.w;
                // Color (offset 9 * 4 = 36)
                attr_dst[9] = m.vertices[i].color.x;
                attr_dst[10] = m.vertices[i].color.y;
                attr_dst[11] = m.vertices[i].color.z;
                attr_dst[12] = m.vertices[i].color.w;
            }

            // 3. Indices
            if (idx_count > 0)
            {
                std::memcpy(current_staging + idx_offset, m.indices.data(), idx_size);
            }

            auto layout = mesh_layout{
                .vertex_buffer_address = _vertex_buffer.gpu_address,
                .mesh_start_offset = mesh_start,
                .positions_offset = pos_offset,
                .interleave_offset = static_cast<uint32_t>(interleave_offset),
                .interleave_stride = stride,
                .uvs_offset = 0,
                .normals_offset = 2 * sizeof(float),
                .tangents_offset = 5 * sizeof(float),
                .color_offset = 9 * sizeof(float),
                .index_offset = static_cast<uint32_t>(idx_offset),
                .index_count = idx_count,
            };

            const auto mesh_idx = _mesh_count++;
            _mesh_indices[id] = mesh_idx;
            _mesh_layouts[id] = layout;
            newly_loaded_layouts.push_back(layout);

            if (_mesh_table_buffer.cpu_address && mesh_idx < _cfg.max_mesh_count)
            {
                static_cast<mesh_layout*>(_mesh_table_buffer.cpu_address)[mesh_idx] = layout;
            }

            _vertex_bytes_allocated += total_mesh_bytes;
            staging_byte_offset += total_mesh_bytes;
        }

        if (staging_byte_offset > 0)
        {
            // Record transfer pass in render graph
            graph.add_transfer_pass<mesh_upload_pass_data>(
                "MeshUploadTransferPass",
                [this](render_graph::pass_builder& builder, mesh_upload_pass_data& data) {
                    data.staging = builder.import_buffer(_staging_buffer);
                    data.vtx_buf = builder.import_buffer(_vertex_buffer);
                    builder.read(data.staging, rhi::pipeline_stage::copy, rhi::resource_access::read);
                    builder.write(data.vtx_buf, rhi::pipeline_stage::copy, rhi::resource_access::write);
                },
                [this, staging_byte_offset]([[maybe_unused]] const mesh_upload_pass_data& data,
                                            [[maybe_unused]] render_graph::pass_execution_context& ctx,
                                            rhi::command_list& cmd) {
                    const auto region = rhi::buffer_copy_region{
                        .src_offset = 0,
                        .dst_offset = _vertex_bytes_allocated - staging_byte_offset,
                        .size = staging_byte_offset,
                    };
                    cmd.copy_buffer(_staging_buffer, _vertex_buffer, span<const rhi::buffer_copy_region>{&region, 1});
                });
        }
    }

    void resource_pool::load_materials(span<const guid> material_ids, const core::material_registry& registry,
                                       [[maybe_unused]] render_graph::render_graph& graph)
    {
        for (const auto& id : material_ids)
        {
            if (_material_indices.contains(id))
            {
                continue;
            }

            auto mat_opt = registry.find(id);
            if (!mat_opt.has_value())
            {
                continue;
            }

            const auto& m = mat_opt.value();
            auto payload = material_payload{};

            if (auto val = m.get_vec4(core::material::base_color_factor_name))
            {
                payload.base_color_factor = *val;
            }
            if (auto val = m.get_vec4(core::material::emissive_factor_name))
            {
                payload.emissive_factor = *val;
            }
            if (auto val = m.get_vec4(core::material::volume_attenuation_color_name))
            {
                payload.attenuation_color = *val;
            }
            if (auto val = m.get_scalar(core::material::metallic_factor_name))
            {
                payload.metallic_factor = *val;
            }
            if (auto val = m.get_scalar(core::material::roughness_factor_name))
            {
                payload.roughness_factor = *val;
            }
            if (auto val = m.get_scalar(core::material::normal_scale_name))
            {
                payload.normal_scale = *val;
            }
            if (auto val = m.get_scalar(core::material::alpha_cutoff_name))
            {
                payload.alpha_cutoff = *val;
            }
            if (auto val = m.get_scalar(core::material::transmissive_factor_name))
            {
                payload.transmission_factor = *val;
            }
            if (auto val = m.get_scalar(core::material::volume_thickness_factor_name))
            {
                payload.thickness_factor = *val;
            }
            if (auto val = m.get_scalar(core::material::volume_attenuation_distance_name))
            {
                payload.attenuation_distance = *val;
            }

            // Bindless texture slot indices
            if (auto tex_id = m.get_texture(core::material::base_color_texture_name))
            {
                payload.base_color_texture_id = get_texture_descriptor_index(*tex_id);
            }
            if (auto tex_id = m.get_texture(core::material::normal_texture_name))
            {
                payload.normal_texture_id = get_texture_descriptor_index(*tex_id);
            }
            if (auto tex_id = m.get_texture(core::material::metallic_roughness_texture_name))
            {
                payload.metallic_roughness_texture_id = get_texture_descriptor_index(*tex_id);
            }
            if (auto tex_id = m.get_texture(core::material::emissive_texture_name))
            {
                payload.emissive_texture_id = get_texture_descriptor_index(*tex_id);
            }
            if (auto tex_id = m.get_texture(core::material::occlusion_texture_name))
            {
                payload.occlusion_texture_id = get_texture_descriptor_index(*tex_id);
            }
            if (auto tex_id = m.get_texture(core::material::transmissive_texture_name))
            {
                payload.transmission_texture_id = get_texture_descriptor_index(*tex_id);
            }
            if (auto tex_id = m.get_texture(core::material::volume_thickness_texture_name))
            {
                payload.thickness_texture_id = get_texture_descriptor_index(*tex_id);
            }

            auto mode_str = m.get_string(core::material::alpha_mode_name).value_or("OPAQUE");
            if (mode_str == "MASK")
            {
                payload.type = material_type::mask;
            }
            else if (mode_str == "BLEND")
            {
                payload.type = material_type::blend;
            }
            else if (mode_str == "TRANSMISSIVE")
            {
                payload.type = material_type::transmissive;
            }
            else
            {
                payload.type = material_type::opaque;
            }

            const auto mat_idx = _material_count++;
            _material_indices[id] = mat_idx;
            _materials[id] = payload;

            if (_material_table_buffer.cpu_address && mat_idx < _cfg.max_material_count)
            {
                static_cast<material_payload*>(_material_table_buffer.cpu_address)[mat_idx] = payload;
            }
        }
    }

    void resource_pool::load_textures(span<const guid> texture_ids, const core::texture_registry& registry,
                                      render_graph::render_graph& graph)
    {
        if (!_device)
        {
            return;
        }

        for (const auto& id : texture_ids)
        {
            if (_textures.contains(id))
            {
                continue;
            }

            auto tex_opt = registry.get_texture(id);
            if (!tex_opt.has_value() || tex_opt->mips.empty())
            {
                continue;
            }

            const auto& t = tex_opt.value();
            auto format = rhi::data_format::rgba8_srgb;
            if (t.format == core::texture_format::rgba8_unorm)
            {
                format = rhi::data_format::rgba8_unorm;
            }
            else if (t.format == core::texture_format::rgba16_unorm)
            {
                format = rhi::data_format::rgba16_float;
            }
            else if (t.format == core::texture_format::rgba32_float)
            {
                format = rhi::data_format::rgba32_float;
            }

            auto tex_handle = _device->create_texture(rhi::texture_desc{
                .width = t.width,
                .height = t.height,
                .depth = 1,
                .mip_levels = static_cast<uint32_t>(t.mips.size()),
                .array_layers = 1,
                .format = format,
                .memory_usage = rhi::memory_usage::device_only,
                .usage = rhi::texture_usage::sampled | rhi::texture_usage::transfer_dst,
                .name = "BindlessTexture",
            });

            auto view_handle = _device->create_texture_view(tex_handle, rhi::texture_view_desc{
                .override_format = format,
                .base_mip_level = 0,
                .mip_level_count = static_cast<uint32_t>(t.mips.size()),
                .base_array_layer = 0,
                .array_layer_count = 1,
            });

            auto desc_handle = _device->allocate_descriptor(rhi::descriptor_type::sampled_image);
            _device->write_sampled_image_descriptor(desc_handle, view_handle, rhi::image_layout::general);

            _textures[id] = texture_entry{
                .texture = tex_handle,
                .view = view_handle,
                .descriptor = desc_handle,
            };

            // Stage and upload top mip
            const auto& top_mip = t.mips[0];
            if (!top_mip.data.empty())
            {
                auto staging = _device->create_buffer(rhi::buffer_desc{
                    .size = top_mip.data.size(),
                    .memory_usage = rhi::memory_usage::upload,
                    .usage = rhi::buffer_usage::transfer_src,
                    .name = "TextureStagingBuffer",
                });
                std::memcpy(staging.cpu_address, top_mip.data.data(), top_mip.data.size());
                _staging_buffers_to_free.push_back(staging);

                struct tex_upload_pass_data
                {
                    render_graph::rg_buffer_id staging;
                    render_graph::rg_texture_id tex;
                };

                graph.add_transfer_pass<tex_upload_pass_data>(
                    "TextureUploadTransferPass",
                    [staging, tex_handle, view_handle](render_graph::pass_builder& builder, tex_upload_pass_data& data) {
                        data.staging = builder.import_buffer(staging);
                        data.tex = builder.import_texture(tex_handle, view_handle, rhi::image_layout::undefined);
                        builder.read(data.staging, rhi::pipeline_stage::copy, rhi::resource_access::read);
                        builder.write(data.tex, rhi::pipeline_stage::copy, rhi::resource_access::write,
                                      rhi::image_layout::general);
                    },
                    [staging, tex_handle, w = t.width, h = t.height]([[maybe_unused]] const tex_upload_pass_data& data,
                                                                 [[maybe_unused]] render_graph::pass_execution_context& ctx,
                                                                 rhi::command_list& cmd) {
                        const auto copy_region = rhi::buffer_texture_copy_region{
                            .buffer_offset = 0,
                            .buffer_row_length = 0,
                            .buffer_image_height = 0,
                            .mip_level = 0,
                            .base_array_layer = 0,
                            .array_layer_count = 1,
                            .image_offset_x = 0,
                            .image_offset_y = 0,
                            .image_offset_z = 0,
                            .image_extent_width = w,
                            .image_extent_height = h,
                            .image_extent_depth = 1,
                        };
                        cmd.copy_buffer_to_texture(staging, tex_handle,
                                                   span<const rhi::buffer_texture_copy_region>{&copy_region, 1});
                    });
            }
        }
    }

    auto resource_pool::get_vertex_buffer_address() const noexcept -> uint64_t
    {
        return _vertex_buffer.gpu_address;
    }

    auto resource_pool::get_mesh_table_address() const noexcept -> uint64_t
    {
        return _mesh_table_buffer.gpu_address;
    }

    auto resource_pool::get_material_table_address() const noexcept -> uint64_t
    {
        return _material_table_buffer.gpu_address;
    }

    auto resource_pool::get_mesh_address(const guid& id) const noexcept -> uint64_t
    {
        auto it = _mesh_indices.find(id);
        if (it != _mesh_indices.end())
        {
            return _mesh_table_buffer.gpu_address + it->second * sizeof(mesh_layout);
        }
        return 0;
    }

    auto resource_pool::get_material_address(const guid& id) const noexcept -> uint64_t
    {
        auto it = _material_indices.find(id);
        if (it != _material_indices.end())
        {
            return _material_table_buffer.gpu_address + it->second * sizeof(material_payload);
        }
        return 0;
    }

    auto resource_pool::get_mesh_layout(const guid& id) const noexcept -> optional<mesh_layout>
    {
        auto it = _mesh_layouts.find(id);
        if (it != _mesh_layouts.end())
        {
            return it->second;
        }
        return nullopt;
    }

    auto resource_pool::get_texture_descriptor_index(const guid& id) const noexcept -> int16_t
    {
        auto it = _textures.find(id);
        if (it != _textures.end())
        {
            return static_cast<int16_t>(it->second.descriptor.index);
        }
        return -1;
    }

    auto resource_pool::get_scene_constants_address() const noexcept -> uint64_t
    {
        return _scene_constants_buffer.gpu_address + static_cast<uint64_t>(_frame_slot) * sizeof(scene_constants);
    }

    auto resource_pool::get_scene_constants_buffer() const noexcept -> rhi::buffer_handle
    {
        return _scene_constants_buffer;
    }

    auto resource_pool::get_object_buffer() const noexcept -> rhi::buffer_handle
    {
        return _object_buffer;
    }

    auto resource_pool::get_instance_buffer() const noexcept -> rhi::buffer_handle
    {
        return _instance_buffer;
    }

    auto resource_pool::get_draw_commands_buffer() const noexcept -> rhi::buffer_handle
    {
        return _draw_commands_buffer;
    }

    void resource_pool::advance_frame() noexcept
    {
        if (_cfg.frames_in_flight > 0)
        {
            _frame_slot = (_frame_slot + 1) % _cfg.frames_in_flight;
        }
    }

    void resource_pool::write_scene_constants(const scene_constants& constants)
    {
        if (_scene_constants_buffer.cpu_address)
        {
            auto* dst = static_cast<scene_constants*>(_scene_constants_buffer.cpu_address) + _frame_slot;
            std::memcpy(dst, &constants, sizeof(constants));
        }
    }

    void resource_pool::write_objects(span<const object_payload> objects)
    {
        if (_object_buffer.cpu_address && !objects.empty())
        {
            const auto count = tempest::min(objects.size(), static_cast<size_t>(_cfg.max_object_count));
            std::memcpy(_object_buffer.cpu_address, objects.data(), count * sizeof(object_payload));
        }
    }

    void resource_pool::write_instances(span<const uint32_t> instances)
    {
        if (_instance_buffer.cpu_address && !instances.empty())
        {
            const auto count = tempest::min(instances.size(), static_cast<size_t>(_cfg.max_instance_count));
            std::memcpy(_instance_buffer.cpu_address, instances.data(), count * sizeof(uint32_t));
        }
    }

    void resource_pool::write_draw_commands(span<const indexed_indirect_command> commands)
    {
        if (_draw_commands_buffer.cpu_address && !commands.empty())
        {
            const auto count = tempest::min(commands.size(), static_cast<size_t>(_cfg.max_draw_command_count));
            std::memcpy(_draw_commands_buffer.cpu_address, commands.data(), count * sizeof(indexed_indirect_command));
        }
    }

    auto resource_pool::get_linear_sampler() const noexcept -> rhi::sampler_handle
    {
        return _linear_sampler;
    }

    auto resource_pool::get_point_sampler() const noexcept -> rhi::sampler_handle
    {
        return _point_sampler;
    }

    auto resource_pool::get_linear_sampler_descriptor() const noexcept -> rhi::descriptor_handle
    {
        return _linear_sampler_descriptor;
    }

    auto resource_pool::get_point_sampler_descriptor() const noexcept -> rhi::descriptor_handle
    {
        return _point_sampler_descriptor;
    }

    void resource_pool::clear_staging_buffers()
    {
        if (_device)
        {
            for (auto& buf : _staging_buffers_to_free)
            {
                if (buf.handle != 0)
                {
                    _device->destroy_buffer(buf);
                }
            }
        }
        _staging_buffers_to_free.clear();
    }

    void resource_pool::release_all()
    {
        clear_staging_buffers();

        if (!_device)
        {
            return;
        }

        for (auto& [_, entry] : _textures)
        {
            if (entry.descriptor.index != ~0U)
            {
                _device->free_descriptor(rhi::descriptor_type::sampled_image, entry.descriptor);
            }
            if (entry.view.handle != 0)
            {
                _device->destroy_texture_view(entry.view);
            }
            if (entry.texture.handle != 0)
            {
                _device->destroy_texture(entry.texture);
            }
        }
        _textures.clear();

        if (_linear_sampler_descriptor.index != ~0U)
        {
            _device->free_descriptor(rhi::descriptor_type::sampler, _linear_sampler_descriptor);
            _linear_sampler_descriptor = {};
        }
        if (_point_sampler_descriptor.index != ~0U)
        {
            _device->free_descriptor(rhi::descriptor_type::sampler, _point_sampler_descriptor);
            _point_sampler_descriptor = {};
        }

        if (_linear_sampler.handle != 0)
        {
            _device->destroy_sampler(_linear_sampler);
            _linear_sampler = {};
        }
        if (_point_sampler.handle != 0)
        {
            _device->destroy_sampler(_point_sampler);
            _point_sampler = {};
        }

        auto destroy_buf = [this](rhi::buffer_handle& buf) {
            if (buf.handle != 0)
            {
                _device->destroy_buffer(buf);
                buf = {};
            }
        };

        destroy_buf(_vertex_buffer);
        destroy_buf(_mesh_table_buffer);
        destroy_buf(_material_table_buffer);
        destroy_buf(_staging_buffer);
        destroy_buf(_scene_constants_buffer);
        destroy_buf(_object_buffer);
        destroy_buf(_instance_buffer);
        destroy_buf(_draw_commands_buffer);
    }
} // namespace tempest::render_system
