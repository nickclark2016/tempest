#include <tempest/ui.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>

namespace tempest::editor::ui
{
    namespace
    {
        ImGuiKey convert_key(const core::key_state& key_state) noexcept
        {
            switch (key_state.k)
            {
            case core::key::TAB:
                return ImGuiKey_Tab;
            case core::key::DPAD_LEFT:
                return ImGuiKey_LeftArrow;
            case core::key::DPAD_RIGHT:
                return ImGuiKey_RightArrow;
            case core::key::DPAD_UP:
                return ImGuiKey_UpArrow;
            case core::key::DPAD_DOWN:
                return ImGuiKey_DownArrow;
            case core::key::PAGE_UP:
                return ImGuiKey_PageUp;
            case core::key::PAGE_DOWN:
                return ImGuiKey_PageDown;
            case core::key::HOME:
                return ImGuiKey_Home;
            case core::key::END:
                return ImGuiKey_End;
            case core::key::INSERT:
                return ImGuiKey_Insert;
            case core::key::DELETE:
                return ImGuiKey_Delete;
            case core::key::BACKSPACE:
                return ImGuiKey_Backspace;
            case core::key::SPACE:
                return ImGuiKey_Space;
            case core::key::ENTER:
                return ImGuiKey_Enter;
            case core::key::ESCAPE:
                return ImGuiKey_Escape;
            case core::key::APOSTROPHE:
                return ImGuiKey_Apostrophe;
            case core::key::COMMA:
                return ImGuiKey_Comma;
            case core::key::MINUS:
                return ImGuiKey_Minus;
            case core::key::PERIOD:
                return ImGuiKey_Period;
            case core::key::SLASH:
                return ImGuiKey_Slash;
            case core::key::SEMICOLON:
                return ImGuiKey_Semicolon;
            case core::key::EQUAL:
                return ImGuiKey_Equal;
            case core::key::LEFT_BRACKET:
                return ImGuiKey_LeftBracket;
            case core::key::BACKSLASH:
                return ImGuiKey_Backslash;
            case core::key::WORLD_1:
                return ImGuiKey_Oem102;
            case core::key::WORLD_2:
                return ImGuiKey_Oem102;
            case core::key::RIGHT_BRACKET:
                return ImGuiKey_RightBracket;
            case core::key::GRAVE_ACCENT:
                return ImGuiKey_GraveAccent;
            case core::key::CAPS_LOCK:
                return ImGuiKey_CapsLock;
            case core::key::SCROLL_LOCK:
                return ImGuiKey_ScrollLock;
            case core::key::NUM_LOCK:
                return ImGuiKey_NumLock;
            case core::key::PRINT_SCREEN:
                return ImGuiKey_PrintScreen;
            case core::key::PAUSE:
                return ImGuiKey_Pause;
            case core::key::KP_0:
                return ImGuiKey_Keypad0;
            case core::key::KP_1:
                return ImGuiKey_Keypad1;
            case core::key::KP_2:
                return ImGuiKey_Keypad2;
            case core::key::KP_3:
                return ImGuiKey_Keypad3;
            case core::key::KP_4:
                return ImGuiKey_Keypad4;
            case core::key::KP_5:
                return ImGuiKey_Keypad5;
            case core::key::KP_6:
                return ImGuiKey_Keypad6;
            case core::key::KP_7:
                return ImGuiKey_Keypad7;
            case core::key::KP_8:
                return ImGuiKey_Keypad8;
            case core::key::KP_9:
                return ImGuiKey_Keypad9;
            case core::key::KP_DECIMAL:
                return ImGuiKey_KeypadDecimal;
            case core::key::KP_DIVIDE:
                return ImGuiKey_KeypadDivide;
            case core::key::KP_MULTIPLY:
                return ImGuiKey_KeypadMultiply;
            case core::key::KP_SUBTRACT:
                return ImGuiKey_KeypadSubtract;
            case core::key::KP_ADD:
                return ImGuiKey_KeypadAdd;
            case core::key::KP_ENTER:
                return ImGuiKey_KeypadEnter;
            case core::key::KP_EQUAL:
                return ImGuiKey_KeypadEqual;
            case core::key::LEFT_SHIFT:
                return ImGuiKey_LeftShift;
            case core::key::LEFT_CONTROL:
                return ImGuiKey_LeftCtrl;
            case core::key::LEFT_ALT:
                return ImGuiKey_LeftAlt;
            case core::key::LEFT_SUPER:
                return ImGuiKey_LeftSuper;
            case core::key::RIGHT_SHIFT:
                return ImGuiKey_RightShift;
            case core::key::RIGHT_CONTROL:
                return ImGuiKey_RightCtrl;
            case core::key::RIGHT_ALT:
                return ImGuiKey_RightAlt;
            case core::key::RIGHT_SUPER:
                return ImGuiKey_RightSuper;
            case core::key::MENU:
                return ImGuiKey_Menu;
            case core::key::A:
                return ImGuiKey_A;
            case core::key::B:
                return ImGuiKey_B;
            case core::key::C:
                return ImGuiKey_C;
            case core::key::D:
                return ImGuiKey_D;
            case core::key::E:
                return ImGuiKey_E;
            case core::key::F:
                return ImGuiKey_F;
            case core::key::G:
                return ImGuiKey_G;
            case core::key::H:
                return ImGuiKey_H;
            case core::key::I:
                return ImGuiKey_I;
            case core::key::J:
                return ImGuiKey_J;
            case core::key::K:
                return ImGuiKey_K;
            case core::key::L:
                return ImGuiKey_L;
            case core::key::M:
                return ImGuiKey_M;
            case core::key::N:
                return ImGuiKey_N;
            case core::key::O:
                return ImGuiKey_O;
            case core::key::P:
                return ImGuiKey_P;
            case core::key::Q:
                return ImGuiKey_Q;
            case core::key::R:
                return ImGuiKey_R;
            case core::key::S:
                return ImGuiKey_S;
            case core::key::T:
                return ImGuiKey_T;
            case core::key::U:
                return ImGuiKey_U;
            case core::key::V:
                return ImGuiKey_V;
            case core::key::W:
                return ImGuiKey_W;
            case core::key::X:
                return ImGuiKey_X;
            case core::key::Y:
                return ImGuiKey_Y;
            case core::key::Z:
                return ImGuiKey_Z;
            case core::key::FUNCTION_1:
                return ImGuiKey_F1;
            case core::key::FUNCTION_2:
                return ImGuiKey_F2;
            case core::key::FUNCTION_3:
                return ImGuiKey_F3;
            case core::key::FUNCTION_4:
                return ImGuiKey_F4;
            case core::key::FUNCTION_5:
                return ImGuiKey_F5;
            case core::key::FUNCTION_6:
                return ImGuiKey_F6;
            case core::key::FUNCTION_7:
                return ImGuiKey_F7;
            case core::key::FUNCTION_8:
                return ImGuiKey_F8;
            case core::key::FUNCTION_9:
                return ImGuiKey_F9;
            case core::key::FUNCTION_10:
                return ImGuiKey_F10;
            case core::key::FUNCTION_11:
                return ImGuiKey_F11;
            case core::key::FUNCTION_12:
                return ImGuiKey_F12;
            case core::key::FUNCTION_13:
                return ImGuiKey_F13;
            case core::key::FUNCTION_14:
                return ImGuiKey_F14;
            case core::key::FUNCTION_15:
                return ImGuiKey_F15;
            case core::key::FUNCTION_16:
                return ImGuiKey_F16;
            case core::key::FUNCTION_17:
                return ImGuiKey_F17;
            case core::key::FUNCTION_18:
                return ImGuiKey_F18;
            case core::key::FUNCTION_19:
                return ImGuiKey_F19;
            case core::key::FUNCTION_20:
                return ImGuiKey_F20;
            case core::key::FUNCTION_21:
                return ImGuiKey_F21;
            case core::key::FUNCTION_22:
                return ImGuiKey_F22;
            case core::key::FUNCTION_23:
                return ImGuiKey_F23;
            case core::key::FUNCTION_24:
                return ImGuiKey_F24;
            default:
                return ImGuiKey_None;
            }
        }
    } // namespace

