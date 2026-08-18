#include <tempest/render_graph/context.hpp>
#include <tempest/render_graph/pass_builder.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_graph/temporal_texture.hpp>

namespace tempest::render_graph
{
    // =========================================================================
    // pass_execution_context
    // =========================================================================

    auto pass_execution_context::get_texture(rg_texture_id tex) const -> rhi::texture_handle
    {
        if (_graph)
        {
            const auto* alloc = _graph->get_physical_texture(tex.id);
            if (alloc)
            {
                return alloc->handle;
            }
        }
        return rhi::texture_handle{};
    }

    auto pass_execution_context::get_texture_view(rg_texture_id tex) const -> rhi::texture_view_handle
    {
        if (_graph)
        {
            const auto* alloc = _graph->get_physical_texture(tex.id);
            if (alloc)
            {
                return alloc->default_view;
            }
        }
        return rhi::texture_view_handle{};
    }

    auto pass_execution_context::get_buffer(rg_buffer_id buf) const -> rhi::buffer_handle
    {
        if (_graph)
        {
            const auto* alloc = _graph->get_physical_buffer(buf.id);
            if (alloc)
            {
                return alloc->handle;
            }
        }
        return rhi::buffer_handle{};
    }

    auto pass_execution_context::get_buffer_device_address(rg_buffer_id buf) const -> uint64_t
    {
        if (_graph)
        {
            const auto* alloc = _graph->get_physical_buffer(buf.id);
            if (alloc)
            {
                return alloc->device_address;
            }
        }
        return null_device_address;
    }

    auto pass_execution_context::get_texture_descriptor(rg_texture_id tex) const -> uint32_t
    {
        if (_graph)
        {
            const auto* alloc = _graph->get_physical_texture(tex.id);
            if (alloc)
            {
                return alloc->sampled_descriptor.index;
            }
        }
        return invalid_descriptor_index;
    }

    auto pass_execution_context::get_storage_texture_descriptor(rg_texture_id tex) const -> uint32_t
    {
        if (_graph)
        {
            const auto* alloc = _graph->get_physical_texture(tex.id);
            if (alloc)
            {
                return alloc->storage_descriptor.index;
            }
        }
        return invalid_descriptor_index;
    }

    auto pass_execution_context::get_resolved_size(rg_texture_id tex) const -> resolved_size
    {
        if (_graph)
        {
            return _graph->resolve_texture_size(tex);
        }
        return resolved_size{.width = 0, .height = 0, .depth = 1};
    }

    // =========================================================================
    // pass_builder
    // =========================================================================

    auto pass_builder::create_texture(const rg_texture_desc& desc) -> rg_texture_id
    {
        const auto id = _graph->create_texture(desc);
        _node->texture_outputs.push_back(id);
        return id;
    }

    auto pass_builder::create_buffer(const rg_buffer_desc& desc) -> rg_buffer_id
    {
        const auto id = _graph->create_buffer(desc);
        _node->buffer_outputs.push_back(id);
        return id;
    }

    auto pass_builder::import_texture(rhi::texture_handle handle, rhi::texture_view_handle view,
                                      rhi::image_layout initial_layout) -> rg_texture_id
    {
        return _graph->import_texture(handle, view, initial_layout);
    }

    auto pass_builder::import_texture(rhi::texture_handle handle, rhi::image_layout initial_layout) -> rg_texture_id
    {
        return _graph->import_texture(handle, initial_layout);
    }

    auto pass_builder::import_buffer(rhi::buffer_handle handle) -> rg_buffer_id
    {
        return _graph->import_buffer(handle);
    }

    auto pass_builder::read(rg_texture_id tex, enum_mask<rhi::pipeline_stage> stages,
                            enum_mask<rhi::resource_access> access, rhi::image_layout layout,
                            const rg_subresource_range& subresource) -> rg_texture_id
    {
        _node->texture_accesses.push_back(texture_access{
            .texture = tex,
            .type = access_type::read,
            .stages = stages,
            .access = access,
            .layout = layout,
            .subresource = subresource,
            .load_op = rhi::load_op::dont_care,
            .store_op = rhi::store_op::dont_care,
            .clear_color = {},
            .clear_depth_stencil = {},
            .attachment = attachment_type::none,
        });

        return tex;
    }

