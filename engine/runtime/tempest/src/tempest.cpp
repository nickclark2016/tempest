#include <tempest/default_importers.hpp>
#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/render_system/renderer.hpp>
#include <tempest/rhi.hpp>
#include <tempest/tempest.hpp>

#include <chrono>
#include <clocale>
#include <cstdlib>
#include <iostream>
#include <locale>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tempest
{
    namespace
    {
        auto make_default_log_sinks()
        {
            auto sinks = vector<unique_ptr<log_sink>>{};
            sinks.push_back(make_unique<mt_stdout_log_sink>(log_level::trace, log_level::fatal));
            return sinks;
        }

        auto make_default_logger(span<unique_ptr<log_sink>> sinks)
        {
            auto logger = tempest::logger();
            for (auto& sink : sinks)
            {
                logger = tempest::logger(*sink);
            }
            return logger;
        }
    } // namespace

    standalone_engine_context::standalone_engine_context()
        : _log_sinks(make_default_log_sinks()), _logger(make_default_logger(_log_sinks)),
          _entity_registry(_event_registry), _asset_database(&_asset_type_reg)
    {
        if (::setlocale(LC_ALL, "en_US.UTF-8") == nullptr)
        {
            _logger.error("Failed to set locale to UTF-8. Logging may not work correctly.");
        }

        try
        {
            std::locale utf8_locale("en_US.UTF-8");
            std::locale::global(utf8_locale);
            std::cin.imbue(utf8_locale);
            std::cout.imbue(utf8_locale);
            std::cerr.imbue(utf8_locale);
        }
        catch (const std::runtime_error&)
        {
            _logger.error("Failed to set global locale to UTF-8. Logging may not work correctly.");
        }

#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif

        assets::register_default_importers(_asset_database, &_mesh_reg, &_texture_reg, &_material_reg);

        auto ctx_desc = rhi::context_desc{
            .application_name = "Tempest Engine",
            .version_major = 1,
            .version_minor = 0,
            .version_patch = 0,
            .enable_api_validation = true,
            .api = rhi::graphics_api::vulkan,
        };

        auto ctx_res = rhi::create_context(ctx_desc);
        if (ctx_res.has_value())
        {
            _rhi_context = tempest::move(ctx_res).value();
            auto devices = _rhi_context->enumerate_devices();
            if (!devices.empty())
            {
                _device = _rhi_context->create_device(devices[0].device_uuid);
            }
        }

        if (_device)
        {
            auto builder = render_system::renderer::builder{};
            builder.set_config(render_system::renderer_config{
                .render_width = 1920,
                .render_height = 1080,
                .tonemapped_color_format = rhi::data_format::rgba8_srgb,
            });
            builder.set_inputs(render_system::renderer_inputs{
                .entity_registry = &_entity_registry,
                .meshes = &_mesh_reg,
                .textures = &_texture_reg,
                .materials = &_material_reg,
            });
            _renderer = builder.build(*_device, _logger);
        }
    }

    standalone_engine_context::~standalone_engine_context()
    {
        if (_device)
        {
            _device->wait_idle();
            _renderer.reset();

            for (auto& win : _windows)
            {
                if (win.acquire_sem.handle != 0)
                {
                    _device->destroy_semaphore(win.acquire_sem);
                    win.acquire_sem = {};
                }
                if (win.timeline_sem.handle != 0)
                {
                    _device->destroy_semaphore(win.timeline_sem);
                    win.timeline_sem = {};
                }
                for (auto sem : win.render_semaphores)
                {
                    _device->destroy_semaphore(sem);
                }
                win.render_semaphores.clear();

                if (win.render_surface)
                {
                    _device->destroy_render_surface(tempest::move(win.render_surface));
                }
                if (win.raw_surface.handle != 0)
                {
                    _device->destroy_raw_surface(win.raw_surface);
                    win.raw_surface = {};
                }
            }
            _windows.clear();
        }
    }

    auto standalone_engine_context::register_window(window_desc desc, [[maybe_unused]] bool install_swapchain_blit)
        -> window_registration_info
    {
        auto handle = _window_manager.create_window(desc);
        if (!handle.is_valid())
        {
            return {};
        }

        if (!_device)
        {
            return window_registration_info{
                .handle = handle,
                .inputs = _window_manager.get_input_group(handle),
            };
        }

        auto native_handle = _window_manager.get_native_wsi_handle(handle);
        auto raw_res = _device->create_raw_surface(native_handle);
        if (!raw_res.has_value())
        {
            _window_manager.destroy_window(handle);
            return {};
        }
        auto raw_surf = raw_res.value();

        auto caps = _device->get_surface_capabilities(raw_surf);
        auto selected_surface_format = optional<rhi::surface_format>{};
        for (const auto& fmt : caps.supported_formats)
        {
            if (fmt.color_space == rhi::surface_color_space::srgb_nonlinear &&
                (fmt.format == rhi::render_surface_format::bgra8_srgb ||
                 fmt.format == rhi::render_surface_format::rgba8_srgb))
            {
                selected_surface_format = fmt;
                break;
            }
        }
        if (!selected_surface_format && !caps.supported_formats.empty())
        {
            selected_surface_format = caps.supported_formats[0];
        }

        auto selected_present_mode = rhi::present_mode::vsync;
        for (const auto& mode : caps.supported_present_modes)
        {
            if (mode == rhi::present_mode::vsync)
            {
                selected_present_mode = mode;
                break;
            }
        }

        const auto surface_format_val = selected_surface_format.value_or(rhi::surface_format{
            .format = rhi::render_surface_format::bgra8_srgb,
            .color_space = rhi::surface_color_space::srgb_nonlinear,
        });

        const auto cur_fb_w = _window_manager.get_framebuffer_width(handle);
        const auto cur_fb_h = _window_manager.get_framebuffer_height(handle);

        auto surf_desc = rhi::render_surface_desc{
            .raw_surface = raw_surf,
            .present_mode = selected_present_mode,
            .format = surface_format_val,
            .width = (cur_fb_w > 0) ? cur_fb_w : desc.width,
            .height = (cur_fb_h > 0) ? cur_fb_h : desc.height,
            .min_image_count = caps.min_image_count,
            .preferred_image_count = caps.min_image_count + 1,
        };

        auto surface = _device->create_render_surface(surf_desc);
        if (!surface)
        {
            _device->destroy_raw_surface(raw_surf);
            _window_manager.destroy_window(handle);
            return {};
        }

        auto acquire_sem = _device->create_binary_semaphore();
        auto timeline_sem = _device->create_timeline_semaphore();

        _window_manager.register_resize_callback(
            handle, [this, handle]([[maybe_unused]] uint32_t w, [[maybe_unused]] uint32_t h) {
                for (auto& win : _windows)
                {
                    if (win.handle == handle)
                    {
                        win.need_recreate = true;
                        break;
                    }
                }
            });

        _windows.push_back(window_context{
            .handle = handle,
            .raw_surface = raw_surf,
            .render_surface = tempest::move(surface),
            .surface_format = surface_format_val,
            .present_mode = selected_present_mode,
            .acquire_sem = acquire_sem,
            .timeline_sem = timeline_sem,
            .timeline_value = 0,
            .render_semaphores = {},
            .need_recreate = false,
        });

        return window_registration_info{
            .handle = handle,
            .inputs = _window_manager.get_input_group(handle),
        };
    }

    auto standalone_engine_context::register_on_initialize_callback(function<void(engine_context&)> callback) -> void
    {
        _on_initialize_callbacks.push_back(tempest::move(callback));
    }

    auto standalone_engine_context::register_on_close_callback(function<void(engine_context&)> callback) -> void
    {
        _on_close_callbacks.push_back(tempest::move(callback));
    }

    auto standalone_engine_context::register_on_fixed_update_callback(
        function<void(engine_context&, std::chrono::duration<float>)> callback) -> void
    {
        _on_fixed_update_callbacks.push_back(tempest::move(callback));
    }

    auto standalone_engine_context::register_on_variable_update_callback(
        function<void(engine_context&, std::chrono::duration<float>)> callback) -> void
    {
        _on_variable_update_callbacks.push_back(tempest::move(callback));
    }

    auto standalone_engine_context::request_close(bool close) -> void
    {
        _should_close = close;
    }

    auto standalone_engine_context::should_close() const -> bool
    {
        return _should_close;
    }

    auto standalone_engine_context::load_entity(ecs::entity src) -> ecs::entity
    {
        return _entity_registry.duplicate(src);
    }

    auto standalone_engine_context::run() -> void
    {
        _logger.trace("Starting engine");

        _logger.trace("Running initialization callbacks");
        for (auto&& init_cb : _on_initialize_callbacks)
        {
            init_cb(*this);
        }
        _logger.trace("Finished initialization callbacks");

        auto simulated_time = std::chrono::duration<double>(0.0);
        auto delta_time = std::chrono::duration<double>(1.0 / 60.0);

        auto current_time = std::chrono::steady_clock::now();
        auto accumulator = std::chrono::duration<double>(0.0);
        _last_frame_time = current_time;

        _logger.trace("Starting main loop");
        while (!_should_close)
        {
            auto frame_start_time = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::duration<float>>(frame_start_time - _last_frame_time);
            _delta_frame_time = delta;
            _last_frame_time = frame_start_time;

            auto new_time = std::chrono::steady_clock::now();
            auto frame_time = new_time - current_time;
            current_time = new_time;

            accumulator += frame_time;

            while (accumulator >= delta_time)
            {
                _update_fixed(std::chrono::duration_cast<std::chrono::duration<float>>(delta_time));
                if (_should_close)
                {
                    goto exit_main_loop;
                }

                simulated_time += delta_time;
                accumulator -= delta_time;
            }

            _update_variable(_delta_frame_time);
            _render_frame();
        }

    exit_main_loop:
        _logger.trace("Exiting main loop");

        if (_device)
        {
            _device->wait_idle();
        }

        _logger.trace("Running close callbacks");
        for (auto&& close_cb : _on_close_callbacks)
        {
            close_cb(*this);
        }
        _logger.trace("Finished close callbacks");
    }

    auto standalone_engine_context::get_entities() -> ecs::archetype_registry&
    {
        return _entity_registry;
    }

    auto standalone_engine_context::get_entities() const -> const ecs::archetype_registry&
    {
        return _entity_registry;
    }

    auto standalone_engine_context::get_events() -> event::event_registry&
    {
        return _event_registry;
    }

    auto standalone_engine_context::get_events() const -> const event::event_registry&
    {
        return _event_registry;
    }

    auto standalone_engine_context::get_materials() -> core::material_registry&
    {
        return _material_reg;
    }

    auto standalone_engine_context::get_materials() const -> const core::material_registry&
    {
        return _material_reg;
    }

    auto standalone_engine_context::get_meshes() -> core::mesh_registry&
    {
        return _mesh_reg;
    }

    auto standalone_engine_context::get_meshes() const -> const core::mesh_registry&
    {
        return _mesh_reg;
    }

    auto standalone_engine_context::get_textures() -> core::texture_registry&
    {
        return _texture_reg;
    }

    auto standalone_engine_context::get_textures() const -> const core::texture_registry&
    {
        return _texture_reg;
    }

    auto standalone_engine_context::get_assets() -> assets::asset_database&
    {
        return _asset_database;
    }

    auto standalone_engine_context::get_assets() const -> const assets::asset_database&
    {
        return _asset_database;
    }

    auto standalone_engine_context::get_renderer() -> render_system::renderer&
    {
        return *_renderer;
    }

    auto standalone_engine_context::get_renderer() const -> const render_system::renderer&
    {
        return *_renderer;
    }

    auto standalone_engine_context::get_device() -> rhi::device&
    {
        return *_device;
    }

    auto standalone_engine_context::get_device() const -> const rhi::device&
    {
        return *_device;
    }

    auto standalone_engine_context::get_window_manager() -> window_manager&
    {
        return _window_manager;
    }

    auto standalone_engine_context::get_window_manager() const -> const window_manager&
    {
        return _window_manager;
    }

    auto standalone_engine_context::get_render_surface(window_handle win) -> rhi::render_surface*
    {
        for (auto& w : _windows)
        {
            if (w.handle == win)
            {
                return w.render_surface.get();
            }
        }
        return nullptr;
    }

    auto standalone_engine_context::get_render_surface(window_handle win) const -> const rhi::render_surface*
    {
        for (const auto& w : _windows)
        {
            if (w.handle == win)
            {
                return w.render_surface.get();
            }
        }
        return nullptr;
    }

    auto standalone_engine_context::get_raw_surface(window_handle win) const -> rhi::raw_surface_handle
    {
        for (const auto& w : _windows)
        {
            if (w.handle == win)
            {
                return w.raw_surface;
            }
        }
        return {};
    }

    auto standalone_engine_context::_update_fixed(std::chrono::duration<float> delta_time) -> void
    {
        for (auto& win : _windows)
        {
            auto& mouse = _window_manager.get_mouse(win.handle);
            mouse.reset_mouse_deltas();
            mouse.set_disabled(_window_manager.is_cursor_disabled(win.handle));
        }

        _window_manager.poll_events();

        for (auto it = _windows.begin(); it != _windows.end();)
        {
            if (_window_manager.should_close(it->handle))
            {
                if (_device)
                {
                    if (it->acquire_sem.handle != 0)
                    {
                        _device->destroy_semaphore(it->acquire_sem);
                    }
                    if (it->timeline_sem.handle != 0)
                    {
                        _device->destroy_semaphore(it->timeline_sem);
                    }
                    for (auto sem : it->render_semaphores)
                    {
                        _device->destroy_semaphore(sem);
                    }
                    if (it->render_surface)
                    {
                        _device->destroy_render_surface(tempest::move(it->render_surface));
                    }
                    if (it->raw_surface.handle != 0)
                    {
                        _device->destroy_raw_surface(it->raw_surface);
                    }
                }
                _window_manager.destroy_window(it->handle);
                it = _windows.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (_windows.empty())
        {
            _should_close = true;
            return;
        }

        for (auto&& callback : _on_fixed_update_callbacks)
        {
            callback(*this, delta_time);
        }
    }

    auto standalone_engine_context::_update_variable(std::chrono::duration<float> delta_time) -> void
    {
        for (auto&& callback : _on_variable_update_callbacks)
        {
            callback(*this, delta_time);
        }
    }

    auto standalone_engine_context::_render_frame() -> void
    {
        if (!_renderer || !_device || _windows.empty())
        {
            return;
        }

        auto& win = _windows.front();
        if (!win.render_surface)
        {
            return;
        }

        const auto cur_w = _window_manager.get_framebuffer_width(win.handle);
        const auto cur_h = _window_manager.get_framebuffer_height(win.handle);

        if (cur_w == 0 || cur_h == 0)
        {
            // Window is minimized
            return;
        }

        if (cur_w != win.render_surface->get_width() || cur_h != win.render_surface->get_height() || win.need_recreate)
        {
            _device->wait_idle();

            auto caps = _device->get_surface_capabilities(win.raw_surface);
            auto new_surf_desc = rhi::render_surface_desc{
                .raw_surface = win.raw_surface,
                .present_mode = win.present_mode,
                .format = win.surface_format,
                .width = cur_w,
                .height = cur_h,
                .min_image_count = caps.min_image_count,
                .preferred_image_count = caps.min_image_count + 1,
                .old_surface = win.render_surface.get(),
            };

            auto new_surf = _device->create_render_surface(new_surf_desc);
            if (new_surf)
            {
                _device->destroy_render_surface(tempest::move(win.render_surface));
                win.render_surface = tempest::move(new_surf);
                _renderer->resize(cur_w, cur_h);
                win.need_recreate = false;
            }
            else
            {
                return;
            }
        }

        if (win.timeline_value > 0)
        {
            _device->wait_for_sync(rhi::host_sync_point{
                .semaphore = win.timeline_sem,
                .value = win.timeline_value,
            });
        }

        auto acquire_res = win.render_surface->acquire_next_image(rhi::device_sync_point{
            .semaphore = win.acquire_sem,
            .value = 0,
            .stages = rhi::pipeline_stage::attachment_output,
        });

        if (!acquire_res.has_value())
        {
            if (acquire_res.error() == rhi::swapchain_error::out_of_date ||
                acquire_res.error() == rhi::swapchain_error::suboptimal)
            {
                win.need_recreate = true;
            }
            return;
        }

        auto sc_img = acquire_res.value();

        while (sc_img.swapchain_image_index >= win.render_semaphores.size())
        {
            win.render_semaphores.push_back(_device->create_binary_semaphore());
        }
        auto render_sem = win.render_semaphores[sc_img.swapchain_image_index];
        win.timeline_value++;

        _renderer->prepare_frame(cur_w, cur_h, sc_img.texture, sc_img.view);

        const auto sync = render_graph::frame_sync_options{
            .wait_semaphore = win.acquire_sem,
            .wait_stages = rhi::pipeline_stage::attachment_output,
            .signal_semaphore = render_sem,
            .signal_stages = rhi::pipeline_stage::bottom_of_pipe,
            .timeline_semaphore = win.timeline_sem,
            .timeline_value = win.timeline_value,
            .presented_texture = sc_img.texture,
        };

        static_cast<void>(_renderer->render(sync));

        auto& graphics_port = _device->get_graphics_execution_port();
        auto present_res = win.render_surface->present(graphics_port, rhi::device_sync_point{
                                                                          .semaphore = render_sem,
                                                                          .value = 0,
                                                                      });

        if (!present_res.has_value() && (present_res.error() == rhi::swapchain_error::out_of_date ||
                                         present_res.error() == rhi::swapchain_error::suboptimal))
        {
            win.need_recreate = true;
        }
    }
} // namespace tempest