#include <tempest/render_graph/transient_allocator.hpp>

namespace tempest::render_graph
{
    void transient_allocator::allocate(rhi::device& dev, const compiled_dag& dag,
                                       span<const registered_texture> textures, span<const registered_buffer> buffers,
                                       uint32_t surface_width, uint32_t surface_height)
    {
        _active_textures.clear();
        _active_buffers.clear();

        // Reset last_pass_used for the new frame across pool items in the active flight slot
        for (auto& pool_tex : _texture_pool)
        {
            if (pool_tex.flight_slot == _frame_slot)
            {
                pool_tex.in_use_this_frame = false;
                pool_tex.last_pass_used = 0;
            }
        }

        for (auto& pool_buf : _buffer_pool)
        {
            if (pool_buf.flight_slot == _frame_slot)
            {
                pool_buf.in_use_this_frame = false;
                pool_buf.last_pass_used = 0;
            }
        }

        // 1. Allocate / Recycle Textures
        struct texture_alloc_req
        {
            uint32_t id;
            resource_lifetime lifetime;
        };

        auto tex_requests = vector<texture_alloc_req>{};
        for (const auto& [tex_id, lifetime] : dag.texture_lifetimes)
        {
            tex_requests.push_back(texture_alloc_req{.id = tex_id, .lifetime = lifetime});
        }

        // Sort requests by first_pass ascending
        for (size_t i = 1; i < tex_requests.size(); ++i)
        {
            const auto key = tex_requests[i];
            auto j = static_cast<ptrdiff_t>(i) - 1;
            while (j >= 0 && (tex_requests[j].lifetime.first_pass > key.lifetime.first_pass ||
                              (tex_requests[j].lifetime.first_pass == key.lifetime.first_pass &&
                               tex_requests[j].lifetime.last_pass > key.lifetime.last_pass)))
            {
                tex_requests[j + 1] = tex_requests[j];
                --j;
            }
            tex_requests[j + 1] = key;
        }

        for (const auto& req : tex_requests)
        {
            const auto tex_id = req.id;
            const auto& lifetime = req.lifetime;

            if (tex_id >= textures.size())
            {
                continue;
            }

            const auto& reg_tex = textures[tex_id];

            if (reg_tex.is_imported)
            {
                _active_textures[tex_id] = physical_texture_allocation{
                    .handle = reg_tex.imported_handle,
                    .default_view = reg_tex.imported_view,
                    .sampled_descriptor = {},
                    .storage_descriptor = {},
                    .size = reg_tex.desc.size.evaluate(surface_width, surface_height),
                    .format = reg_tex.desc.format,
                };
                continue;
            }

            const auto res_size = reg_tex.desc.size.evaluate(surface_width, surface_height);
            auto inferred_usage = reg_tex.desc.usage;
            if (reg_tex.desc.format == rhi::data_format::depth16_unorm ||
                reg_tex.desc.format == rhi::data_format::depth24_unorm_stencil8 ||
                reg_tex.desc.format == rhi::data_format::depth32_float ||
                reg_tex.desc.format == rhi::data_format::depth32_float_stencil8)
            {
                inferred_usage |= rhi::texture_usage::depth_stencil_attachment;
            }
            else
            {
                inferred_usage |= rhi::texture_usage::color_attachment;
                if (reg_tex.desc.format == rhi::data_format::rgba8_srgb ||
                    reg_tex.desc.format == rhi::data_format::bgra8_srgb)
                {
                    inferred_usage &= ~rhi::texture_usage::storage;
                }
            }

            const auto req_desc = rhi::texture_desc{
                .width = res_size.width,
                .height = res_size.height,
                .depth = res_size.depth,
                .mip_levels = reg_tex.desc.mip_levels,
                .array_layers = reg_tex.desc.array_layers,
                .format = reg_tex.desc.format,
                .memory_usage = reg_tex.desc.memory_usage,
                .usage = inferred_usage,
                .name = reg_tex.desc.name,
            };

            auto found_index = optional<size_t>{nullopt};
            for (size_t i = 0; i < _texture_pool.size(); ++i)
            {
                auto& p = _texture_pool[i];
                if (p.flight_slot == _frame_slot && p.desc.width == req_desc.width &&
                    p.desc.height == req_desc.height && p.desc.depth == req_desc.depth &&
                    p.desc.format == req_desc.format && p.desc.usage == req_desc.usage &&
                    p.desc.memory_usage == req_desc.memory_usage && p.desc.mip_levels == req_desc.mip_levels &&
                    p.desc.array_layers == req_desc.array_layers)
                {
                    // Check if non-overlapping: previous pass user must have finished before lifetime.first_pass
                    if (!p.in_use_this_frame || p.last_pass_used < lifetime.first_pass)
                    {
                        found_index = i;
                        break;
                    }
                }
            }

            if (found_index.has_value())
            {
                auto& pooled = _texture_pool[found_index.value()];
                pooled.in_use_this_frame = true;
                pooled.last_pass_used = lifetime.last_pass;

                _active_textures[tex_id] = physical_texture_allocation{
                    .handle = pooled.handle,
                    .default_view = pooled.view,
                    .sampled_descriptor = pooled.sampled_descriptor,
                    .storage_descriptor = pooled.storage_descriptor,
                    .size = res_size,
                    .format = req_desc.format,
                };
            }
            else
            {
                const auto h = dev.create_texture(req_desc);
                const auto v = dev.create_texture_view(h, rhi::texture_view_desc{
                                                              .override_format = nullopt,
                                                              .base_mip_level = 0,
                                                              .mip_level_count = req_desc.mip_levels,
                                                              .base_array_layer = 0,
                                                              .array_layer_count = req_desc.array_layers,
                                                          });

                auto sampled_desc = rhi::descriptor_handle{};
                if (static_cast<bool>(req_desc.usage & rhi::texture_usage::sampled))
                {
                    sampled_desc = dev.allocate_descriptor(rhi::descriptor_type::sampled_image);
                    dev.write_sampled_image_descriptor(sampled_desc, v, rhi::image_layout::general);
                }

                auto storage_desc = rhi::descriptor_handle{};
                if (static_cast<bool>(req_desc.usage & rhi::texture_usage::storage))
                {
                    storage_desc = dev.allocate_descriptor(rhi::descriptor_type::storage_image);
                    dev.write_storage_image_descriptor(storage_desc, v, rhi::image_layout::general);
                }

                _texture_pool.push_back(pooled_texture{
                    .handle = h,
                    .view = v,
                    .sampled_descriptor = sampled_desc,
                    .storage_descriptor = storage_desc,
                    .desc = req_desc,
                    .is_surface_relative = (reg_tex.desc.size.mode == size_mode::surface_relative),
                    .in_use_this_frame = true,
                    .last_pass_used = lifetime.last_pass,
                    .flight_slot = _frame_slot,
                });

                _active_textures[tex_id] = physical_texture_allocation{
                    .handle = h,
                    .default_view = v,
                    .sampled_descriptor = sampled_desc,
                    .storage_descriptor = storage_desc,
                    .size = res_size,
                    .format = req_desc.format,
                };
            }
        }

        // 2. Allocate / Recycle Buffers
        struct buffer_alloc_req
        {
            uint32_t id;
            resource_lifetime lifetime;
        };

        auto buf_requests = vector<buffer_alloc_req>{};
        for (const auto& [buf_id, lifetime] : dag.buffer_lifetimes)
        {
            buf_requests.push_back(buffer_alloc_req{.id = buf_id, .lifetime = lifetime});
        }

        for (size_t i = 1; i < buf_requests.size(); ++i)
        {
            const auto key = buf_requests[i];
            auto j = static_cast<ptrdiff_t>(i) - 1;
            while (j >= 0 && (buf_requests[j].lifetime.first_pass > key.lifetime.first_pass ||
                              (buf_requests[j].lifetime.first_pass == key.lifetime.first_pass &&
                               buf_requests[j].lifetime.last_pass > key.lifetime.last_pass)))
            {
                buf_requests[j + 1] = buf_requests[j];
                --j;
            }
            buf_requests[j + 1] = key;
        }

        for (const auto& req : buf_requests)
        {
            const auto buf_id = req.id;
            const auto& lifetime = req.lifetime;

            if (buf_id >= buffers.size())
            {
                continue;
            }

            const auto& reg_buf = buffers[buf_id];

            if (reg_buf.is_imported)
            {
                _active_buffers[buf_id] = physical_buffer_allocation{
                    .handle = reg_buf.imported_handle,
                    .device_address = reg_buf.imported_handle.gpu_address,
                    .size = reg_buf.desc.size,
                };
                continue;
            }

            const auto req_desc = rhi::buffer_desc{
                .size = reg_buf.desc.size,
                .memory_usage = reg_buf.desc.memory_usage,
                .usage = reg_buf.desc.usage,
                .name = reg_buf.desc.name,
            };

            auto found_index = optional<size_t>{nullopt};
            for (size_t i = 0; i < _buffer_pool.size(); ++i)
            {
                auto& p = _buffer_pool[i];
                if (p.flight_slot == _frame_slot && p.desc.size >= req_desc.size && p.desc.usage == req_desc.usage &&
                    p.desc.memory_usage == req_desc.memory_usage)
                {
                    if (!p.in_use_this_frame || p.last_pass_used < lifetime.first_pass)
                    {
                        found_index = i;
                        break;
                    }
                }
            }

            if (found_index.has_value())
            {
                auto& pooled = _buffer_pool[found_index.value()];
                pooled.in_use_this_frame = true;
                pooled.last_pass_used = lifetime.last_pass;

                _active_buffers[buf_id] = physical_buffer_allocation{
                    .handle = pooled.handle,
                    .device_address = pooled.device_address,
                    .size = req_desc.size,
                };
            }
            else
            {
                const auto h = dev.create_buffer(req_desc);

                _buffer_pool.push_back(pooled_buffer{
                    .handle = h,
                    .device_address = h.gpu_address,
                    .desc = req_desc,
                    .in_use_this_frame = true,
                    .last_pass_used = lifetime.last_pass,
                    .flight_slot = _frame_slot,
                });

                _active_buffers[buf_id] = physical_buffer_allocation{
                    .handle = h,
                    .device_address = h.gpu_address,
                    .size = req_desc.size,
                };
            }
        }

        // 3. Propagate Aliases
        for (const auto& [alias_id, target_id] : dag.resolved_texture_aliases)
        {
            if (_active_textures.contains(target_id.id))
            {
                _active_textures[alias_id.id] = _active_textures.find(target_id.id)->second;
            }
        }

        for (const auto& [alias_id, target_id] : dag.resolved_buffer_aliases)
        {
            if (_active_buffers.contains(target_id.id))
            {
                _active_buffers[alias_id.id] = _active_buffers.find(target_id.id)->second;
            }
        }
    }