    auto pass_builder::write(rg_texture_id tex, enum_mask<rhi::pipeline_stage> stages,
                             enum_mask<rhi::resource_access> access, rhi::image_layout layout,
                             const rg_subresource_range& subresource) -> rg_texture_id
    {
        _node->texture_accesses.push_back(texture_access{
            .texture = tex,
            .type = access_type::write,
            .stages = stages,
            .access = access,
            .layout = layout,
            .subresource = subresource,
            .load_op = rhi::load_op::dont_care,
            .store_op = rhi::store_op::dont_care,
            .clear_color = {},
            .clear_depth_stencil = {},
            .attachment = attachment_type::none,
        });

        const auto next = tex.next_version();
        _node->texture_outputs.push_back(next);
        return next;
    }

    auto pass_builder::read_write(rg_texture_id tex, enum_mask<rhi::pipeline_stage> stages,
                                  enum_mask<rhi::resource_access> access, rhi::image_layout layout,
                                  const rg_subresource_range& subresource) -> rg_texture_id
    {
        read(tex, stages, access, layout, subresource);
        return write(tex, stages, access, layout, subresource);
    }

    auto pass_builder::set_color_attachment([[maybe_unused]] uint32_t slot, const rg_color_attachment& attachment)
        -> rg_texture_id
    {
        _node->texture_accesses.push_back(texture_access{
            .texture = attachment.texture,
            .type = access_type::write,
            .stages = rhi::pipeline_stage::attachment_output,
            .access = (attachment.load_op == rhi::load_op::load) ? rhi::resource_access::read_write
                                                                  : rhi::resource_access::write,
            .layout = rhi::image_layout::general,
            .subresource = attachment.subresource,
            .load_op = attachment.load_op,
            .store_op = attachment.store_op,
            .clear_color = attachment.clear_value,
            .clear_depth_stencil = {},
            .attachment = attachment_type::color,
        });

        const auto next = attachment.texture.next_version();
        _node->texture_outputs.push_back(next);
        return next;
    }

    auto pass_builder::set_depth_stencil_attachment(const rg_depth_stencil_attachment& attachment) -> rg_texture_id
    {
        _node->texture_accesses.push_back(texture_access{
            .texture = attachment.texture,
            .type = access_type::write,
            .stages = rhi::pipeline_stage::early_fragment_tests | rhi::pipeline_stage::late_fragment_tests,
            .access = rhi::resource_access::read_write,
            .layout = rhi::image_layout::general,
            .subresource = attachment.subresource,
            .load_op = attachment.depth_load_op,
            .store_op = attachment.depth_store_op,
            .clear_color = {},
            .clear_depth_stencil = attachment.clear_value,
            .attachment = attachment_type::depth_stencil,
        });

        const auto next = attachment.texture.next_version();
        _node->texture_outputs.push_back(next);
        return next;
    }

    auto pass_builder::use_temporal_texture(temporal_texture& tex, uint32_t requested_history_depth)
        -> temporal_binding
    {
        _graph->track_temporal_resource(&tex);

        auto binding = temporal_binding{};
        const auto max_history = tempest::min(requested_history_depth, static_cast<uint32_t>(max_history_frames));

        for (uint32_t delta = 1; delta <= max_history; ++delta)
        {
            if (tex.is_history_valid(delta))
            {
                const auto h = tex.get_history_texture(delta);
                const auto v = tex.get_history_view(delta);
                const auto imported_id = import_texture(h, v, rhi::image_layout::general);
                binding.history_reads.push_back(imported_id);
            }
        }

        const auto write_h = tex.get_write_texture();
        const auto write_v = tex.get_write_view();
        binding.target_write = import_texture(write_h, write_v, rhi::image_layout::undefined);

        return binding;
    }

