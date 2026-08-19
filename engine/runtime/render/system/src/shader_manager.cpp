#include <tempest/render_system/shader_manager.hpp>

#include <filesystem>
#include <tempest/files.hpp>

namespace tempest::render_system
{
    shader_manager::shader_manager(rhi::device& dev, string_view shader_dir)
        : _device{&dev}, _shader_dir{shader_dir.data(), shader_dir.size()}
    {
        // Reserve index 0 as invalid sentinel across slot arrays
        _modules.push_back(shader_module_record{});
        _graphics_pipelines.push_back(graphics_pipeline_record{});
        _compute_pipelines.push_back(compute_pipeline_record{});
    }

    shader_manager::~shader_manager()
    {
        release_all();
    }

    shader_manager::shader_manager(shader_manager&& other) noexcept
        : _device{other._device}, _shader_dir{tempest::move(other._shader_dir)},
          _modules{tempest::move(other._modules)},
          _graphics_pipelines{tempest::move(other._graphics_pipelines)},
          _compute_pipelines{tempest::move(other._compute_pipelines)},
          _module_paths_to_handle{tempest::move(other._module_paths_to_handle)},
          _named_graphics_pipelines{tempest::move(other._named_graphics_pipelines)},
          _named_compute_pipelines{tempest::move(other._named_compute_pipelines)},
          _module_to_graphics_pipelines{tempest::move(other._module_to_graphics_pipelines)},
          _module_to_compute_pipelines{tempest::move(other._module_to_compute_pipelines)},
          _retired_pipelines{tempest::move(other._retired_pipelines)}
    {
        other._device = nullptr;
    }

    shader_manager& shader_manager::operator=(shader_manager&& other) noexcept
    {
        if (this != &other)
        {
            release_all();
            _device = other._device;
            _shader_dir = tempest::move(other._shader_dir);
            _modules = tempest::move(other._modules);
            _graphics_pipelines = tempest::move(other._graphics_pipelines);
            _compute_pipelines = tempest::move(other._compute_pipelines);
            _module_paths_to_handle = tempest::move(other._module_paths_to_handle);
            _named_graphics_pipelines = tempest::move(other._named_graphics_pipelines);
            _named_compute_pipelines = tempest::move(other._named_compute_pipelines);
            _module_to_graphics_pipelines = tempest::move(other._module_to_graphics_pipelines);
            _module_to_compute_pipelines = tempest::move(other._module_to_compute_pipelines);
            _retired_pipelines = tempest::move(other._retired_pipelines);
            other._device = nullptr;
        }
        return *this;
    }

    auto shader_manager::resolve_path(const std::filesystem::path& input_path) const -> optional<std::filesystem::path>
    {
        if (std::filesystem::exists(input_path))
        {
            return std::filesystem::weakly_canonical(input_path);
        }

        const auto search_paths = {
            std::filesystem::path(_shader_dir.c_str()) / input_path,
            std::filesystem::path(_shader_dir.c_str()) / "rs" / input_path,
            std::filesystem::path("shaders/rs") / input_path,
            std::filesystem::path("shaders") / input_path,
            std::filesystem::path("bin/Debug/windows-clang/shaders/rs") / input_path,
            std::filesystem::path("bin/Debug/windows-clang/shaders") / input_path,
            std::filesystem::path("bin/Debug/windows-msc-v145/shaders/rs") / input_path,
            std::filesystem::path("bin/Debug/windows-msc-v145/shaders") / input_path,
            std::filesystem::path("assets/shaders") / input_path,
            std::filesystem::path("engine/runtime/render/system/shaders") / input_path,
        };

        for (const auto& sp : search_paths)
        {
            if (std::filesystem::exists(sp))
            {
                return std::filesystem::weakly_canonical(sp);
            }
        }
        return nullopt;
    }

    auto shader_manager::read_module_bytes(const shader_module_record& mod) const -> vector<byte>
    {
        if (!mod.memory_blob.empty())
        {
            return mod.memory_blob;
        }

        if (mod.canonical_path.has_value())
        {
            auto path_str = mod.canonical_path->string();
            auto bytes = core::read_bytes(string_view{path_str.c_str(), path_str.size()});
            if (!bytes.empty())
            {
                return bytes;
            }
        }

        if (mod.disk_location.has_value())
        {
            auto resolved = resolve_path(*mod.disk_location);
            if (resolved.has_value())
            {
                auto path_str = resolved->string();
                return core::read_bytes(string_view{path_str.c_str(), path_str.size()});
            }
        }
        return {};
    }