    void transient_allocator::release_all(rhi::device& dev)
    {
        for (const auto& p : _texture_pool)
        {
            if (p.sampled_descriptor.index != ~0U)
            {
                dev.free_descriptor(rhi::descriptor_type::sampled_image, p.sampled_descriptor);
            }
            if (p.storage_descriptor.index != ~0U)
            {
                dev.free_descriptor(rhi::descriptor_type::storage_image, p.storage_descriptor);
            }
            dev.destroy_texture_view(p.view);
            dev.destroy_texture(p.handle);
        }
        _texture_pool.clear();

        for (const auto& p : _buffer_pool)
        {
            dev.destroy_buffer(p.handle);
        }
        _buffer_pool.clear();

        _active_textures.clear();
        _active_buffers.clear();
    }

    void transient_allocator::on_surface_resize(rhi::device& dev)
    {
        auto remaining_textures = vector<pooled_texture>{};

        for (const auto& p : _texture_pool)
        {
            if (p.is_surface_relative)
            {
                if (p.sampled_descriptor.index != ~0U)
                {
                    dev.free_descriptor(rhi::descriptor_type::sampled_image, p.sampled_descriptor);
                }
                if (p.storage_descriptor.index != ~0U)
                {
                    dev.free_descriptor(rhi::descriptor_type::storage_image, p.storage_descriptor);
                }
                dev.destroy_texture_view(p.view);
                dev.destroy_texture(p.handle);
            }
            else
            {
                remaining_textures.push_back(p);
            }
        }

        _texture_pool = tempest::move(remaining_textures);
        _active_textures.clear();
    }

    auto transient_allocator::get_texture(uint32_t texture_id) const noexcept -> const physical_texture_allocation*
    {
        const auto it = _active_textures.find(texture_id);
        if (it != _active_textures.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    auto transient_allocator::get_buffer(uint32_t buffer_id) const noexcept -> const physical_buffer_allocation*
    {
        const auto it = _active_buffers.find(buffer_id);
        if (it != _active_buffers.end())
        {
            return &it->second;
        }
        return nullptr;
    }
} // namespace tempest::render_graph
