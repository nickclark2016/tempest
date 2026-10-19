#include <tempest/array.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/format.hpp>
#include <tempest/game_module.hpp>
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

#ifdef TEMPEST_PLATFORM_WINDOWS
    inline constexpr auto game_library_name = L"game-runtime.dll";
#elif defined(TEMPEST_PLATFORM_LINUX)
    inline constexpr auto game_library_name = "libgame-runtime.so";
#endif


    auto run(span<string_view> args) -> int
    {
        auto game_shared_library_result = shared_library::load(game_library_name);
        if (!game_shared_library_result)
        {
            return 1;
        }

        const auto& game_shared_library = *game_shared_library_result;
        const auto game_register_types_result =
            game_shared_library.get_function_handle<void, ecs::component_type_registry&>("tempest_game_register_types");
        const auto game_on_load_result =
            game_shared_library.get_function_handle<void, engine_context&, span<const string_view>>("tempest_game_on_load");
        const auto game_on_unload_result =
            game_shared_library.get_function_handle<void>("tempest_game_on_unload");
        const auto game_on_play_begin_result =
            game_shared_library.get_function_handle<void, const play_settings&>("tempest_game_on_play_begin");
        const auto game_on_play_end_result =
            game_shared_library.get_function_handle<void>("tempest_game_on_play_end");

        const auto legacy_on_load_result =
            game_shared_library.get_function_handle<void, client_context*, span<string_view>>("on_load");
        const auto legacy_on_unload_result = game_shared_library.get_function_handle<void>("on_unload");

        if (!game_on_load_result && !legacy_on_load_result)
        {
            return 1;
        }

        {
            auto tempest_engine = standalone_engine_context();

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

            if (game_on_load_result)
            {
                if (game_register_types_result)
                {
                    (*game_register_types_result)(tempest_engine.get_components());
                }

                const auto scene_candidates = array<filesystem::path, 2>{
                    filesystem::path{"game/assets/scenes/main.tscene"},
                    filesystem::path{"assets/scenes/main.tscene"},
                };

                for (const auto& scene_path : scene_candidates)
                {
                    if (filesystem::exists(scene_path))
                    {
                        const auto load_result = tempest_engine.get_scene_manager().load_sync(scene_path);
                        if (load_result.has_value())
                        {
                            tempest_engine.get_logger().info(format("Loaded startup scene: {}", scene_path.generic_string()));
                            break;
                        }
                    }
                }

                (*game_on_load_result)(tempest_engine, span<const string_view>{args.data(), args.size()});

                auto settings = play_settings{};
                for (const auto& arg : args)
                {
                    constexpr auto prefix = string_view{"--connect="};
                    if (tempest::starts_with(arg, prefix))
                    {
                        settings.mode = network_mode::connect_remote;
                        settings.remote_address = string_view{arg.data() + prefix.size(), arg.size() - prefix.size()};
                    }
                }

                if (game_on_play_begin_result)
                {
                    (*game_on_play_begin_result)(settings);
                }

                tempest_engine.run();

                if (game_on_play_end_result)
                {
                    (*game_on_play_end_result)();
                }

                if (game_on_unload_result)
                {
                    (*game_on_unload_result)();
                }
            }
            else if (legacy_on_load_result)
            {
                (*legacy_on_load_result)(&tempest_engine, args);
                tempest_engine.run();
                if (legacy_on_unload_result)
                {
                    (*legacy_on_unload_result)();
                }
            }
        }

        return 0;
    }
} // namespace

#ifdef TEMPEST_PLATFORM_WINDOWS

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <windows.h>

auto WINAPI WinMain(HINSTANCE /*unused*/, HINSTANCE /*unused*/, LPSTR /*cmdline*/, int /*unused*/) -> int
{
    auto arg_count = 0;
    auto* const args = CommandLineToArgvW(GetCommandLineW(), &arg_count);

    auto utf8_args = tempest::vector<tempest::string>{};
    for (auto arg_idx = 0; arg_idx < arg_count; ++arg_idx)
    {
        const auto utf8_size = WideCharToMultiByte(CP_UTF8, 0, args[arg_idx], -1, nullptr, 0, nullptr, nullptr);
        if (utf8_size > 0)
        {
            auto utf8_arg = tempest::string(utf8_size - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, args[arg_idx], -1, utf8_arg.data(), utf8_size, nullptr, nullptr);
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
    for (auto arg_idx = 0; arg_idx < argc; ++arg_idx)
    {
        arg_views.push_back(argv[arg_idx]);
    }

    return run(arg_views);
}
#endif