    auto pass_builder::set_temporal_color_attachment(uint32_t slot, const rg_temporal_color_attachment& attachment)
        -> rg_texture_id
    {
        _graph->track_temporal_resource(&attachment.texture);

        const auto write_h = attachment.texture.get_write_texture();
        const auto write_v = attachment.texture.get_write_view();
        const auto imported_id = import_texture(write_h, write_v, rhi::image_layout::undefined);

        return set_color_attachment(slot, rg_color_attachment{
                                              .texture = imported_id,
                                              .load_op = attachment.load_op,
                                              .store_op = attachment.store_op,
                                              .clear_value = attachment.clear_value,
                                              .subresource = attachment.subresource,
                                          });
    }

    void pass_builder::clear_temporal_texture(temporal_texture& tex, rhi::clear_color_value clear_value)
    {
        _graph->add_clear_temporal_pass(tex, clear_value);
    }

    auto pass_builder::read(rg_buffer_id buf, enum_mask<rhi::pipeline_stage> stages,
                            enum_mask<rhi::resource_access> access, uint64_t offset, uint64_t size) -> rg_buffer_id
    {
        _node->buffer_accesses.push_back(buffer_access{
            .buffer = buf,
            .type = access_type::read,
            .stages = stages,
            .access = access,
            .offset = offset,
            .size = size,
        });

        return buf;
    }

    auto pass_builder::write(rg_buffer_id buf, enum_mask<rhi::pipeline_stage> stages,
                             enum_mask<rhi::resource_access> access, uint64_t offset, uint64_t size) -> rg_buffer_id
    {
        _node->buffer_accesses.push_back(buffer_access{
            .buffer = buf,
            .type = access_type::write,
            .stages = stages,
            .access = access,
            .offset = offset,
            .size = size,
        });

        const auto next = buf.next_version();
        _node->buffer_outputs.push_back(next);
        return next;
    }

    auto pass_builder::read_write(rg_buffer_id buf, enum_mask<rhi::pipeline_stage> stages,
                                  enum_mask<rhi::resource_access> access, uint64_t offset, uint64_t size) -> rg_buffer_id
    {
        read(buf, stages, access, offset, size);
        return write(buf, stages, access, offset, size);
    }

    void pass_builder::fallback(rg_texture_id produced, rg_texture_id fallback_source)
    {
        _node->texture_fallbacks[produced] = fallback_source;
    }

    void pass_builder::fallback(rg_buffer_id produced, rg_buffer_id fallback_source)
    {
        _node->buffer_fallbacks[produced] = fallback_source;
    }

    auto pass_builder::passthrough(rg_texture_id input) -> rg_texture_id
    {
        const auto next = input.next_version();
        _node->texture_fallbacks[next] = input;
        return next;
    }

    auto pass_builder::passthrough(rg_buffer_id input) -> rg_buffer_id
    {
        const auto next = input.next_version();
        _node->buffer_fallbacks[next] = input;
        return next;
    }

    void pass_builder::mark_sink() noexcept
    {
        _node->is_sink = true;
    }

    void pass_builder::set_execution_queue(queue_type queue) noexcept
    {
        _node->queue = queue;
    }

    void pass_builder::set_enable_condition(function<bool()> condition)
    {
        _node->enable_condition = tempest::move(condition);
    }

    // =========================================================================
    // render_graph
    // =========================================================================

    void render_graph::set_surface_size(uint32_t width, uint32_t height) noexcept
    {
        _surface_width = width;
        _surface_height = height;
    }

    auto render_graph::get_surface_width() const noexcept -> uint32_t
    {
        return _surface_width;
    }

    auto render_graph::get_surface_height() const noexcept -> uint32_t
    {
        return _surface_height;
    }

    auto render_graph::create_texture(const rg_texture_desc& desc) -> rg_texture_id
    {
        return _compiler.register_texture(desc);
    }

    auto render_graph::create_buffer(const rg_buffer_desc& desc) -> rg_buffer_id
    {
        return _compiler.register_buffer(desc);
    }

    auto render_graph::import_texture(rhi::texture_handle handle, rhi::texture_view_handle view,
                                      rhi::image_layout initial_layout) -> rg_texture_id
    {
        return _compiler.import_texture(handle, view, initial_layout);
    }

