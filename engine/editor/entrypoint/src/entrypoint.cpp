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
#include <tempest/ui.hpp>
#include <tempest/vector.hpp>

#include <cstdio>

namespace
{
    using namespace tempest;

#if defined(TEMPEST_PLATFORM_WINDOWS)
    inline constexpr auto game_library_name = L"game-runtime.dll";
    inline constexpr auto game_editor_library_name = L"game-editor.dll";
#elif defined(TEMPEST_PLATFORM_LINUX)
    inline constexpr auto game_library_name = "libgame-runtime.so";
    inline constexpr auto game_editor_library_name = "libgame-editor.so";
#endif

    auto run(span<string_view> args) -> int
    {
        auto game_shared_library_result = shared_library::load(game_library_name);
        if (!game_shared_library_result)
        {
            return 1;
        }

        auto game_editor_shared_library_result = shared_library::load(game_editor_library_name);
        if (!game_editor_shared_library_result)
        {
            return 1;
        }

        const auto& game_shared_library = *game_shared_library_result;
        const auto game_on_load_result =
            game_shared_library.get_function_handle<void, engine_context*, span<string_view>>("on_load");
        const auto game_on_unload_result = game_shared_library.get_function_handle<void>("on_unload");

        if (!game_on_load_result || !game_on_unload_result)
        {
            return 1;
        }

        const auto& game_editor_shared_library = *game_editor_shared_library_result;
        const auto game_editor_on_load_result =
            game_editor_shared_library
                .get_function_handle<void, engine_context*, editor::editor_context*, span<string_view>>("on_load");
        const auto game_editor_on_unload_result = game_editor_shared_library.get_function_handle<void>("on_unload");

        if (!game_editor_on_load_result || !game_editor_on_unload_result)
        {
            return 1;
        }

        {
            auto tempest_engine = editor::editor_engine_context();

            auto window_data = tempest_engine.register_window(
                {
                    .width = 1920,
                    .height = 1080,
                    .title = "Tempest Editor",
                    .fullscreen = false,
                    .resizable = true,
                },
                false);

            if (!window_data.handle.is_valid())
            {
                tempest_engine.get_logger().fatal("Failed to create editor window.");
                return 1;
            }

            auto* const render_surface = tempest_engine.get_render_surface(window_data.handle);
            auto const target_format =
                render_surface ? rhi::to_data_format(render_surface->get_format()) : rhi::data_format::rgba8_unorm;

            auto ui_ctx = editor::ui_context(tempest_engine.get_window_manager(), window_data.handle,
                                             tempest_engine.get_device(), target_format, 3);

            auto ui_editor = editor::editor_context(tempest_engine, window_data.handle, ui_ctx);

            (*game_on_load_result)(&tempest_engine, args);
            (*game_editor_on_load_result)(&tempest_engine, &ui_editor, args);

            tempest_engine.run();

            (*game_editor_on_unload_result)();
            (*game_on_unload_result)();
        }

        return 0;
    }
} // namespace

#if defined(TEMPEST_PLATFORM_WINDOWS)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <windows.h>

auto WINAPI WinMain(HINSTANCE /*unused*/, HINSTANCE /*unused*/, LPSTR /*cmdline*/, int /*unused*/) -> int
{
    // Try to attach the console
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        auto* fp = static_cast<FILE*>(nullptr);
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

    LocalFree(args);

    // Convert to vector of string_view for easier handling
    auto arg_views = tempest::vector<tempest::string_view>{};
    for (const auto& arg : utf8_args)
    {
        arg_views.push_back(arg);
    }

    return run(arg_views);
}
#else
int main(int argc, char* argv[])
{
    auto arg_views = tempest::vector<tempest::string_view>{};
    for (int i = 0; i < argc; ++i)
    {
        arg_views.push_back(argv[i]);
    }

    return run(arg_views);
}
#endif