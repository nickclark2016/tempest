#include <tempest/ui.hpp>

#include <tempest/tuple.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <cstring>

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

        struct render_state
        {
            rhi::work_queue* queue;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> pipeline_layout;
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
            uint32_t frames_in_flight;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> font_texture =
                rhi::typed_rhi_handle<rhi::rhi_handle_type::image>::null_handle;
        };

        ImGuiWindowFlags to_imgui(enum_mask<ui_context::window_flags> flags)
        {
            ImGuiWindowFlags im_flags = 0;

            if ((flags & ui_context::window_flags::no_title) == ui_context::window_flags::no_title)
            {
                im_flags |= ImGuiWindowFlags_NoTitleBar;
            }

            if ((flags & ui_context::window_flags::no_resize) == ui_context::window_flags::no_resize)
            {
                im_flags |= ImGuiWindowFlags_NoResize;
            }

            if ((flags & ui_context::window_flags::no_move) == ui_context::window_flags::no_move)
            {
                im_flags |= ImGuiWindowFlags_NoMove;
            }

            if ((flags & ui_context::window_flags::no_scrollbar) == ui_context::window_flags::no_scrollbar)
            {
                im_flags |= ImGuiWindowFlags_NoScrollbar;
            }

            if ((flags & ui_context::window_flags::no_bring_to_front_on_focus) ==
                ui_context::window_flags::no_bring_to_front_on_focus)
            {
                im_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
            }

            if ((flags & ui_context::window_flags::no_collapse) == ui_context::window_flags::no_collapse)
            {
                im_flags |= ImGuiWindowFlags_NoCollapse;
            }

            if ((flags & ui_context::window_flags::no_navigation_focus) ==
                ui_context::window_flags::no_navigation_focus)
            {
                im_flags |= ImGuiWindowFlags_NoNavFocus;
            }

            if ((flags & ui_context::window_flags::no_decoration) == ui_context::window_flags::no_decoration)
            {
                im_flags |= ImGuiWindowFlags_NoDecoration;
            }

            if ((flags & ui_context::window_flags::no_background) == ui_context::window_flags::no_background)
            {
                im_flags |= ImGuiWindowFlags_NoBackground;
            }

            if ((flags & ui_context::window_flags::no_docking) == ui_context::window_flags::no_docking)
            {
                im_flags |= ImGuiWindowFlags_NoDocking;
            }

            if ((flags & ui_context::window_flags::menubar) == ui_context::window_flags::menubar)
            {
                im_flags |= ImGuiWindowFlags_MenuBar;
            }

            return im_flags;
        }
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

    ui_context::ui_context(rhi::window_surface* surface, rhi::device* device, rhi::image_format target_fmt,
                           uint32_t frames_in_flight) noexcept
    {
        _impl = make_unique<ui_context::impl>();
        _impl->render_backend_data.color_target_fmt = target_fmt;
        _impl->render_backend_data.frames_in_flight = frames_in_flight;

        IMGUI_CHECKVERSION();

        auto ctx = ImGui::CreateContext();
        ctx->IO = ImGui::GetIO();
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ctx->IO.Fonts->AddFontDefault();

        _impl->imgui_context = ctx;
        _impl->surface = surface;
        _impl->device = device;

        _init_window_backend();
        _init_render_backend();
    }

    ui_context::~ui_context()
    {
        auto viewport_render_data =
            static_cast<render_viewport_data*>(_impl->imgui_context->Viewports[0]->RendererUserData);
        delete viewport_render_data;

        _impl->device->destroy_sampler(_impl->render_backend_data.texture_sampler);
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
        {
            if (!_impl->render_backend_data.font_texture)
            {
                _setup_font_textures();
            }
        }

        // Windowing specific new frame setup
        {
            // Handle window size and framebuffer scale
            uint32_t width = _impl->surface->width();
            uint32_t height = _impl->surface->height();
            uint32_t fb_width = _impl->surface->framebuffer_width();
            uint32_t fb_height = _impl->surface->framebuffer_height();

            _impl->window_size = ImVec2(static_cast<float>(width), static_cast<float>(height));
            _impl->framebuffer_scale = (width > 0 && height > 0)
                                           ? ImVec2(static_cast<float>(fb_width) / static_cast<float>(width),
                                                    static_cast<float>(fb_height) / static_cast<float>(height))
                                           : ImVec2(1.0f, 1.0f);

            io.DisplaySize = _impl->window_size;
            io.DisplayFramebufferScale = _impl->framebuffer_scale;

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
        auto& draw_data = _impl->imgui_context->Viewports[0]->DrawDataP;
        if (!draw_data.Valid)
        {
            return;
        }

        auto fb_width = static_cast<int>(draw_data.DisplaySize.x * draw_data.FramebufferScale.x);
        auto fb_height = static_cast<int>(draw_data.DisplaySize.y * draw_data.FramebufferScale.y);

        // Early return if the framebuffer is 0 sized
        if (fb_width <= 0 || fb_height <= 0)
        {
            return;
        }

        auto owner_vp = draw_data.OwnerViewport;
        auto viewport_render_data = static_cast<render_viewport_data*>(owner_vp->RendererUserData);
        assert(viewport_render_data != nullptr);

        auto wrb = &viewport_render_data->render_buffers;
        if (wrb->frame_render_buffers.empty())
        {
            wrb->index = 0;
            wrb->count = _impl->render_backend_data.frames_in_flight;
            wrb->frame_render_buffers.resize(wrb->count);
        }

        assert(wrb->frame_render_buffers.size() == wrb->count);
        assert(wrb->count == _impl->render_backend_data.frames_in_flight);

        auto& rb = wrb->frame_render_buffers[wrb->index];

        if (draw_data.TotalVtxCount > 0)
        {
            // Set up the vertex and index buffers
            // Map the buffers
            // Copy the vertex data
            // Unmap and flush the buffers

            auto resize_buffers = [device = _impl->device,
                                   alignment = _impl->render_backend_data.buffer_memory_alignment](
                                      rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buf, size_t requested_size,
                                      enum_mask<rhi::buffer_usage> usage, string name) {
                if (buf)
                {
                    device->destroy_buffer(buf);
                }

                const auto aligned_size = math::round_to_next_multiple(requested_size, alignment);

                auto buffer_desc = rhi::buffer_desc{
                    .size = aligned_size,
                    .location = rhi::memory_location::automatic,
                    .usage = usage,
                    .access_type = rhi::host_access_type::incoherent,
                    .access_pattern = rhi::host_access_pattern::sequential,
                    .name = tempest::move(name),
                };

                return make_tuple(device->create_buffer(buffer_desc), aligned_size);
            };

            const auto vertex_size = math::round_to_next_multiple(draw_data.TotalVtxCount * sizeof(ImDrawVert),
                                                                  _impl->render_backend_data.buffer_memory_alignment);
            const auto index_size = math::round_to_next_multiple(draw_data.TotalIdxCount * sizeof(ImDrawIdx),
                                                                 _impl->render_backend_data.buffer_memory_alignment);

            if (!rb.vertex_buffer || rb.vertex_buffer_size < vertex_size)
            {
                auto [buf, size] = resize_buffers(rb.vertex_buffer, vertex_size,
                                                  make_enum_mask(rhi::buffer_usage::vertex), "ImGUI Vertex Buffer");
                rb.vertex_buffer = buf;
                rb.vertex_buffer_size = size;
            }

            if (!rb.index_buffer || rb.index_buffer_size < index_size)
            {
                auto [buf, size] = resize_buffers(rb.index_buffer, index_size, make_enum_mask(rhi::buffer_usage::index),
                                                  "ImGUI Index Buffer");
                rb.index_buffer = buf;
                rb.index_buffer_size = size;
            }

            auto vertex_buffer_data = _impl->device->map_buffer(rb.vertex_buffer);
            auto index_buffer_data = _impl->device->map_buffer(rb.index_buffer);

            auto vtx_dst = reinterpret_cast<ImDrawVert*>(vertex_buffer_data);
            auto idx_dst = reinterpret_cast<ImDrawIdx*>(index_buffer_data);

            for (int n = 0; n < draw_data.CmdListsCount; ++n)
            {
                const auto draw_list = draw_data.CmdLists[n];
                std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
                std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
                vtx_dst += draw_list->VtxBuffer.Size;
                idx_dst += draw_list->IdxBuffer.Size;
            }

            const array buffers = {
                rb.vertex_buffer,
                rb.index_buffer,
            };

            _impl->device->flush_buffers(buffers);

            _impl->device->unmap_buffer(rb.vertex_buffer);
            _impl->device->unmap_buffer(rb.index_buffer);
        }

        // Setup the render state for tempest::rhi
        auto setup_render_state = [&bd = _impl->render_backend_data](
                                      ImDrawData& draw_data,
                                      rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline,
                                      per_frame_buffer_data* rb, int fb_width, int fb_height, rhi::work_queue& queue,
                                      rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> commands) {
            queue.bind(commands, pipeline);
            queue.set_cull_mode(commands, make_enum_mask(rhi::cull_mode::none));

            if (draw_data.TotalVtxCount > 0)
            {
                const array vertex_buffers = {
                    rb->vertex_buffer,
                };

                const array vertex_buffer_offsets = {
                    size_t(0),
                };

                queue.bind_vertex_buffers(commands, 0, vertex_buffers, vertex_buffer_offsets);
                queue.bind_index_buffer(commands, rb->index_buffer, 0,
                                        sizeof(ImDrawIdx) == 2 ? rhi::index_format::uint16 : rhi::index_format::uint32);
            }

            queue.set_viewport(commands, 0, 0, static_cast<float>(fb_width), static_cast<float>(fb_height), 0, 1, 0,
                               false);

            const array scale = {
                2.0f / draw_data.DisplaySize.x,
                2.0f / draw_data.DisplaySize.y,
            };

            const array translate = {
                -1.0f - draw_data.DisplayPos.x * scale[0],
                -1.0f - draw_data.DisplayPos.y * scale[1],
            };

            queue.typed_push_constants(commands, bd.pipeline_layout, make_enum_mask(rhi::shader_stage::vertex), 0,
                                       scale);
            queue.typed_push_constants(commands, bd.pipeline_layout, make_enum_mask(rhi::shader_stage::vertex), 8,
                                       translate);
        };

        setup_render_state(draw_data, _impl->render_backend_data.pipeline, &rb, fb_width, fb_height, wq, command_list);

        // Set up the render state for imgui
        ImGuiPlatformIO& platform_io = _impl->imgui_context->PlatformIO;
        render_state state = {
            .queue = &wq,
            .commands = command_list,
            .pipeline = _impl->render_backend_data.pipeline,
            .pipeline_layout = _impl->render_backend_data.pipeline_layout,
        };
        platform_io.Renderer_RenderState = &state;

        auto clip_offset = draw_data.DisplayPos;
        auto clip_scale = draw_data.FramebufferScale;

        auto global_vtx_offset = 0u;
        auto global_idx_offset = 0u;

        for (int n = 0; n < draw_data.CmdListsCount; ++n)
        {
            const auto draw_list = draw_data.CmdLists[n];
            for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; ++cmd_i)
            {
                const auto& cmd = draw_list->CmdBuffer[cmd_i];
                if (cmd.UserCallback)
                {
                    if (cmd.UserCallback != ImDrawCallback_ResetRenderState)
                    {
                        setup_render_state(draw_data, _impl->render_backend_data.viewport_pipeline, &rb, fb_width,
                                           fb_height, wq, command_list);
                    }
                    else
                    {
                        cmd.UserCallback(draw_list, &cmd);
                    }
                }
                else
                {
                    auto clip_min = ImVec2((cmd.ClipRect.x - clip_offset.x) * clip_scale.x,
                                           (cmd.ClipRect.y - clip_offset.y) * clip_scale.y);
                    auto clip_max = ImVec2((cmd.ClipRect.z - clip_offset.x) * clip_scale.x,
                                           (cmd.ClipRect.w - clip_offset.y) * clip_scale.y);

                    if (clip_min.x < 0.0f)
                    {
                        clip_min.x = 0.0f;
                    }

                    if (clip_min.y < 0.0f)
                    {
                        clip_min.y = 0.0f;
                    }

                    if (clip_max.x > static_cast<float>(fb_width))
                    {
                        clip_max.x = static_cast<float>(fb_width);
                    }

                    if (clip_max.y > static_cast<float>(fb_height))
                    {
                        clip_max.y = static_cast<float>(fb_height);
                    }

                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    {
                        continue; // Clipped out
                    }

                    // Apply the scissor test
                    wq.set_scissor_region(
                        command_list, static_cast<int32_t>(clip_min.x), static_cast<int32_t>(clip_min.y),
                        static_cast<uint32_t>(clip_max.x - clip_min.x), static_cast<uint32_t>(clip_max.y - clip_min.y));

                    // Push the descriptor set for the texture
                    auto packed_texture_id = cmd.GetTexID();
                    uint32_t generation = 0, id = 0;
                    math::unpack_uint32x2(packed_texture_id, generation, id);
                    auto texture_handle = rhi::typed_rhi_handle<rhi::rhi_handle_type::image>{
                        .id = id,
                        .generation = generation,
                    };

                    rhi::image_binding_descriptor image_desc = {
                        .index = 0,
                        .type = rhi::descriptor_type::combined_image_sampler,
                        .array_offset = 0,
                        .images = {},
                    };
                    image_desc.images.push_back({
                        .image = texture_handle,
                        .sampler = _impl->render_backend_data.texture_sampler,
                        .layout = rhi::image_layout::shader_read_only,
                    });

                    wq.push_descriptors(command_list, _impl->render_backend_data.pipeline_layout,
                                        rhi::bind_point::graphics, 0, {}, {&image_desc, 1}, {});

                    wq.draw(command_list, cmd.ElemCount, 1u, cmd.IdxOffset + global_idx_offset,
                            static_cast<int32_t>(cmd.VtxOffset + global_vtx_offset), 0);
                }
            }

            global_idx_offset += static_cast<uint32_t>(draw_list->IdxBuffer.Size);
            global_vtx_offset += static_cast<uint32_t>(draw_list->VtxBuffer.Size);
        }

        platform_io.Renderer_RenderState = nullptr;
        wq.set_scissor_region(command_list, 0, 0, static_cast<uint32_t>(fb_width), static_cast<uint32_t>(fb_height));
    }

    bool ui_context::begin_window(window_info info)
    {
        auto vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowViewport(vp->ID);

        struct position_visitor
        {
            void operator()(const math::vec2<float>& pos) const
            {
                ImGui::SetNextWindowPos({pos.x, pos.y}, ImGuiCond_Appearing, ImVec2(0.0f, 0.0f));
            }

            void operator()(default_position_tag_t) const
            {
            }

            void operator()(viewport_origin_tag_t) const
            {
                auto vp = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos({vp->Pos.x, vp->Pos.y}, ImGuiCond_Appearing, ImVec2(0.0f, 0.0f));
            }
        };

        struct size_visitor
        {
            void operator()(const math::vec2<float>& size) const
            {
                ImGui::SetNextWindowSize({size.x, size.y}, ImGuiCond_Always);
            }

            void operator()(default_size_tag_t) const
            {
            }

            void operator()(fullscreen_tag_t) const
            {
                auto vp = ImGui::GetMainViewport();
                ImGui::SetNextWindowSize({vp->Size.x, vp->Size.y}, ImGuiCond_Always);
            }
        };

        visit(position_visitor{}, info.position);
        visit(size_visitor{}, info.size);

        const auto window_flags = to_imgui(info.flags);

        if (holds_alternative<fullscreen_tag_t>(info.size))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        }

        const auto result = ImGui::Begin(info.name.data(), nullptr, window_flags);

        if (holds_alternative<fullscreen_tag_t>(info.size))
        {
            ImGui::PopStyleVar();
        }

        return result;
    }

    namespace
    {
        unique_ptr<ui_context::dockspace_layout> build_layouts(const ui_context::dockspace_configure_node& node,
                                                               ImGuiID& central_node)
        {
            auto layout = make_unique<ui_context::dockspace_layout>();

            // Handle the left and right splits
            if (node.left)
            {
                // Split the node left
                auto left_id =
                    ImGui::DockBuilderSplitNode(central_node, ImGuiDir_Left, node.left->size, nullptr, &central_node);
                layout->left_node = build_layouts(*node.left, left_id);
            }

            if (node.right)
            {
                // If there is a left node, compute the size of the right node. The size in the config is relative to
                // the full size, not the remaining space.
                auto right_size = node.right->size;
                if (layout->left_node)
                {
                    right_size = node.right->size / (1.0f - node.left->size);
                }

                // Split the node right
                auto right_id =
                    ImGui::DockBuilderSplitNode(central_node, ImGuiDir_Right, right_size, nullptr, &central_node);
                layout->right_node = build_layouts(*node.right, right_id);
            }

            if (node.top)
            {
                auto top_id =
                    ImGui::DockBuilderSplitNode(central_node, ImGuiDir_Up, node.top->size, nullptr, &central_node);
                layout->top_node = build_layouts(*node.top, top_id);
            }

            if (node.bottom)
            {
                // If there is a top node, compute the size of the bottom node. The size in the config is relative to
                // the full size, not the remaining space.
                auto bottom_size = node.bottom->size;
                if (layout->top_node)
                {
                    bottom_size = node.bottom->size / (1.0f - node.top->size);
                }
                auto bottom_id =
                    ImGui::DockBuilderSplitNode(central_node, ImGuiDir_Down, bottom_size, nullptr, &central_node);
                layout->bottom_node = build_layouts(*node.bottom, bottom_id);
            }

            for (auto&& window_name : node.docked_windows)
            {
                ImGui::DockBuilderDockWindow(window_name.data(), central_node);
            }

            layout->central_node = central_node;

            return layout;
        }
    } // namespace

    ui_context::dockspace_layout ui_context::configure_dockspace(dockspace_configure_info&& info)
    {
        auto dock_id = ImGui::GetID(info.name.data());
        ImGui::DockBuilderRemoveNode(dock_id);
        ImGui::DockBuilderAddNode(dock_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodePos(dock_id, {0, 0});
        ImGui::DockBuilderSetNodeSize(dock_id, ImGui::GetMainViewport()->Size);

        dockspace_layout identifiers;

        auto root_layout = build_layouts(info.root, dock_id);
        identifiers.central_node = root_layout->central_node;
        identifiers.left_node = tempest::move(root_layout->left_node);
        identifiers.right_node = tempest::move(root_layout->right_node);
        identifiers.top_node = tempest::move(root_layout->top_node);
        identifiers.bottom_node = tempest::move(root_layout->bottom_node);

        ImGui::DockBuilderFinish(dock_id);

        return identifiers;
    }

    void ui_context::dockspace(ui_context::dockspace_identifier id)
    {
        ImGui::DockSpace(id, {0, 0}, ImGuiDockNodeFlags_PassthruCentralNode);
    }

    bool ui_context::begin_menu_bar()
    {
        return ImGui::BeginMenuBar();
    }

    void ui_context::end_menu_bar()
    {
        ImGui::EndMenuBar();
    }

    bool ui_context::begin_menu(string_view name, bool enabled)
    {
        return ImGui::BeginMenu(name.data(), enabled);
    }

    void ui_context::end_menu()
    {
        ImGui::EndMenu();
    }

    bool ui_context::menu_item(string_view name, bool enabled)
    {
        return ImGui::MenuItem(name.data(), nullptr, nullptr, enabled);
    }

    void ui_context::text(string_view content, ...)
    {
        va_list args;
        va_start(args, content);
        ImGui::TextV(content.data(), args);
        va_end(args);
    }

    void ui_context::end_window()
    {
        ImGui::End();
    }

    math::vec2<uint32_t> ui_context::get_current_window_size() noexcept
    {
        const auto win_size = ImGui::GetWindowSize();
        return math::vec2(static_cast<uint32_t>(win_size.x), static_cast<uint32_t>(win_size.y));
    }

    ui_context::dockspace_identifier ui_context::get_dockspace_id(string_view name) noexcept
    {
        return ImGui::GetID(name.data());
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
            if (key_state.action != core::key_action::press && key_state.action != core::key_action::release)
            {
                return;
            }

            auto& io = ctx->IO;

            ImGuiKey key = convert_key(key_state);
            io.AddKeyEvent(key, key_state.action == core::key_action::press);
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
            if (mouse_state.action != core::mouse_action::press && mouse_state.action != core::mouse_action::release)
            {
                return;
            }

            auto& io = ctx->IO;

            ImGuiMouseButton button = -1;
            switch (mouse_state.button)
            {
            case core::mouse_button::left:
                button = ImGuiMouseButton_Left;
                break;
            case core::mouse_button::right:
                button = ImGuiMouseButton_Right;
                break;
            case core::mouse_button::middle:
                button = ImGuiMouseButton_Middle;
                break;
            default:
                return; // Unsupported mouse button
            }

            if (button >= 0 && button < ImGuiMouseButton_COUNT)
            {
                io.AddMouseButtonEvent(button, mouse_state.action == core::mouse_action::press);
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

        auto set_0_layout = _impl->device->create_descriptor_set_layout(
            set_0_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::push));

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
            .dst_alpha_blend_factor = rhi::blend_factor::one_minus_src_alpha,
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
                    .sample_shading = none(),
                    .alpha_to_coverage = false,
                    .alpha_to_one = false,
                },
            .rasterization =
                {
                    .depth_clamp_enable = false,
                    .rasterizer_discard_enable = false,
                    .polygon_mode = rhi::polygon_mode::fill,
                    .cull_mode = make_enum_mask(rhi::cull_mode::none),
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
        pipeline_desc.color_attachment_formats.push_back(_impl->render_backend_data.color_target_fmt);

        render_data.pipeline = _impl->device->create_graphics_pipeline(pipeline_desc);

        // Texture sampler state
        auto texture_sampler = rhi::sampler_desc{
            .mag = rhi::filter::linear,
            .min = rhi::filter::linear,
            .mipmap = rhi::mipmap_mode::linear,
            .address_u = rhi::address_mode::clamp_to_edge,
            .address_v = rhi::address_mode::clamp_to_edge,
            .address_w = rhi::address_mode::clamp_to_edge,
            .mip_lod_bias = 0.0f,
            .min_lod = -1000.0f,
            .max_lod = 1000.0f,
            .max_anisotropy = 1.0f,
            .compare = none(),
            .name = "ImGUI Texture Sampler",
        };

        render_data.texture_sampler = _impl->device->create_sampler(texture_sampler);

        // set up viewport data
        auto main_vp = _impl->imgui_context->Viewports[0];
        main_vp->RendererUserData = new render_viewport_data();
    }

    void ui_context::_setup_font_textures()
    {
        if (_impl->render_backend_data.font_texture)
        {
            _impl->device->destroy_image(_impl->render_backend_data.font_texture);
            _impl->render_backend_data.font_texture = rhi::typed_rhi_handle<rhi::rhi_handle_type::image>::null_handle;
        }

        auto& io = _impl->imgui_context->IO;

        auto& wq = _impl->device->get_primary_work_queue();
        auto cmds = wq.get_next_command_list();

        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        const auto upload_size = width * height * 4 * sizeof(char);

        auto font_tex_create_info = rhi::image_desc{
            .format = rhi::image_format::rgba8_unorm,
            .type = rhi::image_type::image_2d,
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::sampled, rhi::image_usage::transfer_dst),
            .name = "ImGUI Font Texture",
        };

        auto font_tex = _impl->device->create_image(font_tex_create_info);
        _impl->render_backend_data.font_texture = font_tex;

        // Set up the upload buffer
        auto upload_buffer_desc = rhi::buffer_desc{
            .size = upload_size,
            .location = rhi::memory_location::host,
            .usage = make_enum_mask(rhi::buffer_usage::transfer_src),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "ImGUI Font Upload Buffer",
        };

        auto upload_buffer = _impl->device->create_buffer(upload_buffer_desc);
        auto upload_buffer_data = _impl->device->map_buffer(upload_buffer);
        std::memcpy(upload_buffer_data, pixels, upload_size);
        _impl->device->unmap_buffer(upload_buffer);

        wq.begin_command_list(cmds, true);

        // Transition the font texture to transfer destination layout
        const array pre_barriers = {
            rhi::work_queue::image_barrier{
                .image = _impl->render_backend_data.font_texture,
                .old_layout = rhi::image_layout::undefined,
                .new_layout = rhi::image_layout::transfer_dst,
                .src_stages = make_enum_mask(rhi::pipeline_stage::bottom),
                .src_access = make_enum_mask(rhi::memory_access::none),
                .dst_stages = make_enum_mask(rhi::pipeline_stage::copy),
                .dst_access = make_enum_mask(rhi::memory_access::transfer_write),
            },
        };

        wq.transition_image(cmds, pre_barriers);
        wq.copy(cmds, upload_buffer, _impl->render_backend_data.font_texture, rhi::image_layout::transfer_dst, 0, 0);

        const array post_barriers = {
            rhi::work_queue::image_barrier{
                .image = _impl->render_backend_data.font_texture,
                .old_layout = rhi::image_layout::transfer_dst,
                .new_layout = rhi::image_layout::shader_read_only,
                .src_stages = make_enum_mask(rhi::pipeline_stage::copy),
                .src_access = make_enum_mask(rhi::memory_access::transfer_write),
                .dst_stages = make_enum_mask(rhi::pipeline_stage::fragment_shader),
                .dst_access = make_enum_mask(rhi::memory_access::shader_read, rhi::memory_access::shader_sampled_read),
            },
        };

        wq.transition_image(cmds, post_barriers);

        wq.end_command_list(cmds);

        rhi::work_queue::submit_info submit_info;
        submit_info.command_lists.push_back(cmds);

        wq.submit(tempest::span(&submit_info, 1));
        _impl->device->destroy_buffer(upload_buffer);

        auto packed_handle = math::pack_uint32x2(font_tex.generation, font_tex.id);
        io.Fonts->SetTexID(static_cast<ImTextureID>(packed_handle));
    }

    ui_pipeline::ui_pipeline(ui_context* ui_ctx) : _ui_ctx{ui_ctx}
    {
    }

    void ui_pipeline::initialize(graphics::renderer& parent, rhi::device& dev)
    {
        _device = &dev;
        _timeline_value = 0;
        _timeline_sem = dev.create_semaphore({
            .type = rhi::semaphore_type::timeline,
            .initial_value = _timeline_value,
        });
    }

    graphics::render_pipeline::render_result ui_pipeline::render(graphics::renderer& parent, rhi::device& dev,
                                                                 const graphics::render_pipeline::render_state& rs)
    {
        auto& queue = dev.get_primary_work_queue();

        // Use a submit to "split" a binary semaphore into signaling a timeline semaphore
        auto timeline_split_submit_info = rhi::work_queue::submit_info{};
        timeline_split_submit_info.wait_semaphores.push_back({
            .semaphore = rs.start_sem,
            .value = 0,
            .stages = make_enum_mask(rhi::pipeline_stage::color_attachment_output),
        });

        for (auto&& pipe : _child_pipelines)
        {
            timeline_split_submit_info.signal_semaphores.push_back({
                .semaphore = pipe.timeline_sem,
                .value = pipe.timeline_value + 1,
                .stages = make_enum_mask(rhi::pipeline_stage::color_attachment_output),
            });

            pipe.timeline_value += 1;
        }

        timeline_split_submit_info.signal_semaphores.push_back({
            .semaphore = _timeline_sem,
            .value = _timeline_value + 1,
            .stages = make_enum_mask(rhi::pipeline_stage::color_attachment_output),
        });

        _timeline_value += 1;

        queue.submit(tempest::span(&timeline_split_submit_info, 1));

        // Submit all the child pipelines
        // Gather the "wait" semaphores from the child pipelines
        auto child_wait_semaphores = vector<rhi::work_queue::semaphore_submit_info>{};

        for (auto&& pipe : _child_pipelines)
        {
            auto& child_pipeline = *pipe.pipeline;

            auto child_pipeline_render_state = render_pipeline::render_state{
                .start_sem = pipe.timeline_sem,
                .start_value = pipe.timeline_value,
                .end_sem = pipe.timeline_sem,
                .end_value = pipe.timeline_value + 1,
                .end_fence = rhi::typed_rhi_handle<rhi::rhi_handle_type::fence>::null_handle,
                .swapchain_image = rhi::typed_rhi_handle<rhi::rhi_handle_type::image>::null_handle,
                .surface = rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>::null_handle,
                .image_index = 0,
                .image_width = {},
                .image_height = {},
                .render_mode = graphics::render_pipeline::render_type::offscreen,
            };

            pipe.timeline_value += 1;

            child_pipeline.render(parent, dev, child_pipeline_render_state);

            // Add the child pipeline's end semaphore to the wait semaphores
            child_wait_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
                .semaphore = pipe.timeline_sem,
                .value = pipe.timeline_value,
                .stages = make_enum_mask(rhi::pipeline_stage::color_attachment_output),
            });
        }

        auto command_list = queue.get_next_command_list();
        queue.begin_command_list(command_list, true);

        // Transition the swapchain image to a render target
        const array pre_ui_barriers = {
            rhi::work_queue::image_barrier{
                .image = rs.swapchain_image,
                .old_layout = rhi::image_layout::undefined,
                .new_layout = rhi::image_layout::color_attachment,
                .src_stages = make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                .src_access = make_enum_mask(rhi::memory_access::none),
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
        submit_info.wait_semaphores = child_wait_semaphores;
        submit_info.wait_semaphores.push_back({
            .semaphore = _timeline_sem,
            .value = _timeline_value,
            .stages = make_enum_mask(rhi::pipeline_stage::color_attachment_output),
        });

        submit_info.signal_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = rs.end_sem,
            .value = 0,
            .stages = make_enum_mask(rhi::pipeline_stage::bottom),
        });

        // Increment the timeline semaphore value
        submit_info.signal_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = _timeline_sem,
            .value = _timeline_value + 1,
            .stages = make_enum_mask(rhi::pipeline_stage::bottom),
        });
        _timeline_value += 1;

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
            return render_result::request_recreate_swapchain;
        }
        else if (present_result == rhi::work_queue::present_result::error)
        {
            return render_result::failure;
        }
        return render_result::success;
    }

    void ui_pipeline::destroy([[maybe_unused]] graphics::renderer& parent, [[maybe_unused]] rhi::device& dev)
    {
    }

    void ui_pipeline::set_viewport(uint32_t width, uint32_t height)
    {
        _width = width;
        _height = height;
    }

    void ui_pipeline::set_viewport(viewport_pipeline_handle handle, uint32_t width, uint32_t height) noexcept
    {
        auto pipeline_it = _child_pipelines.find(handle);
        if (pipeline_it != _child_pipelines.end())
        {
            pipeline_it->pipeline->set_viewport(width, height);
        }
    }

    ui_pipeline::viewport_pipeline_handle ui_pipeline::register_viewport_pipeline(
        unique_ptr<graphics::render_pipeline> pipeline) noexcept
    {
        auto handle = _child_pipelines.insert({
            .timeline_sem = _device->create_semaphore({
                .type = rhi::semaphore_type::timeline,
                .initial_value = 0,
            }),

            .timeline_value = 0,
            .pipeline = tempest::move(pipeline),
        });

        return handle;
    }

    bool ui_pipeline::unregister_viewport_pipeline(viewport_pipeline_handle handle) noexcept
    {
        return _child_pipelines.erase(handle);
    }

    graphics::render_pipeline* ui_pipeline::get_viewport_pipeline(viewport_pipeline_handle handle) const noexcept
    {
        auto pipeline_it = _child_pipelines.find(handle);
        if (pipeline_it == _child_pipelines.end())
        {
            return nullptr;
        }
        else
        {
            return pipeline_it->pipeline.get();
        }
    }
} // namespace tempest::editor::ui