    auto render_graph::import_texture(rhi::texture_handle handle, rhi::image_layout initial_layout) -> rg_texture_id
    {
        return _compiler.import_texture(handle, initial_layout);
    }

    auto render_graph::import_buffer(rhi::buffer_handle handle) -> rg_buffer_id
    {
        return _compiler.import_buffer(handle);
    }

    auto render_graph::resolve_texture_size(rg_texture_id tex) const -> resolved_size
    {
        const auto textures = _compiler.get_registered_textures();
        if (tex.id < textures.size())
        {
            return textures[tex.id].desc.size.evaluate(_surface_width, _surface_height);
        }
        return resolved_size{.width = 0, .height = 0, .depth = 1};
    }

    struct present_pass_data
    {
        rg_texture_id src;
        rg_texture_id dst;
    };

    auto render_graph::add_present_pass(rg_texture_id src_tex, rhi::texture_handle swapchain_tex) -> void
    {
        const auto swapchain_rg_tex = import_texture(swapchain_tex, rhi::image_layout::undefined);

        add_transfer_pass<present_pass_data>(
            "PresentPass",
            [src_tex, swapchain_rg_tex](pass_builder& builder, present_pass_data& data) {
                data.src = builder.read(src_tex, rhi::pipeline_stage::blit, rhi::resource_access::read,
                                        rhi::image_layout::general);
                data.dst = builder.write(swapchain_rg_tex, rhi::pipeline_stage::blit,
                                         rhi::resource_access::write,
                                         rhi::image_layout::present);
                builder.mark_sink();
            },
            [](const present_pass_data&, pass_execution_context&, rhi::command_list&) {
                // Physical copy/blit/transition will be issued here during execution phase
            });
    }

    struct clear_temporal_pass_data
    {
        inplace_vector<rg_texture_id, max_temporal_slots> targets;
    };

    auto render_graph::add_clear_temporal_pass(temporal_texture& tex, rhi::clear_color_value clear_value) -> void
    {
        track_temporal_resource(&tex);

        add_graphics_pass<clear_temporal_pass_data>(
            "ClearTemporalTexturePass",
            [&tex, clear_value](pass_builder& builder, clear_temporal_pass_data& data) {
                const auto textures = tex.get_all_textures();
                const auto views = tex.get_all_views();
                for (size_t i = 0; i < textures.size(); ++i)
                {
                    const auto imported = builder.import_texture(textures[i], views[i], rhi::image_layout::undefined);
                    const auto target = builder.set_color_attachment(static_cast<uint32_t>(i), rg_color_attachment{
                                                                                                  .texture = imported,
                                                                                                  .load_op = rhi::load_op::clear,
                                                                                                  .store_op = rhi::store_op::store,
                                                                                                  .clear_value = clear_value,
                                                                                              });
                    data.targets.push_back(target);
                }
                builder.mark_sink();
            },
            []([[maybe_unused]] const clear_temporal_pass_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {
                // Color attachment load_op::clear automatically performs the clear during render pass begin
            });

        tex.invalidate();
    }

    void render_graph::track_temporal_resource(temporal_texture* tex)
    {
        if (tex == nullptr)
        {
            return;
        }
        for (const auto* tracked : _tracked_temporal_resources)
        {
            if (tracked == tex)
            {
                return;
            }
        }
        _tracked_temporal_resources.push_back(tex);
    }

    auto render_graph::get_tracked_temporal_resources() const noexcept -> span<temporal_texture* const>
    {
        return span<temporal_texture* const>{_tracked_temporal_resources.data(), _tracked_temporal_resources.size()};
    }

    auto render_graph::compile() -> expected<compiled_dag, dag_compile_error>
    {
        return _compiler.compile();
    }

    auto render_graph::execute(rhi::device& dev, const frame_sync_options& frame_sync)
        -> expected<void, execution_error>
    {
        return _executor.execute(dev, *this, frame_sync);
    }

    void render_graph::reset()
    {
        _compiler.reset();
        _pass_data_storage.clear();
        _tracked_temporal_resources.clear();
    }
} // namespace tempest::render_graph