    auto shader_manager::register_shader_module(const shader_module_create_info& info) -> shader_module_handle
    {
        auto canonical = optional<std::filesystem::path>{};
        if (info.disk_location.has_value())
        {
            canonical = resolve_path(*info.disk_location);
            const auto loc_str = string{info.disk_location->string().c_str(), info.disk_location->string().size()};
            auto it = _module_paths_to_handle.find(loc_str);
            if (it != _module_paths_to_handle.end())
            {
                const auto existing_handle = it->second;
                if (!info.initial_bytes.empty())
                {
                    update_shader_module_bytes(existing_handle, info.initial_bytes);
                }
                return existing_handle;
            }

            if (canonical.has_value())
            {
                const auto can_str = string{canonical->string().c_str(), canonical->string().size()};
                auto it_can = _module_paths_to_handle.find(can_str);
                if (it_can != _module_paths_to_handle.end())
                {
                    const auto existing_handle = it_can->second;
                    if (!info.initial_bytes.empty())
                    {
                        update_shader_module_bytes(existing_handle, info.initial_bytes);
                    }
                    return existing_handle;
                }
            }
        }

        const auto handle_id = static_cast<uint32_t>(_modules.size());
        auto rec = shader_module_record{
            .disk_location = info.disk_location,
            .canonical_path = canonical,
            .memory_blob = !info.disk_location.has_value() && !info.initial_bytes.empty()
                               ? vector<byte>{info.initial_bytes.begin(), info.initial_bytes.end()}
                               : vector<byte>{},
            .stage = info.stage,
            .entry_point = string{info.entry_point.data(), info.entry_point.size()},
            .revision = 1,
        };
        _modules.push_back(tempest::move(rec));

        const auto handle = shader_module_handle{handle_id};
        if (info.disk_location.has_value())
        {
            const auto loc_str = string{info.disk_location->string().c_str(), info.disk_location->string().size()};
            _module_paths_to_handle[loc_str] = handle;
            const auto filename_str = string{info.disk_location->filename().string().c_str(),
                                             info.disk_location->filename().string().size()};
            _module_paths_to_handle[filename_str] = handle;

            if (canonical.has_value())
            {
                const auto can_str = string{canonical->string().c_str(), canonical->string().size()};
                _module_paths_to_handle[can_str] = handle;
            }
        }

        return handle;
    }

    auto shader_manager::register_shader_module(string_view filename, rhi::shader_stage stage,
                                               string_view entry_point) -> shader_module_handle
    {
        return register_shader_module(shader_module_create_info{
            .stage = stage,
            .entry_point = entry_point,
            .disk_location = std::filesystem::path(std::string(filename.data(), filename.size())),
            .initial_bytes = {},
        });
    }

