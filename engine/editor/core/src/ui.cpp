#include <tempest/ui.hpp>

#include <tempest/array.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/enum.hpp>
#include <tempest/files.hpp>
#include <tempest/int.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vector.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <cstring>
#include <filesystem>

namespace tempest::editor
{
    namespace
    {
        auto convert_key(const core::key_state& key_state) noexcept -> ImGuiKey
        {
            switch (key_state.k)
            {
            case core::key::tab:
                return ImGuiKey_Tab;
            case core::key::dpad_left:
                return ImGuiKey_LeftArrow;
            case core::key::dpad_right:
                return ImGuiKey_RightArrow;
            case core::key::dpad_up:
                return ImGuiKey_UpArrow;
            case core::key::dpad_down:
                return ImGuiKey_DownArrow;
            case core::key::page_up:
                return ImGuiKey_PageUp;
            case core::key::page_down:
                return ImGuiKey_PageDown;
            case core::key::home:
                return ImGuiKey_Home;
            case core::key::end:
                return ImGuiKey_End;
            case core::key::insert:
                return ImGuiKey_Insert;
            case core::key::deletion:
                return ImGuiKey_Delete;
            case core::key::backspace:
                return ImGuiKey_Backspace;
            case core::key::space:
                return ImGuiKey_Space;
            case core::key::enter:
                return ImGuiKey_Enter;
            case core::key::escape:
                return ImGuiKey_Escape;
            case core::key::apostrophe:
                return ImGuiKey_Apostrophe;
            case core::key::comma:
                return ImGuiKey_Comma;
            case core::key::minus:
                return ImGuiKey_Minus;
            case core::key::period:
                return ImGuiKey_Period;
            case core::key::slash:
                return ImGuiKey_Slash;
            case core::key::semicolon:
                return ImGuiKey_Semicolon;
            case core::key::equal:
                return ImGuiKey_Equal;
            case core::key::left_bracket:
                return ImGuiKey_LeftBracket;
            case core::key::backslash:
                return ImGuiKey_Backslash;
            case core::key::right_bracket:
                return ImGuiKey_RightBracket;
            case core::key::grave_accent:
                return ImGuiKey_GraveAccent;
            case core::key::caps_lock:
                return ImGuiKey_CapsLock;
            case core::key::scroll_lock:
                return ImGuiKey_ScrollLock;
            case core::key::num_lock:
                return ImGuiKey_NumLock;
            case core::key::print_screen:
                return ImGuiKey_PrintScreen;
            case core::key::pause:
                return ImGuiKey_Pause;
            case core::key::tw_0:
                return ImGuiKey_0;
            case core::key::tw_1:
                return ImGuiKey_1;
            case core::key::tw_2:
                return ImGuiKey_2;
            case core::key::tw_3:
                return ImGuiKey_3;
            case core::key::tw_4:
                return ImGuiKey_4;
            case core::key::tw_5:
                return ImGuiKey_5;
            case core::key::tw_6:
                return ImGuiKey_6;
            case core::key::tw_7:
                return ImGuiKey_7;
            case core::key::tw_8:
                return ImGuiKey_8;
            case core::key::tw_9:
                return ImGuiKey_9;
            case core::key::a:
                return ImGuiKey_A;
            case core::key::b:
                return ImGuiKey_B;
            case core::key::c:
                return ImGuiKey_C;
            case core::key::d:
                return ImGuiKey_D;
            case core::key::e:
                return ImGuiKey_E;
            case core::key::f:
                return ImGuiKey_F;
            case core::key::g:
                return ImGuiKey_G;
            case core::key::h:
                return ImGuiKey_H;
            case core::key::i:
                return ImGuiKey_I;
            case core::key::j:
                return ImGuiKey_J;
            case core::key::k:
                return ImGuiKey_K;
            case core::key::l:
                return ImGuiKey_L;
            case core::key::m:
                return ImGuiKey_M;
            case core::key::n:
                return ImGuiKey_N;
            case core::key::o:
                return ImGuiKey_O;
            case core::key::p:
                return ImGuiKey_P;
            case core::key::q:
                return ImGuiKey_Q;
            case core::key::r:
                return ImGuiKey_R;
            case core::key::s:
                return ImGuiKey_S;
            case core::key::t:
                return ImGuiKey_T;
            case core::key::u:
                return ImGuiKey_U;
            case core::key::v:
                return ImGuiKey_V;
            case core::key::w:
                return ImGuiKey_W;
            case core::key::x:
                return ImGuiKey_X;
            case core::key::y:
                return ImGuiKey_Y;
            case core::key::z:
                return ImGuiKey_Z;
            case core::key::fn_1:
                return ImGuiKey_F1;
            case core::key::fn_2:
                return ImGuiKey_F2;
            case core::key::fn_3:
                return ImGuiKey_F3;
            case core::key::fn_4:
                return ImGuiKey_F4;
            case core::key::fn_5:
                return ImGuiKey_F5;
            case core::key::fn_6:
                return ImGuiKey_F6;
            case core::key::fn_7:
                return ImGuiKey_F7;
            case core::key::fn_8:
                return ImGuiKey_F8;
            case core::key::fn_9:
                return ImGuiKey_F9;
            case core::key::fn_10:
                return ImGuiKey_F10;
            case core::key::fn_11:
                return ImGuiKey_F11;
            case core::key::fn_12:
                return ImGuiKey_F12;
            case core::key::fn_13:
                return ImGuiKey_F13;
            case core::key::fn_14:
                return ImGuiKey_F14;
            case core::key::fn_15:
                return ImGuiKey_F15;
            case core::key::fn_16:
                return ImGuiKey_F16;
            case core::key::fn_17:
                return ImGuiKey_F17;
            case core::key::fn_18:
                return ImGuiKey_F18;
            case core::key::fn_19:
                return ImGuiKey_F19;
            case core::key::fn_20:
                return ImGuiKey_F20;
            case core::key::fn_21:
                return ImGuiKey_F21;
            case core::key::fn_22:
                return ImGuiKey_F22;
            case core::key::fn_23:
                return ImGuiKey_F23;
            case core::key::fn_24:
                return ImGuiKey_F24;
            case core::key::kp_0:
                return ImGuiKey_Keypad0;
            case core::key::kp_1:
                return ImGuiKey_Keypad1;
            case core::key::kp_2:
                return ImGuiKey_Keypad2;
            case core::key::kp_3:
                return ImGuiKey_Keypad3;
            case core::key::kp_4:
                return ImGuiKey_Keypad4;
            case core::key::kp_5:
                return ImGuiKey_Keypad5;
            case core::key::kp_6:
                return ImGuiKey_Keypad6;
            case core::key::kp_7:
                return ImGuiKey_Keypad7;
            case core::key::kp_8:
                return ImGuiKey_Keypad8;
            case core::key::kp_9:
                return ImGuiKey_Keypad9;
            case core::key::kp_decimal:
                return ImGuiKey_KeypadDecimal;
            case core::key::kp_divide:
                return ImGuiKey_KeypadDivide;
            case core::key::kp_multiply:
                return ImGuiKey_KeypadMultiply;
            case core::key::kp_subtract:
                return ImGuiKey_KeypadSubtract;
            case core::key::kp_add:
                return ImGuiKey_KeypadAdd;
            case core::key::kp_enter:
                return ImGuiKey_KeypadEnter;
            case core::key::kp_equal:
                return ImGuiKey_KeypadEqual;
            case core::key::left_shift:
                return ImGuiKey_LeftShift;
            case core::key::left_control:
                return ImGuiKey_LeftCtrl;
            case core::key::left_alt:
                return ImGuiKey_LeftAlt;
            case core::key::left_super:
                return ImGuiKey_LeftSuper;
            case core::key::right_shift:
                return ImGuiKey_RightShift;
            case core::key::right_control:
                return ImGuiKey_RightCtrl;
            case core::key::right_alt:
                return ImGuiKey_RightAlt;
            case core::key::right_super:
                return ImGuiKey_RightSuper;
            case core::key::menu:
                return ImGuiKey_Menu;
            default:
                return ImGuiKey_None;
            }
        }

