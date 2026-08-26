#include <tempest/render_system/resource_pool.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/bit.hpp>
#include <cstring>

namespace tempest::render_system
{
    namespace
    {
        void generate_mip_chain_blit_fallback(rhi::command_list& cmd, rhi::texture_handle texture,
                                              uint32_t width, uint32_t height,
                                              uint32_t first_generated_mip, uint32_t total_texture_mips,
                                              uint32_t array_layers = 1)
        {
            if (first_generated_mip >= total_texture_mips || first_generated_mip == 0)
            {
                return;
            }

            for (uint32_t dst_mip = first_generated_mip; dst_mip < total_texture_mips; ++dst_mip)
            {
                const auto src_mip = dst_mip - 1;
                const auto src_w = tempest::max(1u, width >> src_mip);
                const auto src_h = tempest::max(1u, height >> src_mip);
                const auto dst_w = tempest::max(1u, width >> dst_mip);
                const auto dst_h = tempest::max(1u, height >> dst_mip);

                const auto blit_reg = rhi::texture_blit_region{
                    .src_subresource = {
                        .mip_level = src_mip,
                        .base_array_layer = 0,
                        .array_layer_count = array_layers,
                    },
                    .src_offsets = {
                        rhi::offset_3d{0, 0, 0},
                        rhi::offset_3d{static_cast<int32_t>(src_w), static_cast<int32_t>(src_h), 1},
                    },
                    .dst_subresource = {
                        .mip_level = dst_mip,
                        .base_array_layer = 0,
                        .array_layer_count = array_layers,
                    },
                    .dst_offsets = {
                        rhi::offset_3d{0, 0, 0},
                        rhi::offset_3d{static_cast<int32_t>(dst_w), static_cast<int32_t>(dst_h), 1},
                    },
                };

                cmd.blit_texture(texture, texture, span<const rhi::texture_blit_region>{&blit_reg, 1},
                                 rhi::filter_mode::linear);

                if (dst_mip + 1 < total_texture_mips)
                {
                    const auto mip_barrier = rhi::texture_barrier{
                        .texture = texture,
                        .src = {
                            .stages = rhi::pipeline_stage::blit,
                            .access = rhi::resource_access::write,
                            .layout = rhi::image_layout::general,
                        },
                        .dst = {
                            .stages = rhi::pipeline_stage::blit,
                            .access = rhi::resource_access::read,
                            .layout = rhi::image_layout::general,
                        },
                        .base_mip_level = dst_mip,
                        .mip_level_count = 1,
                        .base_array_layer = 0,
                        .array_layer_count = array_layers,
                    };
                    cmd.pipeline_barrier(span<const rhi::texture_barrier>{&mip_barrier, 1}, {});
                }
            }

            // Post-barrier: flush all blit writes across generated mips for subsequent shader reading
            const auto post_barrier = rhi::texture_barrier{
                .texture = texture,
                .src = {
                    .stages = rhi::pipeline_stage::blit,
                    .access = rhi::resource_access::write,
                    .layout = rhi::image_layout::general,
                },
                .dst = {
                    .stages = rhi::pipeline_stage::all_graphics | rhi::pipeline_stage::compute,
                    .access = rhi::resource_access::read,
                    .layout = rhi::image_layout::general,
                },
                .base_mip_level = first_generated_mip,
                .mip_level_count = total_texture_mips - first_generated_mip,
                .base_array_layer = 0,
                .array_layer_count = array_layers,
            };
            cmd.pipeline_barrier(span<const rhi::texture_barrier>{&post_barrier, 1}, {});
        }
    } // namespace

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
          _directional_shadow_buffer{other._directional_shadow_buffer},
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
        other._directional_shadow_buffer = {};
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
            _directional_shadow_buffer = other._directional_shadow_buffer;
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
            other._directional_shadow_buffer = {};
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