    auto shader_manager::compile_graphics_pipeline(const graphics_pipeline_record& rec,
                                                   span<const byte> override_bytes,
                                                   shader_module_handle override_handle)
        -> rhi::graphics_pipeline_handle
    {
        if (!_device)
        {
            return {};
        }

        auto module_bytecodes = vector<vector<byte>>{};
        module_bytecodes.reserve(rec.modules.size());
        auto shader_descs = vector<rhi::shader_module_desc>{};
        shader_descs.reserve(rec.modules.size());

        for (const auto mod_h : rec.modules)
        {
            if (mod_h.id == 0 || mod_h.id >= _modules.size())
            {
                return {};
            }

            const auto& mod = _modules[mod_h.id];
            auto byte_span = span<const byte>{};

            if (mod_h.id == override_handle.id && !override_bytes.empty())
            {
                byte_span = override_bytes;
            }
            else
            {
                auto bytes = read_module_bytes(mod);
                if (bytes.empty())
                {
                    return {};
                }
                module_bytecodes.push_back(tempest::move(bytes));
                const auto& cached = module_bytecodes.back();
                byte_span = span<const byte>{cached.data(), cached.size()};
            }

            shader_descs.push_back(rhi::shader_module_desc{
                .stage = mod.stage,
                .ir_code = byte_span,
                .entry_point = cstring_view{mod.entry_point.data(), mod.entry_point.size()},
                .specialization_constants = {},
            });
        }

        auto desc = rhi::graphics_pipeline_desc{
            .shader_modules = span<const rhi::shader_module_desc>{shader_descs.data(), shader_descs.size()},
            .color_attachment_formats = span<const rhi::data_format>{rec.color_attachment_formats.data(),
                                                                     rec.color_attachment_formats.size()},
            .depth_stencil_attachment_format = rec.depth_stencil_attachment_format,
            .primitive_topology = rec.primitive_topology,
            .rasterization_state = rec.rasterization_state,
            .depth_stencil_state = rec.depth_stencil_state,
            .color_attachment_blend_states = span<const rhi::attachment_blend_state>{
                rec.color_attachment_blend_states.data(), rec.color_attachment_blend_states.size()},
        };

        return _device->create_graphics_pipeline(desc);
    }

    auto shader_manager::compile_compute_pipeline(const compute_pipeline_record& rec,
                                                  span<const byte> override_bytes)
        -> rhi::compute_pipeline_handle
    {
        if (!_device || rec.module.id == 0 || rec.module.id >= _modules.size())
        {
            return {};
        }

        const auto& mod = _modules[rec.module.id];
        auto byte_span = span<const byte>{};
        auto bytes = vector<byte>{};

        if (!override_bytes.empty())
        {
            byte_span = override_bytes;
        }
        else
        {
            bytes = read_module_bytes(mod);
            if (bytes.empty())
            {
                return {};
            }
            byte_span = span<const byte>{bytes.data(), bytes.size()};
        }

        auto desc = rhi::compute_pipeline_desc{
            .shader_module =
                rhi::shader_module_desc{
                    .stage = rhi::shader_stage::compute,
                    .ir_code = byte_span,
                    .entry_point = cstring_view{mod.entry_point.data(), mod.entry_point.size()},
                    .specialization_constants = {},
                },
        };

        return _device->create_compute_pipeline(desc);
    }

    auto shader_manager::register_graphics_pipeline(string_view name, const graphics_pipeline_template& tmpl)
        -> graphics_pipeline_handle
    {
        const auto name_str = string{name.data(), name.size()};
        auto it = _named_graphics_pipelines.find(name_str);
        if (it != _named_graphics_pipelines.end())
        {
            return it->second;
        }

        auto rec = graphics_pipeline_record{
            .name = name_str,
            .rhi_pipeline = {},
            .modules = vector<shader_module_handle>{tmpl.shader_modules.begin(), tmpl.shader_modules.end()},
            .color_attachment_formats = vector<rhi::data_format>{tmpl.color_attachment_formats.begin(),
                                                                 tmpl.color_attachment_formats.end()},
            .depth_stencil_attachment_format = tmpl.depth_stencil_attachment_format,
            .primitive_topology = tmpl.primitive_topology,
            .rasterization_state = tmpl.rasterization_state,
            .depth_stencil_state = tmpl.depth_stencil_state,
            .color_attachment_blend_states = vector<rhi::attachment_blend_state>{
                tmpl.color_attachment_blend_states.begin(), tmpl.color_attachment_blend_states.end()},
        };

        auto rhi_h = compile_graphics_pipeline(rec);
        rec.rhi_pipeline = rhi_h;

        const auto handle_id = static_cast<uint32_t>(_graphics_pipelines.size());
        _graphics_pipelines.push_back(tempest::move(rec));

        const auto handle = graphics_pipeline_handle{handle_id};
        _named_graphics_pipelines[name_str] = handle;

        for (const auto mod_h : tmpl.shader_modules)
        {
            _module_to_graphics_pipelines[mod_h.id].push_back(handle);
        }

        return handle;
    }

