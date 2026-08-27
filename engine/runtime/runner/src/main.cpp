#include <tempest/move.hpp>
#include <tempest/shared_library.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/tempest.hpp>
#include <tempest/vector.hpp>

namespace
{
    using namespace tempest;

#if defined(TEMPEST_PLATFORM_WINDOWS)
    inline constexpr auto game_library_name = L"game-runtime.dll";
#elif defined(TEMPEST_PLATFORM_LINUX)
    inline constexpr auto game_library_name = "libgame-runtime.so";
#endif

    auto run(span<string_view> args) -> int
    {
        auto tempest_engine = standalone_engine_context();

        auto game_shared_library_result = shared_library::load(game_library_name);
        if (!game_shared_library_result)
        {
            tempest_engine.get_logger().fatal("Failed to load game shared library.");
            return 1;
        }

        const auto& game_shared_library = *game_shared_library_result;
        const auto on_load_result =
            game_shared_library.get_function_handle<void, engine_context*, span<string_view>>("on_load");
        const auto on_unload_result = game_shared_library.get_function_handle<void>("on_unload");

        if (!on_load_result || !on_unload_result)
        {
            tempest_engine.get_logger().fatal("Failed to load on_load or on_unload from game shared library.");
            return 1;
        }

        auto window_data = tempest_engine.register_window(
            {
                .width = 1920,
                .height = 1080,
                .title = "Tempest Game",
                .fullscreen = false,
                .resizable = true,
            },
            true);

        if (!window_data.handle.is_valid())
        {
            tempest_engine.get_logger().fatal("Failed to create game window.");
            return 1;
        }

        (*on_load_result)(&tempest_engine, args);
        tempest_engine.run();
        (*on_unload_result)();

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