#include <tempest/ui.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>

namespace tempest::editor::ui
{
    namespace
    {
        constexpr array imgui_vertex_shader_spv = {
            0x07230203, 0x00010000, 0x00080001, 0x0000002e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b, 0x00000001,
            0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x000a000f, 0x00000000,
            0x00000004, 0x6e69616d, 0x00000000, 0x0000000b, 0x0000000f, 0x00000015, 0x0000001b, 0x0000001c, 0x00030003,
            0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00030005, 0x00000009, 0x00000000,
            0x00050006, 0x00000009, 0x00000000, 0x6f6c6f43, 0x00000072, 0x00040006, 0x00000009, 0x00000001, 0x00005655,
            0x00030005, 0x0000000b, 0x0074754f, 0x00040005, 0x0000000f, 0x6c6f4361, 0x0000726f, 0x00030005, 0x00000015,
            0x00565561, 0x00060005, 0x00000019, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x00000019,
            0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69, 0x00030005, 0x0000001b, 0x00000000, 0x00040005, 0x0000001c,
            0x736f5061, 0x00000000, 0x00060005, 0x0000001e, 0x73755075, 0x6e6f4368, 0x6e617473, 0x00000074, 0x00050006,
            0x0000001e, 0x00000000, 0x61635375, 0x0000656c, 0x00060006, 0x0000001e, 0x00000001, 0x61725475, 0x616c736e,
            0x00006574, 0x00030005, 0x00000020, 0x00006370, 0x00040047, 0x0000000b, 0x0000001e, 0x00000000, 0x00040047,
            0x0000000f, 0x0000001e, 0x00000002, 0x00040047, 0x00000015, 0x0000001e, 0x00000001, 0x00050048, 0x00000019,
            0x00000000, 0x0000000b, 0x00000000, 0x00030047, 0x00000019, 0x00000002, 0x00040047, 0x0000001c, 0x0000001e,
            0x00000000, 0x00050048, 0x0000001e, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x0000001e, 0x00000001,
            0x00000023, 0x00000008, 0x00030047, 0x0000001e, 0x00000002, 0x00020013, 0x00000002, 0x00030021, 0x00000003,
            0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040017,
            0x00000008, 0x00000006, 0x00000002, 0x0004001e, 0x00000009, 0x00000007, 0x00000008, 0x00040020, 0x0000000a,
            0x00000003, 0x00000009, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000003, 0x00040015, 0x0000000c, 0x00000020,
            0x00000001, 0x0004002b, 0x0000000c, 0x0000000d, 0x00000000, 0x00040020, 0x0000000e, 0x00000001, 0x00000007,
            0x0004003b, 0x0000000e, 0x0000000f, 0x00000001, 0x00040020, 0x00000011, 0x00000003, 0x00000007, 0x0004002b,
            0x0000000c, 0x00000013, 0x00000001, 0x00040020, 0x00000014, 0x00000001, 0x00000008, 0x0004003b, 0x00000014,
            0x00000015, 0x00000001, 0x00040020, 0x00000017, 0x00000003, 0x00000008, 0x0003001e, 0x00000019, 0x00000007,
            0x00040020, 0x0000001a, 0x00000003, 0x00000019, 0x0004003b, 0x0000001a, 0x0000001b, 0x00000003, 0x0004003b,
            0x00000014, 0x0000001c, 0x00000001, 0x0004001e, 0x0000001e, 0x00000008, 0x00000008, 0x00040020, 0x0000001f,
            0x00000009, 0x0000001e, 0x0004003b, 0x0000001f, 0x00000020, 0x00000009, 0x00040020, 0x00000021, 0x00000009,
            0x00000008, 0x0004002b, 0x00000006, 0x00000028, 0x00000000, 0x0004002b, 0x00000006, 0x00000029, 0x3f800000,
            0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000007,
            0x00000010, 0x0000000f, 0x00050041, 0x00000011, 0x00000012, 0x0000000b, 0x0000000d, 0x0003003e, 0x00000012,
            0x00000010, 0x0004003d, 0x00000008, 0x00000016, 0x00000015, 0x00050041, 0x00000017, 0x00000018, 0x0000000b,
            0x00000013, 0x0003003e, 0x00000018, 0x00000016, 0x0004003d, 0x00000008, 0x0000001d, 0x0000001c, 0x00050041,
            0x00000021, 0x00000022, 0x00000020, 0x0000000d, 0x0004003d, 0x00000008, 0x00000023, 0x00000022, 0x00050085,
            0x00000008, 0x00000024, 0x0000001d, 0x00000023, 0x00050041, 0x00000021, 0x00000025, 0x00000020, 0x00000013,
            0x0004003d, 0x00000008, 0x00000026, 0x00000025, 0x00050081, 0x00000008, 0x00000027, 0x00000024, 0x00000026,
            0x00050051, 0x00000006, 0x0000002a, 0x00000027, 0x00000000, 0x00050051, 0x00000006, 0x0000002b, 0x00000027,
            0x00000001, 0x00070050, 0x00000007, 0x0000002c, 0x0000002a, 0x0000002b, 0x00000028, 0x00000029, 0x00050041,
            0x00000011, 0x0000002d, 0x0000001b, 0x0000000d, 0x0003003e, 0x0000002d, 0x0000002c, 0x000100fd, 0x00010038,
        };