    auto shader_manager::register_compute_pipeline(string_view name, const compute_pipeline_template& tmpl)
        -> compute_pipeline_handle
    {
        const auto name_str = string{name.data(), name.size()};
        auto it = _named_compute_pipelines.find(name_str);
        if (it != _named_compute_pipelines.end())
        {
            return it->second;
        }

        auto rec = compute_pipeline_record{
            .name = name_str,
            .rhi_pipeline = {},
            .module = tmpl.shader_module,
        };

        auto rhi_h = compile_compute_pipeline(rec);
        rec.rhi_pipeline = rhi_h;

        const auto handle_id = static_cast<uint32_t>(_compute_pipelines.size());
        _compute_pipelines.push_back(tempest::move(rec));

        const auto handle = compute_pipeline_handle{handle_id};
        _named_compute_pipelines[name_str] = handle;

        _module_to_compute_pipelines[tmpl.shader_module.id].push_back(handle);

        return handle;
    }

    auto shader_manager::get_rhi_pipeline(graphics_pipeline_handle handle) const noexcept
        -> rhi::graphics_pipeline_handle
    {
        if (handle.id == 0 || handle.id >= _graphics_pipelines.size())
        {
            return {};
        }
        return _graphics_pipelines[handle.id].rhi_pipeline;
    }

    auto shader_manager::get_rhi_pipeline(compute_pipeline_handle handle) const noexcept
        -> rhi::compute_pipeline_handle
    {
        if (handle.id == 0 || handle.id >= _compute_pipelines.size())
        {
            return {};
        }
        return _compute_pipelines[handle.id].rhi_pipeline;
    }

    auto shader_manager::find_graphics_pipeline(string_view name) const noexcept
        -> optional<graphics_pipeline_handle>
    {
        const auto name_str = string{name.data(), name.size()};
        auto it = _named_graphics_pipelines.find(name_str);
        if (it != _named_graphics_pipelines.end())
        {
            return it->second;
        }
        return nullopt;
    }

    auto shader_manager::find_compute_pipeline(string_view name) const noexcept
        -> optional<compute_pipeline_handle>
    {
        const auto name_str = string{name.data(), name.size()};
        auto it = _named_compute_pipelines.find(name_str);
        if (it != _named_compute_pipelines.end())
        {
            return it->second;
        }
        return nullopt;
    }

    void shader_manager::enqueue_pipeline_retirement(rhi::graphics_pipeline_handle gfx,
                                                     rhi::compute_pipeline_handle comp)
    {
        if (!_device || (gfx.handle == 0 && comp.handle == 0))
        {
            return;
        }

        auto entry = retired_pipeline_entry{
            .graphics_pipeline = gfx,
            .compute_pipeline = comp,
        };

        // Snapshot current timeline sync points from all execution ports
        auto& gfx_port = _device->get_graphics_execution_port();
        entry.required_sync_points.push_back(gfx_port.get_timeline_sync_point());

        auto& comp_port = _device->get_async_compute_execution_port();
        entry.required_sync_points.push_back(comp_port.get_timeline_sync_point());

        auto& xfer_port = _device->get_async_transfer_execution_port();
        entry.required_sync_points.push_back(xfer_port.get_timeline_sync_point());

        _retired_pipelines.push_back(tempest::move(entry));
    }

    auto shader_manager::update_shader_module_bytes(shader_module_handle handle, span<const byte> new_bytes) -> bool
    {
        if (handle.id == 0 || handle.id >= _modules.size() || new_bytes.empty())
        {
            return false;
        }

        auto& mod = _modules[handle.id];
        mod.revision++;

        bool all_recompiled = true;

        // Surgically recompile dependent graphics pipelines
        auto it_gfx = _module_to_graphics_pipelines.find(handle.id);
        if (it_gfx != _module_to_graphics_pipelines.end())
        {
            for (const auto pipe_h : it_gfx->second)
            {
                if (pipe_h.id < _graphics_pipelines.size())
                {
                    auto& rec = _graphics_pipelines[pipe_h.id];
                    auto new_pipeline = compile_graphics_pipeline(rec, new_bytes, handle);
                    if (new_pipeline.handle != 0)
                    {
                        enqueue_pipeline_retirement(rec.rhi_pipeline, {});
                        rec.rhi_pipeline = new_pipeline;
                    }
                    else
                    {
                        // Fail-safe: keep existing valid pipeline
                        all_recompiled = false;
                    }
                }
            }
        }

        // Surgically recompile dependent compute pipelines
        auto it_comp = _module_to_compute_pipelines.find(handle.id);
        if (it_comp != _module_to_compute_pipelines.end())
        {
            for (const auto pipe_h : it_comp->second)
            {
                if (pipe_h.id < _compute_pipelines.size())
                {
                    auto& rec = _compute_pipelines[pipe_h.id];
                    auto new_pipeline = compile_compute_pipeline(rec, new_bytes);
                    if (new_pipeline.handle != 0)
                    {
                        enqueue_pipeline_retirement({}, rec.rhi_pipeline);
                        rec.rhi_pipeline = new_pipeline;
                    }
                    else
                    {
                        // Fail-safe: keep existing valid pipeline
                        all_recompiled = false;
                    }
                }
            }
        }

        if (all_recompiled && !mod.disk_location.has_value())
        {
            mod.memory_blob = vector<byte>{new_bytes.begin(), new_bytes.end()};
        }

        return all_recompiled;
    }

