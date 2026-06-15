#include <tempest/shared_library.hpp>
#include <tempest/tempest.hpp>
#include <tempest/vector.hpp>

namespace
{
#if defined(TEMPEST_PLATFORM_WINDOWS)
    inline constexpr auto game_library_name = L"game-runtime.dll";
#elif defined(TEMPEST_PLATFORM_LINUX)
    inline constexpr auto game_library_name = "libgame-runtime.so";
#endif

    void run(tempest::span<tempest::string_view> args)
    {
        auto tempest_engine = tempest::standalone_engine_context();

        auto game_shared_library_result = tempest::shared_library::load(game_library_name);
        if (!game_shared_library_result)
        {
            tempest_engine.get_logger().fatal("Failed to load game shared library.");
            return;
        }

        const auto& game_shared_library = *game_shared_library_result;
        const auto on_load_result =
            game_shared_library
                .get_function_handle<void, tempest::engine_context*, tempest::span<tempest::string_view>>("on_load");
        const auto on_unload_result = game_shared_library.get_function_handle<void>("on_unload");

        if (!on_load_result || !on_unload_result)
        {
            // Handle function loading errors
            return;
        }

        auto on_load = *on_load_result;
        auto on_unload = *on_unload_result;

        [[maybe_unused]] auto window = tempest_engine.register_window(
            {
                .width = 1920,
                .height = 1080,
                .name = "Tempest Game",
                .fullscreen = false,
            },
            true);

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
            utf8_args.push_back(std::move(utf8_arg));
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