        auto load_shader_bytecode(const assets::asset_database& db, string_view filename) -> vector<byte>
        {
            const auto* asset = db.find_asset(filename);
            if (asset == nullptr)
            {
                auto aliased = string("@shaders/");
                aliased.append(filename.data(), filename.size());
                asset = db.find_asset(aliased);
            }
            if (asset != nullptr)
            {
                auto blob = db.get_blob(asset->id);
                if (!blob.empty())
                {
                    auto result = vector<byte>{};
                    unsafe::resize_no_init(result, blob.size());
                    tempest::memcpy(result.data(), blob.data(), blob.size());
                    return result;
                }
            }

            return {};
        }

        struct push_constants
        {
            math::float2 scale;
            math::float2 translate;
            uint64_t vertex_buffer;
            uint32_t texture_id;
            uint32_t sampler_id;
        };
    } // namespace

    struct per_frame_buffer
    {
        rhi::buffer_handle vertex_buffer{};
        rhi::buffer_handle index_buffer{};
        size_t vertex_count{0};
        size_t index_count{0};
    };

    struct ui_context::impl
    {
        window_manager* win_mgr{nullptr};
        window_handle win{null_window_handle};
        rhi::device* dev{nullptr};
        assets::asset_database* asset_db{nullptr};
        rhi::data_format target_format{rhi::data_format::unknown};
        uint32_t frames_in_flight{1};