    struct ui_context::impl
    {
        rhi::window_surface* surface = nullptr;
        rhi::window_surface* mouse_surface = nullptr;
        rhi::device* device = nullptr;

        ImGuiContext* imgui_context = nullptr;
        ImVec2 window_size;
        ImVec2 framebuffer_scale;

        ImVec2 last_mouse_pos = {0.0f, 0.0f};

        std::chrono::steady_clock::time_point time;

        bool mouse_ignore_button_up;
    };

    ui_context::ui_context(rhi::window_surface* surface, rhi::device* device) noexcept
    {
        _impl = make_unique<ui_context::impl>();

        IMGUI_CHECKVERSION();

        auto ctx = ImGui::CreateContext();
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        _impl->imgui_context = ctx;
        _impl->surface = surface;
        _impl->device = device;

        ctx->IO.BackendPlatformUserData = static_cast<void*>(_impl.get());
        ctx->IO.BackendPlatformName = "tempest_editor_ui";
        ctx->IO.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        ctx->IO.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

        auto main_viewport = ctx->Viewports[0];
        main_viewport->PlatformHandle = static_cast<void*>(surface);

        surface->register_focus_callback([ctx](bool focused) { ctx->IO.AddFocusEvent(focused); });

        surface->register_keyboard_callback([ctx](const core::key_state& key_state) {
            if (key_state.action != core::key_action::PRESS && key_state.action != core::key_action::RELEASE)
            {
                return;
            }

            auto& io = ctx->IO;

            ImGuiKey key = convert_key(key_state);
            io.AddKeyEvent(key, key_state.action == core::key_action::PRESS);
        });

        surface->register_cursor_callback([ctx](float x, float y) {
            auto& io = ctx->IO;
            io.AddMousePosEvent(x, y);
            static_cast<ui_context::impl*>(io.BackendPlatformUserData)->last_mouse_pos = ImVec2(x, y);
        });

        surface->register_cursor_enter_callback([ctx, surface](bool entered) {
            auto& io = ctx->IO;
            auto bd = static_cast<ui_context::impl*>(io.BackendPlatformUserData);
            if (entered)
            {
                io.AddMousePosEvent(bd->last_mouse_pos.x, bd->last_mouse_pos.y);
                bd->mouse_surface = surface;
            }
            else
            {
                bd->last_mouse_pos = io.MousePos;
                bd->mouse_surface = nullptr;
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            }
        });

        surface->register_character_input_callback([ctx](uint32_t codepoint) {
            auto& io = ctx->IO;
            io.AddInputCharacter(codepoint);
        });

        surface->register_mouse_callback([ctx](const core::mouse_button_state& mouse_state) {
            if (mouse_state.action != core::mouse_action::RELEASE)
            {
                return;
            }

            auto& io = ctx->IO;

            ImGuiMouseButton button = -1;
            switch (mouse_state.button)
            {
            case core::mouse_button::LEFT:
                button = ImGuiMouseButton_Left;
                break;
            case core::mouse_button::RIGHT:
                button = ImGuiMouseButton_Right;
                break;
            case core::mouse_button::MIDDLE:
                button = ImGuiMouseButton_Middle;
                break;
            default:
                return; // Unsupported mouse button
            }

            if (button > 0 && button < ImGuiMouseButton_COUNT)
            {
                io.AddMouseButtonEvent(button, mouse_state.action == core::mouse_action::PRESS);
            }
        });

        surface->register_scroll_callback([ctx](float x_offset, float y_offset) {
            auto& io = ctx->IO;
            io.AddMouseWheelEvent(x_offset, y_offset);
        });

        auto& platform_io = ctx->PlatformIO;
        auto monitors = _impl->surface->get_monitors();

        platform_io.Monitors.clear();

        for (const auto& monitor : monitors)
        {
            ImGuiPlatformMonitor platform_monitor;
            platform_monitor.MainPos = ImVec2(static_cast<float>(monitor.x), static_cast<float>(monitor.y));
            platform_monitor.MainSize = ImVec2(static_cast<float>(monitor.current_video_mode.width),
                                               static_cast<float>(monitor.current_video_mode.height));
            platform_monitor.WorkPos = ImVec2(static_cast<float>(monitor.work_x), static_cast<float>(monitor.work_y));
            platform_monitor.WorkSize =
                ImVec2(static_cast<float>(monitor.work_width), static_cast<float>(monitor.work_height));
            platform_monitor.DpiScale = monitor.content_scale_x;
            platform_io.Monitors.push_back(platform_monitor);
        }

        _impl->time = std::chrono::steady_clock::now();
    }

