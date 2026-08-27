#include <tempest/editor.hpp>
#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/menus/menu_item.hpp>
#include <tempest/tempest.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GAME_API __declspec(dllexport)
#elif defined(TEMPEST_PLATFORM_LINUX)
#define GAME_API __attribute__((visibility("default")))
#else
#error "Unsupported platform"
#endif

namespace
{
    class sample_game_menu_item final : public tempest::editor::menu_item
    {
      public:
        explicit sample_game_menu_item(tempest::engine_context& ctx)
            : tempest::editor::menu_item("Game", "Log Status"), _ctx(&ctx)
        {
        }

        auto on_press() noexcept -> void override
        {
            _ctx->get_logger().info("Game menu action triggered!");
        }

      private:
        tempest::engine_context* _ctx;
    };
} // namespace

extern "C"
{
    GAME_API void on_load(tempest::engine_context* engine, tempest::editor::editor_context* editor,
                          [[maybe_unused]] tempest::span<tempest::string_view> args)
    {
        auto& logger = engine->get_logger();
        logger.info("Game Editor loaded successfully!");

        if (editor != nullptr)
        {
            editor->register_menu_item(tempest::make_unique<sample_game_menu_item>(*engine));
        }
    }

    GAME_API void on_unload()
    {
    }
}