        ImGuiContext* imgui_context{nullptr};

        rhi::texture_handle font_texture{};
        rhi::texture_view_handle font_view{};
        rhi::descriptor_handle font_descriptor{};

        rhi::sampler_handle linear_sampler{};
        rhi::descriptor_handle linear_sampler_descriptor{};

        rhi::graphics_pipeline_handle pipeline{};

        vector<per_frame_buffer> frame_buffers;
        uint32_t current_frame_index{0};

        std::chrono::steady_clock::time_point last_time;
        ImVec2 last_mouse_pos{0.0f, 0.0f};

        auto init_input_callbacks() -> void;
        auto init_render_backend() -> void;
        auto setup_font_texture() -> void;
    };

    auto ui_context::impl::init_input_callbacks() -> void
    {
        if (!win_mgr || !win.is_valid())
        {
            return;
        }

        win_mgr->register_key_callback(win, [this](const core::key_state& key_state) {
            if (key_state.action != core::key_action::press && key_state.action != core::key_action::release)
            {
                return;
            }
            ImGui::SetCurrentContext(imgui_context);
            auto& io = ImGui::GetIO();
            auto key = convert_key(key_state);
            io.AddKeyEvent(key, key_state.action == core::key_action::press);
            io.AddKeyEvent(ImGuiKey_ModShift, core::test_modifier(key_state, core::key_modifier::shift));
            io.AddKeyEvent(ImGuiKey_ModCtrl, core::test_modifier(key_state, core::key_modifier::control));
            io.AddKeyEvent(ImGuiKey_ModAlt, core::test_modifier(key_state, core::key_modifier::alt));
            io.AddKeyEvent(ImGuiKey_ModSuper, core::test_modifier(key_state, core::key_modifier::super));
        });

        win_mgr->register_char_callback(win, [this](uint32_t codepoint) {
            ImGui::SetCurrentContext(imgui_context);
            auto& io = ImGui::GetIO();
            io.AddInputCharacter(codepoint);
        });

        win_mgr->register_mouse_button_callback(win, [this](const core::mouse_button_state& mouse_state) {
            if (mouse_state.action != core::mouse_action::press && mouse_state.action != core::mouse_action::release)
            {
                return;
            }
            ImGui::SetCurrentContext(imgui_context);
            auto& io = ImGui::GetIO();
            auto button = -1;
            switch (mouse_state.button)
            {
            case core::mouse_button::mb_1:
                button = ImGuiMouseButton_Left;
                break;
            case core::mouse_button::mb_2:
                button = ImGuiMouseButton_Right;
                break;
            case core::mouse_button::mb_3:
                button = ImGuiMouseButton_Middle;
                break;
            default:
                return;
            }
            if (button >= 0 && button < ImGuiMouseButton_COUNT)
            {
                io.AddMouseButtonEvent(button, mouse_state.action == core::mouse_action::press);
            }
        });

        win_mgr->register_cursor_pos_callback(win, [this](float x, float y) {
            ImGui::SetCurrentContext(imgui_context);
            auto& io = ImGui::GetIO();
            io.AddMousePosEvent(x, y);
            last_mouse_pos = ImVec2(x, y);
        });

        win_mgr->register_scroll_callback(win, [this](float x_offset, float y_offset) {
            ImGui::SetCurrentContext(imgui_context);
            auto& io = ImGui::GetIO();
            io.AddMouseWheelEvent(x_offset, y_offset);
        });

        win_mgr->register_focus_callback(win, [this](bool focused) {
            ImGui::SetCurrentContext(imgui_context);
            auto& io = ImGui::GetIO();
            io.AddFocusEvent(focused);
        });
    }

