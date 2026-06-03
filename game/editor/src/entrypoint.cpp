#include <tempest/editor.hpp>
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
    GAME_API void on_load(tempest::engine_context* engine, tempest::editor::editor_context* editor,
                          [[maybe_unused]] tempest::span<tempest::string_view> args)
    {
        engine->register_on_initialize_callback([](tempest::engine_context& ctx) {
            for (auto&& [self, transform] :
                 ctx.get_registry().with<tempest::ecs::self_component, tempest::ecs::transform_component>())
            {
            }
        });
    }

    GAME_API void on_unload()
    {
    }
}