    auto shader_manager::reload_shader_module(shader_module_handle handle) -> bool
    {
        if (handle.id == 0 || handle.id >= _modules.size())
        {
            return false;
        }

        const auto& mod = _modules[handle.id];
        auto bytes = read_module_bytes(mod);
        if (bytes.empty())
        {
            return false;
        }

        return update_shader_module_bytes(handle, span<const byte>{bytes.data(), bytes.size()});
    }

    auto shader_manager::notify_file_changed(const std::filesystem::path& path) -> bool
    {
        const auto filename = path.filename().string();
        const auto canonical = std::filesystem::exists(path) ? optional<std::filesystem::path>{std::filesystem::weakly_canonical(path)} : nullopt;

        bool any_reloaded = false;
        for (uint32_t i = 1; i < _modules.size(); ++i)
        {
            const auto& mod = _modules[i];
            if (!mod.disk_location.has_value())
            {
                continue;
            }

            bool match = false;
            if (canonical.has_value() && mod.canonical_path.has_value() && *canonical == *mod.canonical_path)
            {
                match = true;
            }
            else if (mod.disk_location->filename() == filename || mod.disk_location->string() == path.string())
            {
                match = true;
            }

            if (match)
            {
                if (reload_shader_module(shader_module_handle{i}))
                {
                    any_reloaded = true;
                }
            }
        }
        return any_reloaded;
    }