    auto ui_context::impl::setup_font_texture() -> void
    {
        if (!dev)
        {
            return;
        }

        ImGui::SetCurrentContext(imgui_context);
        auto& io = ImGui::GetIO();

        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        const auto upload_size = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4;

        auto tex_desc = rhi::texture_desc{
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = rhi::data_format::rgba8_unorm,
            .memory_usage = rhi::memory_usage::device_only,
            .usage = rhi::texture_usage::sampled | rhi::texture_usage::transfer_dst,
            .name = "ImGui Font Texture",
        };
        font_texture = dev->create_texture(tex_desc);

        auto upload_desc = rhi::buffer_desc{
            .size = upload_size,
            .memory_usage = rhi::memory_usage::upload,
            .usage = rhi::buffer_usage::transfer_src,
            .name = "ImGui Font Upload Buffer",
        };
        auto upload_buf = dev->create_buffer(upload_desc);
        std::memcpy(upload_buf.cpu_address, pixels, upload_size);

        auto& graphics_port = dev->get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, rhi::command_list_lifetime::transient);

        cmd.begin();

        auto pre_barrier = rhi::texture_barrier{
            .texture = font_texture,
            .src =
                {
                    .stages = rhi::pipeline_stage::top_of_pipe,
                    .access = rhi::resource_access::none,
                    .layout = rhi::image_layout::undefined,
                },
            .dst =
                {
                    .stages = rhi::pipeline_stage::all_transfer,
                    .access = rhi::resource_access::write,
                    .layout = rhi::image_layout::general,
                },
        };
        cmd.pipeline_barrier(span<const rhi::texture_barrier>{&pre_barrier, 1}, {});