    ui_context::~ui_context()
    {
        ImGui::DestroyContext(static_cast<ImGuiContext*>(_impl->imgui_context));
    }

    void ui_context::begin_ui_commands()
    {
        auto context = static_cast<ImGuiContext*>(_impl->imgui_context);
        ImGui::SetCurrentContext(context);
        auto& io = context->IO;

        // Renderer-specific new frame setup

        // Windowing specific new frame setup
        {
            // Handle window size and framebuffer scale
            uint32_t width = _impl->surface->height();
            uint32_t height = _impl->surface->width();
            uint32_t fb_width = _impl->surface->framebuffer_width();
            uint32_t fb_height = _impl->surface->framebuffer_height();

            _impl->window_size = ImVec2(static_cast<float>(width), static_cast<float>(height));
            _impl->framebuffer_scale = (width > 0 && height > 0)
                                           ? ImVec2(static_cast<float>(fb_width) / static_cast<float>(width),
                                                    static_cast<float>(fb_height) / static_cast<float>(height))
                                           : ImVec2(1.0f, 1.0f);

            // Handle monitors
            auto& platform_io = context->PlatformIO;
            auto monitors = _impl->surface->get_monitors();

            platform_io.Monitors.clear();

            for (const auto& monitor : monitors)
            {
                ImGuiPlatformMonitor platform_monitor;
                platform_monitor.MainPos = ImVec2(static_cast<float>(monitor.x), static_cast<float>(monitor.y));
                platform_monitor.MainSize = ImVec2(static_cast<float>(monitor.current_video_mode.width),
                                                   static_cast<float>(monitor.current_video_mode.height));
                platform_monitor.WorkPos =
                    ImVec2(static_cast<float>(monitor.work_x), static_cast<float>(monitor.work_y));
                platform_monitor.WorkSize =
                    ImVec2(static_cast<float>(monitor.work_width), static_cast<float>(monitor.work_height));
                platform_monitor.DpiScale = monitor.content_scale_x;
                platform_io.Monitors.push_back(platform_monitor);
            }

            auto current_time = std::chrono::steady_clock::now();
            if (current_time <= _impl->time)
            {
                current_time = _impl->time + std::chrono::microseconds(100);
            }
            io.DeltaTime = std::chrono::duration<float>(current_time - _impl->time).count();
            _impl->time = current_time;

            // Handle mouse data
            _impl->mouse_ignore_button_up = false;

            [&]() {
                if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || _impl->surface->is_cursor_disabled())
                {
                    return;
                }

                ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
                for (int n = 0; n < platform_io.Viewports.Size; ++n)
                {
                    auto surface = static_cast<rhi::window_surface*>(platform_io.Viewports[n]->PlatformHandle);
                    if (cursor == ImGuiMouseCursor_None)
                    {
                        surface->hide_cursor();
                    }
                    else
                    {
                        auto cursor_shape = [cursor]() {
                            switch (cursor)
                            {
                            case ImGuiMouseCursor_Arrow:
                                return rhi::window_surface::cursor_shape::arrow;
                            case ImGuiMouseCursor_TextInput:
                                return rhi::window_surface::cursor_shape::ibeam;
                            case ImGuiMouseCursor_ResizeAll:
                                return rhi::window_surface::cursor_shape::crosshair;
                            case ImGuiMouseCursor_ResizeNS:
                                return rhi::window_surface::cursor_shape::resize_vertical;
                            case ImGuiMouseCursor_ResizeEW:
                                return rhi::window_surface::cursor_shape::resize_horizontal;
                            case ImGuiMouseCursor_ResizeNESW:
                                return rhi::window_surface::cursor_shape::resize_horizontal;
                            case ImGuiMouseCursor_ResizeNWSE:
                                return rhi::window_surface::cursor_shape::resize_vertical;
                            case ImGuiMouseCursor_Hand:
                                return rhi::window_surface::cursor_shape::hand;
                            default:
                                return rhi::window_surface::cursor_shape::arrow;
                            }
                        }();

                        surface->set_cursor_shape(cursor_shape);
                        surface->show_cursor();
                    }
                }
            }();
        }

        // Start new imgui frame
        ImGui::NewFrame();
    }

    void ui_context::finish_ui_commands()
    {
        ImGui::Render();
    }

    void ui_context::render_ui_commands(rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> command_list,
                                        rhi::work_queue& wq) noexcept
    {
    }
} // namespace tempest::editor::ui