        constexpr array imgui_fragment_shader_spv = {
            0x07230203, 0x00010000, 0x00080001, 0x0000001e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b, 0x00000001,
            0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x0007000f, 0x00000004,
            0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000d, 0x00030010, 0x00000004, 0x00000007, 0x00030003,
            0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00040005, 0x00000009, 0x6c6f4366,
            0x0000726f, 0x00030005, 0x0000000b, 0x00000000, 0x00050006, 0x0000000b, 0x00000000, 0x6f6c6f43, 0x00000072,
            0x00040006, 0x0000000b, 0x00000001, 0x00005655, 0x00030005, 0x0000000d, 0x00006e49, 0x00050005, 0x00000016,
            0x78655473, 0x65727574, 0x00000000, 0x00040047, 0x00000009, 0x0000001e, 0x00000000, 0x00040047, 0x0000000d,
            0x0000001e, 0x00000000, 0x00040047, 0x00000016, 0x00000022, 0x00000000, 0x00040047, 0x00000016, 0x00000021,
            0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020,
            0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008, 0x00000003, 0x00000007, 0x0004003b,
            0x00000008, 0x00000009, 0x00000003, 0x00040017, 0x0000000a, 0x00000006, 0x00000002, 0x0004001e, 0x0000000b,
            0x00000007, 0x0000000a, 0x00040020, 0x0000000c, 0x00000001, 0x0000000b, 0x0004003b, 0x0000000c, 0x0000000d,
            0x00000001, 0x00040015, 0x0000000e, 0x00000020, 0x00000001, 0x0004002b, 0x0000000e, 0x0000000f, 0x00000000,
            0x00040020, 0x00000010, 0x00000001, 0x00000007, 0x00090019, 0x00000013, 0x00000006, 0x00000001, 0x00000000,
            0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001b, 0x00000014, 0x00000013, 0x00040020, 0x00000015,
            0x00000000, 0x00000014, 0x0004003b, 0x00000015, 0x00000016, 0x00000000, 0x0004002b, 0x0000000e, 0x00000018,
            0x00000001, 0x00040020, 0x00000019, 0x00000001, 0x0000000a, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
            0x00000003, 0x000200f8, 0x00000005, 0x00050041, 0x00000010, 0x00000011, 0x0000000d, 0x0000000f, 0x0004003d,
            0x00000007, 0x00000012, 0x00000011, 0x0004003d, 0x00000014, 0x00000017, 0x00000016, 0x00050041, 0x00000019,
            0x0000001a, 0x0000000d, 0x00000018, 0x0004003d, 0x0000000a, 0x0000001b, 0x0000001a, 0x00050057, 0x00000007,
            0x0000001c, 0x00000017, 0x0000001b, 0x00050085, 0x00000007, 0x0000001d, 0x00000012, 0x0000001c, 0x0003003e,
            0x00000009, 0x0000001d, 0x000100fd, 0x00010038,
        };

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