        auto copy_region = rhi::buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = static_cast<uint32_t>(width),
            .buffer_image_height = static_cast<uint32_t>(height),
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = static_cast<uint32_t>(width),
            .image_extent_height = static_cast<uint32_t>(height),
            .image_extent_depth = 1,
        };
        cmd.copy_buffer_to_texture(upload_buf, font_texture,
                                   span<const rhi::buffer_texture_copy_region>{&copy_region, 1});

        auto post_barrier = rhi::texture_barrier{
            .texture = font_texture,
            .src =
                {
                    .stages = rhi::pipeline_stage::all_transfer,
                    .access = rhi::resource_access::write,
                    .layout = rhi::image_layout::general,
                },
            .dst =
                {
                    .stages = rhi::pipeline_stage::fragment,
                    .access = rhi::resource_access::read,
                    .layout = rhi::image_layout::general,
                },
        };
        cmd.pipeline_barrier(span<const rhi::texture_barrier>{&post_barrier, 1}, {});

        cmd.end();

        auto timeline_sem = dev->create_timeline_semaphore();
        auto signal_point = rhi::device_sync_point{
            .semaphore = timeline_sem,
            .value = 1,
            .stages = rhi::pipeline_stage::all_transfer,
        };

        const auto* cmd_ptr = &cmd;
        auto submit_res = graphics_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                               span<const rhi::device_sync_point>{&signal_point, 1});
        if (submit_res.has_value())
        {
            dev->wait_for_sync(rhi::host_sync_point{.semaphore = timeline_sem, .value = 1});
        }

        dev->destroy_semaphore(timeline_sem);
        dev->destroy_buffer(upload_buf);

        font_view = dev->create_texture_view(font_texture, rhi::texture_view_desc{});
        font_descriptor = dev->allocate_descriptor(rhi::descriptor_type::sampled_image);
        dev->write_sampled_image_descriptor(font_descriptor, font_view, rhi::image_layout::general);

        io.Fonts->SetTexID(static_cast<ImTextureID>(static_cast<uintptr_t>(font_descriptor.index)));
    }

    auto ui_context::impl::init_render_backend() -> void
    {
        if (!dev)
        {
            return;
        }

        // Create linear clamp sampler and its descriptor
        auto samp_desc = rhi::sampler_desc{
            .min_filter = rhi::filter_mode::linear,
            .mag_filter = rhi::filter_mode::linear,
            .mipmap_mode = rhi::mipmap_mode::linear,
            .address_u = rhi::address_mode::clamp_to_edge,
            .address_v = rhi::address_mode::clamp_to_edge,
            .address_w = rhi::address_mode::clamp_to_edge,
            .name = "ImGui Linear Sampler",
        };
        linear_sampler = dev->create_sampler(samp_desc);
        linear_sampler_descriptor = dev->allocate_descriptor(rhi::descriptor_type::sampler);
        dev->write_sampler_descriptor(linear_sampler_descriptor, linear_sampler);

        // Load Slang-compiled shaders
        auto vs_bytes = asset_db ? load_shader_bytecode(*asset_db, "imgui.vert.spv") : vector<byte>{};
        auto fs_bytes = asset_db ? load_shader_bytecode(*asset_db, "imgui.frag.spv") : vector<byte>{};

        if (!vs_bytes.empty() && !fs_bytes.empty())
        {
            auto vs_desc = rhi::shader_module_desc{
                .stage = rhi::shader_stage::vertex,
                .ir_code = span<const byte>{vs_bytes.data(), vs_bytes.size()},
                .entry_point = "VSMain",
            };
            auto fs_desc = rhi::shader_module_desc{
                .stage = rhi::shader_stage::fragment,
                .ir_code = span<const byte>{fs_bytes.data(), fs_bytes.size()},
                .entry_point = "FSMain",
            };

            auto stages = array{vs_desc, fs_desc};
            auto color_formats = array{target_format};
            auto blend_state = rhi::attachment_blend_state{
                .blend_enable = true,
                .src_color_blend_factor = rhi::blend_factor::src_alpha,
                .dst_color_blend_factor = rhi::blend_factor::one_minus_src_alpha,
                .src_alpha_blend_factor = rhi::blend_factor::one,
                .dst_alpha_blend_factor = rhi::blend_factor::one_minus_src_alpha,
            };
            auto blend_states = array{blend_state};

            auto pipe_desc = rhi::graphics_pipeline_desc{
                .shader_modules = span<const rhi::shader_module_desc>{stages.data(), stages.size()},
                .color_attachment_formats = span<const rhi::data_format>{color_formats.data(), color_formats.size()},
                .depth_stencil_attachment_format = nullopt,
                .primitive_topology = rhi::primitive_topology::triangle_list,
                .rasterization_state =
                    {
                        .polygon_mode = rhi::polygon_mode::fill,
                        .cull_mode = rhi::cull_mode::none,
                        .front_face = rhi::vertex_winding_order::counter_clockwise,
                    },
                .depth_stencil_state =
                    {
                        .depth_test_enable = false,
                        .depth_write_enable = false,
                    },
                .color_attachment_blend_states =
                    span<const rhi::attachment_blend_state>{blend_states.data(), blend_states.size()},
                .name = "ImGui Graphics Pipeline",
            };

            pipeline = dev->create_graphics_pipeline(pipe_desc);
        }

        // Initialize font texture
        setup_font_texture();

        // Allocate per-frame buffers
        frame_buffers.resize(frames_in_flight);
    }

    ui_context::ui_context(window_manager& win_mgr, window_handle win, rhi::device& device,
                           assets::asset_database& asset_db, rhi::data_format target_format, uint32_t frames_in_flight)
        : _impl(make_unique<impl>())
    {
        _impl->win_mgr = &win_mgr;
        _impl->win = win;
        _impl->dev = &device;
        _impl->asset_db = &asset_db;
        _impl->target_format = target_format;
        _impl->frames_in_flight = tempest::max(1u, frames_in_flight);

        IMGUI_CHECKVERSION();
        _impl->imgui_context = ImGui::CreateContext();
        ImGui::SetCurrentContext(_impl->imgui_context);

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.BackendPlatformName = "tempest_window_manager";
        io.BackendRendererName = "tempest_rhi_vk_slang";

        _impl->last_time = std::chrono::steady_clock::now();

        _impl->init_input_callbacks();
        _impl->init_render_backend();
    }

    ui_context::~ui_context()
    {
        if (_impl)
        {
            if (_impl->dev)
            {
                if (_impl->font_descriptor.index != ~0U)
                {
                    _impl->dev->free_descriptor(rhi::descriptor_type::sampled_image, _impl->font_descriptor);
                }
                if (_impl->linear_sampler_descriptor.index != ~0U)
                {
                    _impl->dev->free_descriptor(rhi::descriptor_type::sampler, _impl->linear_sampler_descriptor);
                }
                if (_impl->font_view.handle != 0)
                {
                    _impl->dev->destroy_texture_view(_impl->font_view);
                }
                if (_impl->font_texture.handle != 0)
                {
                    _impl->dev->destroy_texture(_impl->font_texture);
                }
                if (_impl->linear_sampler.handle != 0)
                {
                    _impl->dev->destroy_sampler(_impl->linear_sampler);
                }
                if (_impl->pipeline.handle != 0)
                {
                    _impl->dev->destroy_graphics_pipeline(_impl->pipeline);
                }
                for (auto& fb : _impl->frame_buffers)
                {
                    if (fb.vertex_buffer.handle != 0)
                    {
                        _impl->dev->destroy_buffer(fb.vertex_buffer);
                    }
                    if (fb.index_buffer.handle != 0)
                    {
                        _impl->dev->destroy_buffer(fb.index_buffer);
                    }
                }
            }

            if (_impl->imgui_context)
            {
                ImGui::DestroyContext(_impl->imgui_context);
                _impl->imgui_context = nullptr;
            }
        }
    }

    auto ui_context::begin_ui_commands() -> void
    {
        ImGui::SetCurrentContext(_impl->imgui_context);
        auto& io = ImGui::GetIO();

        if (_impl->win_mgr && _impl->win.is_valid())
        {
            auto width = _impl->win_mgr->get_width(_impl->win);
            auto height = _impl->win_mgr->get_height(_impl->win);
            auto fb_width = _impl->win_mgr->get_framebuffer_width(_impl->win);
            auto fb_height = _impl->win_mgr->get_framebuffer_height(_impl->win);

            io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
            if (width > 0 && height > 0)
            {
                io.DisplayFramebufferScale = ImVec2(static_cast<float>(fb_width) / static_cast<float>(width),
                                                    static_cast<float>(fb_height) / static_cast<float>(height));
            }
            else
            {
                io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            }
        }

        auto current_time = std::chrono::steady_clock::now();
        auto dt = std::chrono::duration<float>(current_time - _impl->last_time).count();
        io.DeltaTime = dt > 0.0f ? dt : (1.0f / 60.0f);
        _impl->last_time = current_time;

        ImGui::NewFrame();
    }

    auto ui_context::finish_ui_commands() -> void
    {
        ImGui::SetCurrentContext(_impl->imgui_context);
        ImGui::Render();
    }

    auto ui_context::get_imgui_context() const noexcept -> ImGuiContext*
    {
        return _impl ? _impl->imgui_context : nullptr;
    }

    auto ui_context::render_ui_commands(rhi::command_list& cmd, uint32_t width, uint32_t height) -> void
    {
        ImGui::SetCurrentContext(_impl->imgui_context);
        auto* draw_data = ImGui::GetDrawData();
        if (!draw_data || draw_data->TotalVtxCount <= 0 || width == 0 || height == 0 || _impl->pipeline.handle == 0)
        {
            return;
        }

        _impl->current_frame_index = (_impl->current_frame_index + 1) % _impl->frames_in_flight;
        auto& fb = _impl->frame_buffers[_impl->current_frame_index];

        const auto required_vtx_count = static_cast<size_t>(draw_data->TotalVtxCount);
        const auto required_idx_count = static_cast<size_t>(draw_data->TotalIdxCount);

        if (fb.vertex_count < required_vtx_count || fb.vertex_buffer.handle == 0)
        {
            if (fb.vertex_buffer.handle != 0)
            {
                _impl->dev->destroy_buffer(fb.vertex_buffer);
            }
            fb.vertex_count = required_vtx_count + 1024;
            auto vtx_desc = rhi::buffer_desc{
                .size = fb.vertex_count * sizeof(ImDrawVert),
                .memory_usage = rhi::memory_usage::upload,
                .usage = rhi::buffer_usage::vertex_buffer | rhi::buffer_usage::storage_buffer |
                         rhi::buffer_usage::device_address,
                .name = "ImGui Vertex Buffer",
            };
            fb.vertex_buffer = _impl->dev->create_buffer(vtx_desc);
        }

        if (fb.index_count < required_idx_count || fb.index_buffer.handle == 0)
        {
            if (fb.index_buffer.handle != 0)
            {
                _impl->dev->destroy_buffer(fb.index_buffer);
            }
            fb.index_count = required_idx_count + 2048;
            auto idx_desc = rhi::buffer_desc{
                .size = fb.index_count * sizeof(ImDrawIdx),
                .memory_usage = rhi::memory_usage::upload,
                .usage = rhi::buffer_usage::index_buffer | rhi::buffer_usage::storage_buffer |
                         rhi::buffer_usage::device_address,
                .name = "ImGui Index Buffer",
            };
            fb.index_buffer = _impl->dev->create_buffer(idx_desc);
        }

        auto* vtx_dst = static_cast<ImDrawVert*>(fb.vertex_buffer.cpu_address);
        auto* idx_dst = static_cast<ImDrawIdx*>(fb.index_buffer.cpu_address);

        for (int n = 0; n < draw_data->CmdListsCount; ++n)
        {
            const auto* cmd_list = draw_data->CmdLists[n];
            std::memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            std::memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        cmd.bind_pipeline(_impl->pipeline);
        cmd.bind_index_buffer(fb.index_buffer,
                              sizeof(ImDrawIdx) == 2 ? rhi::index_type::uint16 : rhi::index_type::uint32, 0);
        cmd.set_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);

        const auto clip_off = draw_data->DisplayPos;
        const auto clip_scale = draw_data->FramebufferScale;
        auto global_vtx_offset = 0u;
        auto global_idx_offset = 0u;

        for (int n = 0; n < draw_data->CmdListsCount; ++n)
        {
            const auto* cmd_list = draw_data->CmdLists[n];
            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i)
            {
                const auto& pcmd = cmd_list->CmdBuffer[cmd_i];
                if (pcmd.UserCallback != nullptr)
                {
                    if (pcmd.UserCallback == ImDrawCallback_ResetRenderState)
                    {
                        cmd.bind_pipeline(_impl->pipeline);
                        cmd.bind_index_buffer(
                            fb.index_buffer, sizeof(ImDrawIdx) == 2 ? rhi::index_type::uint16 : rhi::index_type::uint32,
                            0);
                        cmd.set_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
                    }
                    else
                    {
                        pcmd.UserCallback(cmd_list, &pcmd);
                    }
                }
                else
                {
                    auto clip_min = math::float2((pcmd.ClipRect.x - clip_off.x) * clip_scale.x,
                                                 (pcmd.ClipRect.y - clip_off.y) * clip_scale.y);
                    auto clip_max = math::float2((pcmd.ClipRect.z - clip_off.x) * clip_scale.x,
                                                 (pcmd.ClipRect.w - clip_off.y) * clip_scale.y);

                    if (clip_min.x < 0.0f)
                        clip_min.x = 0.0f;
                    if (clip_min.y < 0.0f)
                        clip_min.y = 0.0f;
                    if (clip_max.x > static_cast<float>(width))
                        clip_max.x = static_cast<float>(width);
                    if (clip_max.y > static_cast<float>(height))
                        clip_max.y = static_cast<float>(height);

                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    {
                        continue;
                    }

                    cmd.set_scissor(static_cast<int32_t>(clip_min.x), static_cast<int32_t>(clip_min.y),
                                    static_cast<uint32_t>(clip_max.x - clip_min.x),
                                    static_cast<uint32_t>(clip_max.y - clip_min.y));

                    const auto pc = push_constants{
                        .scale = math::float2(2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y),
                        .translate = math::float2(-1.0f - draw_data->DisplayPos.x * (2.0f / draw_data->DisplaySize.x),
                                                  -1.0f - draw_data->DisplayPos.y * (2.0f / draw_data->DisplaySize.y)),
                        .vertex_buffer = fb.vertex_buffer.gpu_address +
                                         static_cast<uint64_t>(pcmd.VtxOffset + global_vtx_offset) * sizeof(ImDrawVert),
                        .texture_id = static_cast<uint32_t>(pcmd.GetTexID()),
                        .sampler_id = _impl->linear_sampler_descriptor.index,
                    };

                    cmd.push_constants(rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                                       span<const byte>{reinterpret_cast<const byte*>(&pc), sizeof(pc)});

                    cmd.draw_indexed(pcmd.ElemCount, 1, pcmd.IdxOffset + global_idx_offset, 0, 0);
                }
            }
            global_idx_offset += static_cast<uint32_t>(cmd_list->IdxBuffer.Size);
            global_vtx_offset += static_cast<uint32_t>(cmd_list->VtxBuffer.Size);
        }
    }

    namespace ui
    {
        auto image(rhi::descriptor_handle descriptor, uint32_t width, uint32_t height) -> void
        {
            ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(descriptor.index)),
                         ImVec2(static_cast<float>(width), static_cast<float>(height)));
        }

        auto scalar(cstring_view label, float input) -> float
        {
            ImGui::InputFloat(label.c_str(), &input);
            return input;
        }

        auto float3(cstring_view label, math::float3 input) -> math::float3
        {
            float vals[] = {input.x, input.y, input.z};
            ImGui::InputFloat3(label.c_str(), vals);
            return math::float3(vals[0], vals[1], vals[2]);
        }

        auto color3(cstring_view label, math::float3 input) -> math::float3
        {
            float vals[] = {input.x, input.y, input.z};
            ImGui::ColorEdit3(label.c_str(), vals);
            return math::float3(vals[0], vals[1], vals[2]);
        }

        auto drag_integral(cstring_view label, int input, int minimum, int maximum) -> int
        {
            ImGui::DragInt(label.c_str(), &input, 1.0f, minimum, maximum);
            return input;
        }

        auto drag_scalar(cstring_view label, float input, float minimum, float maximum) -> float
        {
            ImGui::DragFloat(label.c_str(), &input, 1.0f, minimum, maximum);
            return input;
        }

        namespace
        {
            struct input_text_callback_user_data
            {
                string* str;
            };

            auto input_text_callback(ImGuiInputTextCallbackData* data) -> int
            {
                auto* user_data = static_cast<input_text_callback_user_data*>(data->UserData);
                if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                {
                    auto* str = user_data->str;
                    TEMPEST_ASSERT(data->Buf == str->c_str());
                    str->resize(static_cast<size_t>(data->BufTextLen));
                    data->Buf = str->data();
                }
                return 0;
            }
        } // namespace

        auto input_text(cstring_view label, string& input) -> bool
        {
            auto cb_user_data = input_text_callback_user_data{
                .str = &input,
            };
            const auto modified =
                ImGui::InputText(label.c_str(), input.data(), input.capacity() + 1, ImGuiInputTextFlags_CallbackResize,
                                 input_text_callback, &cb_user_data);
            if (modified)
            {
                input.resize(std::strlen(input.data()));
            }
            return modified;
        }

        auto input_text_with_hint(cstring_view label, cstring_view hint, string& input) -> bool
        {
            auto cb_user_data = input_text_callback_user_data{
                .str = &input,
            };
            const auto modified =
                ImGui::InputTextWithHint(label.c_str(), hint.c_str(), input.data(), input.capacity() + 1,
                                         ImGuiInputTextFlags_CallbackResize, input_text_callback, &cb_user_data);
            if (modified)
            {
                input.resize(std::strlen(input.data()));
            }
            return modified;
        }

        auto centered_button(cstring_view label) -> bool
        {
            const auto text_width = ImGui::CalcTextSize(label.c_str()).x;
            const auto button_width = text_width + ImGui::GetStyle().FramePadding.x * 2.0f;
            const auto available_width = ImGui::GetContentRegionAvail().x;

            if (button_width < available_width)
            {
                const auto offset_x = (available_width - button_width) / 2.0f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
            }

            return ImGui::Button(label.c_str());
        }
    } // namespace ui
} // namespace tempest::editor