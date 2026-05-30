#include <tempest/tempest.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GAME_API __declspec(dllexport)
#elif defined(TEMPEST_PLATFORM_LINUX)
#define GAME_API __attribute__((visibility("default")))
#else
#error "Unsupported platform"
#endif

extern "C"
{
    GAME_API void on_load(tempest::engine_context* engine, tempest::span<tempest::string_view> /*args*/){

    }

    GAME_API void on_unload()
    {
        // Cleanup code for the game goes here
    }
}