        struct per_frame_buffer_data
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> vertex_buffer =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> index_buffer =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>::null_handle;

            uint64_t vertex_buffer_size = 0;
            uint64_t index_buffer_size = 0;
        };

        struct window_render_buffer_data
        {
            uint32_t index;
            uint32_t count;
            vector<per_frame_buffer_data> frame_render_buffers;
        };

        struct render_viewport_data
        {
            rhi::window_surface* surface = nullptr;
            window_render_buffer_data render_buffers = {};
            bool window_owned = false;
        };

        struct render_data
        {
            rhi::device* device = nullptr;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> viewport_pipeline =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline>::null_handle;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> texture_sampler =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle;

            size_t buffer_memory_alignment = 256;

            window_render_buffer_data main_window_render_buffers;

            rhi::image_format color_target_fmt;
        };
    } // namespace

    struct ui_context::impl
    {
        rhi::window_surface* surface = nullptr;
        rhi::window_surface* mouse_surface = nullptr;
        rhi::device* device = nullptr;

        render_data render_backend_data = {};

        ImGuiContext* imgui_context = nullptr;
        ImVec2 window_size;
        ImVec2 framebuffer_scale;

        ImVec2 last_mouse_pos = {0.0f, 0.0f};

        std::chrono::steady_clock::time_point time;

        bool mouse_ignore_button_up;
    };

    ui_context::ui_context(rhi::window_surface* surface, rhi::device* device, rhi::image_format target_fmt) noexcept
    {
        _impl = make_unique<ui_context::impl>();
        _impl->render_backend_data.color_target_fmt = target_fmt;

        IMGUI_CHECKVERSION();

        auto ctx = ImGui::CreateContext();
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        _impl->imgui_context = ctx;
        _impl->surface = surface;
        _impl->device = device;

        _init_window_backend();
        _init_render_backend();
    }

    ui_context::~ui_context()
    {
        _impl->device->destroy_graphics_pipeline(_impl->render_backend_data.pipeline);
        _impl->device->destroy_pipeline_layout(_impl->render_backend_data.pipeline_layout);
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

    void ui_context::_init_window_backend()
    {
        auto& ctx = _impl->imgui_context;
        auto surface = _impl->surface;

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

    void ui_context::_init_render_backend()
    {
        auto& ctx = _impl->imgui_context;
        ctx->IO.BackendRendererUserData = &_impl->render_backend_data;

        auto& render_data = _impl->render_backend_data;

        // Set up the pipeline layout
        auto desc_set_0_binding_0 = rhi::descriptor_binding_layout{
            .binding_index = 0,
            .type = rhi::descriptor_type::combined_image_sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
            .flags = make_enum_mask(rhi::descriptor_binding_flags::none),
        };
        auto set_0_bindings = vector<rhi::descriptor_binding_layout>();
        set_0_bindings.push_back(desc_set_0_binding_0);

        auto set_0_layout = _impl->device->create_descriptor_set_layout(set_0_bindings);

        auto pipeline_layout_desc = rhi::pipeline_layout_desc{};
        pipeline_layout_desc.descriptor_set_layouts.push_back(set_0_layout);
        pipeline_layout_desc.push_constants.push_back(rhi::push_constant_range{
            .offset = 0,
            .range = static_cast<uint32_t>(sizeof(float) * 4),
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });

        auto pipeline_layout = _impl->device->create_pipeline_layout(pipeline_layout_desc);
        render_data.pipeline_layout = pipeline_layout;

        // Set up the pipeline

        const auto vertex_shader_bytes = reinterpret_cast<const byte* const>(imgui_vertex_shader_spv.data());
        const auto vertex_shader_size =
            sizeof(decltype(imgui_vertex_shader_spv)::value_type) * imgui_vertex_shader_spv.size();

        const auto fragment_shader_bytes = reinterpret_cast<const byte* const>(imgui_fragment_shader_spv.data());
        const auto fragment_shader_size =
            sizeof(decltype(imgui_fragment_shader_spv)::value_type) * imgui_fragment_shader_spv.size();

        auto vertex_attributes = vector<rhi::vertex_attribute_desc>();
        auto vertex_bindings = vector<rhi::vertex_binding_desc>();

        vertex_bindings.push_back({
            .binding_index = 0,
            .stride = static_cast<uint32_t>(sizeof(ImDrawVert)),
            .input_rate = rhi::vertex_input_rate::vertex,
        });

        vertex_attributes.push_back({
            .binding_index = 0,
            .location_index = 0,
            .format = rhi::buffer_format::rg32_float,
            .offset = offsetof(ImDrawVert, pos),
        });

        vertex_attributes.push_back({
            .binding_index = 0,
            .location_index = 1,
            .format = rhi::buffer_format::rg32_float,
            .offset = offsetof(ImDrawVert, uv),
        });

        vertex_attributes.push_back({
            .binding_index = 0,
            .location_index = 2,
            .format = rhi::buffer_format::rgba8_unorm,
            .offset = offsetof(ImDrawVert, col),
        });

        auto vertex_input_desc = rhi::vertex_input_desc{
            .bindings = tempest::move(vertex_bindings),
            .attributes = tempest::move(vertex_attributes),
        };

        const auto color_attachment_blend = rhi::color_blend_attachment{
            .blend_enable = true,
            .src_color_blend_factor = rhi::blend_factor::src_alpha,
            .dst_color_blend_factor = rhi::blend_factor::one_minus_src_alpha,
            .color_blend_op = rhi::blend_op::add,
            .src_alpha_blend_factor = rhi::blend_factor::one,
            .dst_alpha_blend_factor = rhi::blend_factor::one_minus_constant_alpha,
        };

        auto color_blend_state = rhi::color_blend_state{};
        color_blend_state.attachments.push_back(color_attachment_blend);

        auto pipeline_desc = rhi::graphics_pipeline_desc{
            .color_attachment_formats = {},
            .depth_attachment_format = none(),
            .stencil_attachment_format = none(),
            .vertex_shader = {vertex_shader_bytes, vertex_shader_bytes + vertex_shader_size},
            .tessellation_control_shader = {},
            .tessellation_evaluation_shader = {},
            .geometry_shader = {},
            .fragment_shader = {fragment_shader_bytes, fragment_shader_bytes + fragment_shader_size},
            .input_assembly =
                {
                    .topology = rhi::primitive_topology::triangle_list,
                },
            .vertex_input = tempest::move(vertex_input_desc),
            .tessellation = none(),
            .multisample =
                {
                    .sample_count = rhi::image_sample_count::sample_count_1,
                    .sample_shading = false,
                    .alpha_to_coverage = false,
                    .alpha_to_one = false,
                },
            .rasterization =
                {
                    .depth_clamp_enable = false,
                    .rasterizer_discard_enable = false,
                    .polygon_mode = rhi::polygon_mode::fill,
                    .cull_mode = make_enum_mask(rhi::cull_mode::back),
                    .vertex_winding = rhi::vertex_winding::counter_clockwise,
                    .depth_bias = none(),
                    .line_width = 1.0f,
                },
            .depth_stencil =
                {
                    .depth = none(),
                    .stencil = none(),
                },
            .color_blend = color_blend_state,
            .layout = pipeline_layout,
            .name = "ImGUI Pipeline",
        };

        render_data.pipeline = _impl->device->create_graphics_pipeline(pipeline_desc);
    }

    ui_pipeline::ui_pipeline(ui_context* ui_ctx) : _ui_ctx{ui_ctx}
    {
    }

    void ui_pipeline::initialize(graphics::renderer& parent, rhi::device& dev)
    {
    }

    graphics::render_pipeline::render_result ui_pipeline::render(graphics::renderer& parent, rhi::device& dev,
                                                                 const graphics::render_pipeline::render_state& rs)
    {
        auto& queue = dev.get_primary_work_queue();
        auto command_list = queue.get_next_command_list();

        queue.begin_command_list(command_list, true);

        // Transition the swapchain image to a render target
        const array pre_ui_barriers = {
            rhi::work_queue::image_barrier{
                .image = rs.swapchain_image,
                .old_layout = rhi::image_layout::undefined,
                .new_layout = rhi::image_layout::color_attachment,
                .src_stages = make_enum_mask(rhi::pipeline_stage::blit),
                .src_access = make_enum_mask(rhi::memory_access::transfer_read),
                .dst_stages = make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                .dst_access = make_enum_mask(rhi::memory_access::color_attachment_write),
            },
        };

        queue.transition_image(command_list, pre_ui_barriers);

        rhi::work_queue::render_pass_info ui_rpi;
        ui_rpi.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = rs.swapchain_image,
            .layout = rhi::image_layout::color_attachment,
            .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        });

        ui_rpi.x = 0;
        ui_rpi.y = 0;
        ui_rpi.width = rs.image_width;
        ui_rpi.height = rs.image_height;
        ui_rpi.layers = 1;
        ui_rpi.name = "UI Render Pass";

        queue.begin_rendering(command_list, ui_rpi);
        _ui_ctx->render_ui_commands(command_list, queue);
        queue.end_rendering(command_list);

        // Transition the swapchain image to present layout
        const array post_ui_barriers = {
            rhi::work_queue::image_barrier{
                .image = rs.swapchain_image,
                .old_layout = rhi::image_layout::color_attachment,
                .new_layout = rhi::image_layout::present,
                .src_stages = make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                .src_access = make_enum_mask(rhi::memory_access::color_attachment_write),
                .dst_stages = make_enum_mask(rhi::pipeline_stage::bottom),
                .dst_access = make_enum_mask(rhi::memory_access::none),
            },
        };

        queue.transition_image(command_list, post_ui_barriers);

        queue.end_command_list(command_list);

        rhi::work_queue::submit_info submit_info;
        submit_info.command_lists.push_back(command_list);
        submit_info.wait_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = rs.start_sem,
            .value = 0,
            .stages = make_enum_mask(rhi::pipeline_stage::all_transfer),
        });
        submit_info.signal_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = rs.end_sem,
            .value = 1,
            .stages = make_enum_mask(rhi::pipeline_stage::bottom),
        });

        queue.submit(tempest::span(&submit_info, 1), rs.end_fence);

        rhi::work_queue::present_info present_info;
        present_info.swapchain_images.push_back(rhi::work_queue::swapchain_image_present_info{
            .render_surface = rs.surface,
            .image_index = rs.image_index,
        });
        present_info.wait_semaphores.push_back(rs.end_sem);
        auto present_result = queue.present(present_info);

        ++_frame_number;
        _frame_in_flight = _frame_number % dev.frames_in_flight();

        if (present_result == rhi::work_queue::present_result::out_of_date ||
            present_result == rhi::work_queue::present_result::suboptimal)
        {
            return render_result::REQUEST_RECREATE_SWAPCHAIN;
        }
        else if (present_result == rhi::work_queue::present_result::error)
        {
            return render_result::FAILURE;
        }
        return render_result::SUCCESS;
    }

    void ui_pipeline::destroy([[maybe_unused]] graphics::renderer& parent, [[maybe_unused]] rhi::device& dev)
    {
    }
} // namespace tempest::editor::ui