        _directional_shadow_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(directional_shadow_data) * _cfg.frames_in_flight,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::uniform_buffer | rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address | rhi::buffer_usage::transfer_dst,
            .name = "DirectionalShadowBuffer",
        });

        _object_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(object_payload) * _cfg.max_object_count * _cfg.frames_in_flight,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address,
            .name = "ObjectPayloadBuffer",
        });

        _instance_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(uint32_t) * _cfg.max_instance_count * _cfg.frames_in_flight,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address,
            .name = "InstanceBuffer",
        });

        _draw_commands_buffer = _device->create_buffer(rhi::buffer_desc{
            .size = sizeof(indexed_indirect_command) * _cfg.max_draw_command_count * _cfg.frames_in_flight,
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
                                      render_graph::render_graph& graph, mipmap_generation_mode mip_mode)
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

            const auto full_mips = (t.width > 0 && t.height > 0)
                                       ? static_cast<uint32_t>(tempest::bit_width(tempest::min(t.width, t.height)))
                                       : 1u;
            const auto full_mip_count = tempest::max(1u, full_mips);
            const auto asset_mips_count = static_cast<uint32_t>(t.mips.size());

            uint32_t total_texture_mips = asset_mips_count;
            uint32_t mips_to_upload = asset_mips_count;
            uint32_t first_generated_mip = asset_mips_count;
            bool generate_mips = false;

            switch (mip_mode)
            {
            case mipmap_generation_mode::none:
                total_texture_mips = asset_mips_count;
                mips_to_upload = asset_mips_count;
                generate_mips = false;
                first_generated_mip = total_texture_mips;
                break;
            case mipmap_generation_mode::if_missing:
                if (asset_mips_count < full_mip_count)
                {
                    total_texture_mips = full_mip_count;
                    mips_to_upload = asset_mips_count;
                    generate_mips = true;
                    first_generated_mip = asset_mips_count;
                }
                else
                {
                    total_texture_mips = asset_mips_count;
                    mips_to_upload = asset_mips_count;
                    generate_mips = false;
                    first_generated_mip = total_texture_mips;
                }
                break;
            case mipmap_generation_mode::force:
                total_texture_mips = full_mip_count;
                mips_to_upload = 1;
                generate_mips = full_mip_count > 1;
                first_generated_mip = 1;
                break;
            }

            auto tex_handle = _device->create_texture(rhi::texture_desc{
                .width = t.width,
                .height = t.height,
                .depth = 1,
                .mip_levels = total_texture_mips,
                .array_layers = 1,
                .format = format,
                .memory_usage = rhi::memory_usage::device_only,
                .usage = rhi::texture_usage::sampled | rhi::texture_usage::transfer_dst |
                         (generate_mips ? rhi::texture_usage::transfer_src : rhi::texture_usage::none),
                .name = t.name.empty() ? "BindlessTexture" : t.name.c_str(),
            });

            auto view_handle = _device->create_texture_view(tex_handle, rhi::texture_view_desc{
                .override_format = format,
                .base_mip_level = 0,
                .mip_level_count = total_texture_mips,
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

            // Calculate staging size and copy existing mips to staging buffer
            size_t total_mip_bytes = 0;
            auto mip_sizes = vector<size_t>{};
            mip_sizes.reserve(mips_to_upload);
            for (uint32_t i = 0; i < mips_to_upload; ++i)
            {
                const auto sz = t.mips[i].data.size();
                mip_sizes.push_back(sz);
                total_mip_bytes += sz;
            }

            if (total_mip_bytes > 0)
            {
                auto staging = _device->create_buffer(rhi::buffer_desc{
                    .size = total_mip_bytes,
                    .memory_usage = rhi::memory_usage::upload,
                    .usage = rhi::buffer_usage::transfer_src,
                    .name = "TextureStagingBuffer",
                });

                auto* staging_ptr = static_cast<byte*>(staging.cpu_address);
                size_t staging_offset = 0;
                for (uint32_t i = 0; i < mips_to_upload; ++i)
                {
                    if (!t.mips[i].data.empty())
                    {
                        std::memcpy(staging_ptr + staging_offset, t.mips[i].data.data(), t.mips[i].data.size());
                        staging_offset += t.mips[i].data.size();
                    }
                }
                _staging_buffers_to_free.push_back(staging);

                struct tex_upload_pass_data
                {
                    render_graph::rg_buffer_id staging;
                    render_graph::rg_texture_id tex;
                };

                graph.add_graphics_pass<tex_upload_pass_data>(
                    "TextureUploadPass",
                    [staging, tex_handle, view_handle, generate_mips](render_graph::pass_builder& builder,
                                                                      tex_upload_pass_data& data) {
                        data.staging = builder.import_buffer(staging);
                        data.tex = builder.import_texture(tex_handle, view_handle, rhi::image_layout::undefined);
                        builder.read(data.staging, rhi::pipeline_stage::copy, rhi::resource_access::read);
                        auto write_stages = enum_mask{rhi::pipeline_stage::copy};
                        if (generate_mips)
                        {
                            write_stages |= rhi::pipeline_stage::blit;
                        }
                        builder.write(data.tex, write_stages, rhi::resource_access::write,
                                      rhi::image_layout::general);
                    },
                    [staging, tex_handle, w = t.width, h = t.height, mips_to_upload, generate_mips,
                     first_generated_mip, total_texture_mips, mip_sizes](
                        [[maybe_unused]] const tex_upload_pass_data& data,
                        [[maybe_unused]] render_graph::pass_execution_context& ctx,
                        rhi::command_list& cmd) {
                        auto copy_regions = vector<rhi::buffer_texture_copy_region>{};
                        copy_regions.reserve(mips_to_upload);
                        uint64_t current_buf_offset = 0;
                        for (uint32_t i = 0; i < mips_to_upload; ++i)
                        {
                            const auto mip_w = tempest::max(1u, w >> i);
                            const auto mip_h = tempest::max(1u, h >> i);
                            copy_regions.push_back(rhi::buffer_texture_copy_region{
                                .buffer_offset = current_buf_offset,
                                .buffer_row_length = 0,
                                .buffer_image_height = 0,
                                .mip_level = i,
                                .base_array_layer = 0,
                                .array_layer_count = 1,
                                .image_offset_x = 0,
                                .image_offset_y = 0,
                                .image_offset_z = 0,
                                .image_extent_width = mip_w,
                                .image_extent_height = mip_h,
                                .image_extent_depth = 1,
                            });
                            current_buf_offset += mip_sizes[i];
                        }

                        cmd.copy_buffer_to_texture(staging, tex_handle, copy_regions);

                        if (generate_mips && first_generated_mip < total_texture_mips)
                        {
                            generate_mip_chain_blit_fallback(cmd, tex_handle, w, h, first_generated_mip,
                                                             total_texture_mips);
                        }
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

    auto resource_pool::get_material_type(const guid& id) const noexcept -> optional<material_type>
    {
        auto it = _materials.find(id);
        if (it != _materials.end())
        {
            return it->second.type;
        }
        return nullopt;
    }

    auto resource_pool::get_material(const guid& id) const noexcept -> optional<material_payload>
    {
        auto it = _materials.find(id);
        if (it != _materials.end())
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

    auto resource_pool::get_directional_shadow_address() const noexcept -> uint64_t
    {
        return _directional_shadow_buffer.gpu_address + static_cast<uint64_t>(_frame_slot) * sizeof(directional_shadow_data);
    }

    auto resource_pool::get_object_buffer_address() const noexcept -> uint64_t
    {
        return _object_buffer.gpu_address +
               static_cast<uint64_t>(_frame_slot) * sizeof(object_payload) * _cfg.max_object_count;
    }

    auto resource_pool::get_instance_buffer_address() const noexcept -> uint64_t
    {
        return _instance_buffer.gpu_address +
               static_cast<uint64_t>(_frame_slot) * sizeof(uint32_t) * _cfg.max_instance_count;
    }

    auto resource_pool::get_draw_commands_buffer_offset() const noexcept -> uint64_t
    {
        return static_cast<uint64_t>(_frame_slot) * sizeof(indexed_indirect_command) * _cfg.max_draw_command_count;
    }

    auto resource_pool::get_scene_constants_buffer() const noexcept -> rhi::buffer_handle
    {
        return _scene_constants_buffer;
    }

    auto resource_pool::get_directional_shadow_buffer() const noexcept -> rhi::buffer_handle
    {
        return _directional_shadow_buffer;
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

    void resource_pool::write_directional_shadow_data(const directional_shadow_data& data)
    {
        if (_directional_shadow_buffer.cpu_address)
        {
            auto* dst = static_cast<directional_shadow_data*>(_directional_shadow_buffer.cpu_address) + _frame_slot;
            std::memcpy(dst, &data, sizeof(data));
        }
    }

    void resource_pool::write_objects(span<const object_payload> objects)
    {
        if (_object_buffer.cpu_address && !objects.empty())
        {
            const auto count = tempest::min(objects.size(), static_cast<size_t>(_cfg.max_object_count));
            auto* dst = static_cast<object_payload*>(_object_buffer.cpu_address) +
                        static_cast<size_t>(_frame_slot) * _cfg.max_object_count;
            std::memcpy(dst, objects.data(), count * sizeof(object_payload));
        }
    }

    void resource_pool::write_instances(span<const uint32_t> instances)
    {
        if (_instance_buffer.cpu_address && !instances.empty())
        {
            const auto count = tempest::min(instances.size(), static_cast<size_t>(_cfg.max_instance_count));
            auto* dst = static_cast<uint32_t*>(_instance_buffer.cpu_address) +
                        static_cast<size_t>(_frame_slot) * _cfg.max_instance_count;
            std::memcpy(dst, instances.data(), count * sizeof(uint32_t));
        }
    }

    void resource_pool::write_draw_commands(span<const indexed_indirect_command> commands)
    {
        if (_draw_commands_buffer.cpu_address && !commands.empty())
        {
            const auto count = tempest::min(commands.size(), static_cast<size_t>(_cfg.max_draw_command_count));
            auto* dst = static_cast<indexed_indirect_command*>(_draw_commands_buffer.cpu_address) +
                        static_cast<size_t>(_frame_slot) * _cfg.max_draw_command_count;
            std::memcpy(dst, commands.data(), count * sizeof(indexed_indirect_command));
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
        destroy_buf(_directional_shadow_buffer);
        destroy_buf(_object_buffer);
        destroy_buf(_instance_buffer);
        destroy_buf(_draw_commands_buffer);
    }
} // namespace tempest::render_system
