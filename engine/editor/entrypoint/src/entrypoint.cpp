#include <tempest/archetype.hpp>
#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/memory.hpp>
#include <tempest/move.hpp>
#include <tempest/rhi.hpp>
#include <tempest/shared_library.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/ui.hpp>
#include <tempest/vector.hpp>
#include <tempest/windows/engine_component_view_providers.hpp>
#include <tempest/windows/entity_view_window.hpp>
#include <tempest/windows/scene_hierarchy_window.hpp>
#include <tempest/windows/viewport_window.hpp>

#include <cstdio>

namespace
{
    using namespace tempest;

#if defined(TEMPEST_PLATFORM_WINDOWS)
    inline constexpr auto game_library_name = L"game-runtime.dll";
    inline constexpr auto game_editor_library_name = L"game-editor.dll";
#elif defined(TEMPEST_PLATFORM_LINUX)
    inline constexpr auto game_library_name = "libgame-runtime.so";
    inline constexpr auto game_editor_library_name = L"libgame-editor.so";
#endif

    static unique_ptr<editor::editor_context> setup_render_graph(editor::editor_engine_context& ctx,
                                                                 rhi::window_surface& win_surface,
                                                                 editor::ui_context& ui_ctx)
    {
        return make_unique<editor::editor_context>(ctx, win_surface, ui_ctx);
    }

    void run(span<string_view> args)
    {
        auto tempest_engine = tempest::editor::editor_engine_context();

        auto game_shared_library_result = tempest::shared_library::load(game_library_name);
        if (!game_shared_library_result)
        {
            tempest_engine.get_logger().fatal("Failed to load game shared library.");
            return;
        }

        auto game_editor_shared_library_result = shared_library::load(game_editor_library_name);
        if (!game_editor_shared_library_result)
        {
            tempest_engine.get_logger().fatal("Failed to load game editor shared library.");
            return;
        }

        const auto& game_shared_library = *game_shared_library_result;
        const auto game_on_load_result =
            game_shared_library
                .get_function_handle<void, tempest::engine_context*, tempest::span<tempest::string_view>>("on_load");
        const auto game_on_unload_result = game_shared_library.get_function_handle<void>("on_unload");

        if (!game_on_load_result || !game_on_unload_result)
        {
            // Handle function loading errors
            return;
        }

        const auto& game_editor_shared_library = *game_editor_shared_library_result;
        const auto game_editor_on_load_result = game_editor_shared_library.get_function_handle<
            void, tempest::engine_context*, tempest::editor::editor_context*, tempest::span<tempest::string_view>>(
            "on_load");
        const auto game_editor_on_unload_result = game_editor_shared_library.get_function_handle<void>("on_unload");

        if (!game_editor_on_load_result || !game_editor_on_unload_result)
        {
            // Handle function loading errors
            return;
        }

        auto window_data = tempest_engine.register_window(
            {
                .width = 1920,
                .height = 1080,
                .name = "Tempest Editor",
                .fullscreen = false,
            },
            false);

        auto&& [win_surface, render_surface, inputs] = window_data;
        auto ui_ctx =
            editor::ui_context(win_surface, &tempest_engine.get_renderer().get_device(),
                               tempest_engine.get_renderer().get_frame_graph().get_tonemapped_color_format(), 3);

        auto ui_editor = unique_ptr<editor::editor_context>();

        tempest_engine.register_on_initialize_callback([&](engine_context& ctx) {
            ui_editor = setup_render_graph(static_cast<editor::editor_engine_context&>(ctx), *win_surface, ui_ctx);
        });

        auto on_load = [&](auto&& engine, auto&& args) {
            (*game_on_load_result)(engine, args);
            (*game_editor_on_load_result)(engine, ui_editor.get(), args);
        };

        auto on_unload = [&]() {
            (*game_editor_on_unload_result)();
            (*game_on_unload_result)();
        };

        on_load(&tempest_engine, args);
        tempest_engine.run();
        on_unload();
    }
} // namespace

#if defined(TEMPEST_PLATFORM_WINDOWS)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <windows.h>

auto WINAPI WinMain(HINSTANCE /*unused*/, HINSTANCE /*unused*/, LPSTR cmdline, int /*unused*/) -> int
{
    // Try to attach the console
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        auto fp = static_cast<FILE*>(nullptr);
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
    }

    auto arg_count = 0;
    auto* const args = CommandLineToArgvW(GetCommandLineW(), &arg_count);

    auto utf8_args = tempest::vector<tempest::string>{};
    for (int i = 0; i < arg_count; ++i)
    {
        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, args[i], -1, nullptr, 0, nullptr, nullptr);
        if (utf8_size > 0)
        {
            auto utf8_arg = tempest::string(utf8_size - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, args[i], -1, utf8_arg.data(), utf8_size, nullptr, nullptr);
            utf8_args.push_back(tempest::move(utf8_arg));
        }
        else
        {
            utf8_args.push_back({});
        }
    }

    LocalFree((void*)args);

    // Convert to vector of string_view for easier handling
    auto arg_views = tempest::vector<tempest::string_view>{};
    for (const auto& arg : utf8_args)
    {
        arg_views.push_back(arg);
    }

    run(arg_views);
}
#else
int main(int argc, char* argv[])
{
    auto arg_views = tempest::vector<tempest::string_view>{};
    for (int i = 0; i < argc; ++i)
    {
        arg_views.push_back(argv[i]);
    }

    run(arg_views);
}
#endif