    void shader_manager::process_deferred_retirements()
    {
        if (!_device)
        {
            return;
        }

        for (auto it = _retired_pipelines.begin(); it != _retired_pipelines.end();)
        {
            bool all_queues_completed = true;
            for (const auto& sync_point : it->required_sync_points)
            {
                if (sync_point.semaphore.handle != 0 && sync_point.value > 0)
                {
                    const auto current_val = _device->get_semaphore_value(sync_point.semaphore);
                    if (current_val < sync_point.value)
                    {
                        all_queues_completed = false;
                        break;
                    }
                }
            }

            if (all_queues_completed)
            {
                if (it->graphics_pipeline.handle != 0)
                {
                    _device->destroy_graphics_pipeline(it->graphics_pipeline);
                }
                if (it->compute_pipeline.handle != 0)
                {
                    _device->destroy_compute_pipeline(it->compute_pipeline);
                }
                it = _retired_pipelines.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void shader_manager::release_all()
    {
        if (_device)
        {
            for (const auto& it : _retired_pipelines)
            {
                if (it.graphics_pipeline.handle != 0)
                {
                    _device->destroy_graphics_pipeline(it.graphics_pipeline);
                }
                if (it.compute_pipeline.handle != 0)
                {
                    _device->destroy_compute_pipeline(it.compute_pipeline);
                }
            }
            _retired_pipelines.clear();

            for (const auto& rec : _graphics_pipelines)
            {
                if (rec.rhi_pipeline.handle != 0)
                {
                    _device->destroy_graphics_pipeline(rec.rhi_pipeline);
                }
            }
            _graphics_pipelines.clear();
            _graphics_pipelines.push_back(graphics_pipeline_record{});

            for (const auto& rec : _compute_pipelines)
            {
                if (rec.rhi_pipeline.handle != 0)
                {
                    _device->destroy_compute_pipeline(rec.rhi_pipeline);
                }
            }
            _compute_pipelines.clear();
            _compute_pipelines.push_back(compute_pipeline_record{});
        }

        _modules.clear();
        _modules.push_back(shader_module_record{});

        _module_paths_to_handle.clear();
        _named_graphics_pipelines.clear();
        _named_compute_pipelines.clear();
        _module_to_graphics_pipelines.clear();
        _module_to_compute_pipelines.clear();
        _legacy_bytecode_cache.clear();
    }

    auto shader_manager::load_shader_bytecode(string_view shader_filename) -> vector<byte>
    {
        const auto key = string{shader_filename.data(), shader_filename.size()};
        auto it = _legacy_bytecode_cache.find(key);
        if (it != _legacy_bytecode_cache.end())
        {
            return it->second;
        }

        auto resolved = resolve_path(std::filesystem::path(std::string(shader_filename.data(), shader_filename.size())));
        if (resolved.has_value())
        {
            auto path_str = resolved->string();
            auto bytes = core::read_bytes(string_view{path_str.c_str(), path_str.size()});
            if (!bytes.empty())
            {
                _legacy_bytecode_cache[key] = bytes;
                return bytes;
            }
        }
        return {};
    }

    auto shader_manager::create_shader_module_desc(string_view shader_filename, rhi::shader_stage stage,
                                                   string_view entry_point) -> optional<rhi::shader_module_desc>
    {
        const auto key = string{shader_filename.data(), shader_filename.size()};
        auto it = _legacy_bytecode_cache.find(key);
        if (it == _legacy_bytecode_cache.end())
        {
            auto bytes = load_shader_bytecode(shader_filename);
            if (bytes.empty())
            {
                return nullopt;
            }
            it = _legacy_bytecode_cache.find(key);
        }

        if (it == _legacy_bytecode_cache.end() || it->second.empty())
        {
            return nullopt;
        }

        return rhi::shader_module_desc{
            .stage = stage,
            .ir_code = span<const byte>{it->second.data(), it->second.size()},
            .entry_point = cstring_view{entry_point.data(), entry_point.size()},
            .specialization_constants = {},
        };
    }

    auto shader_manager::get_or_create_graphics_pipeline(string_view key, const rhi::graphics_pipeline_desc& desc)
        -> rhi::graphics_pipeline_handle
    {
        const auto key_str = string{key.data(), key.size()};
        auto it = _named_graphics_pipelines.find(key_str);
        if (it != _named_graphics_pipelines.end())
        {
            return get_rhi_pipeline(it->second);
        }

        if (!_device)
        {
            return {};
        }

        auto handle = _device->create_graphics_pipeline(desc);
        if (handle.handle != 0)
        {
            const auto handle_id = static_cast<uint32_t>(_graphics_pipelines.size());
            auto rec = graphics_pipeline_record{
                .name = key_str,
                .rhi_pipeline = handle,
            };
            _graphics_pipelines.push_back(tempest::move(rec));
            _named_graphics_pipelines[key_str] = graphics_pipeline_handle{handle_id};
        }
        return handle;
    }

    auto shader_manager::get_or_create_compute_pipeline(string_view key, const rhi::compute_pipeline_desc& desc)
        -> rhi::compute_pipeline_handle
    {
        const auto key_str = string{key.data(), key.size()};
        auto it = _named_compute_pipelines.find(key_str);
        if (it != _named_compute_pipelines.end())
        {
            return get_rhi_pipeline(it->second);
        }

        if (!_device)
        {
            return {};
        }

        auto handle = _device->create_compute_pipeline(desc);
        if (handle.handle != 0)
        {
            const auto handle_id = static_cast<uint32_t>(_compute_pipelines.size());
            auto rec = compute_pipeline_record{
                .name = key_str,
                .rhi_pipeline = handle,
            };
            _compute_pipelines.push_back(tempest::move(rec));
            _named_compute_pipelines[key_str] = compute_pipeline_handle{handle_id};
        }
        return handle;
    }
} // namespace tempest::render_system

