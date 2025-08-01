#include "asset_explorer.hpp"
#include "editor.hpp"
#include "entity_panes.hpp"
#include "viewport.hpp"

#include <tempest/tempest.hpp>
#include <tempest/ui.hpp>

namespace tempest::editor
{
    void run()
    {
        auto engine = tempest::engine_context{};
        auto [win_surface, inputs] = engine.register_window({
            .width = 1920,
            .height = 1080,
            .name = "Tempest Editor",
            .fullscreen = false,
        });

        auto ui_context = tempest::make_unique<ui::ui_context>(win_surface, &engine.get_renderer().get_device(),
                                                               rhi::image_format::bgra8_srgb, 3);
        auto ui_pipeline = engine.register_pipeline<ui::ui_pipeline>(win_surface, ui_context.get());
        ui_pipeline->set_viewport(win_surface->framebuffer_width(), win_surface->framebuffer_height());

        auto editor_ctx = editor_context{};

        editor_ctx.register_pane(tempest::make_unique<viewport>(), editor_context::dock_location::center);
        [[maybe_unused]] auto entity_pane = editor_ctx.register_pane(
            tempest::make_unique<entity_inspector>(engine.get_registry()), editor_context::dock_location::right);
        [[maybe_unused]] auto hierarchy_pane = editor_ctx.register_pane(
            tempest::make_unique<scene_hierarchy>(engine.get_registry()), editor_context::dock_location::left);
        editor_ctx.register_pane(tempest::make_unique<asset_explorer>(), editor_context::dock_location::bottom);

        engine.register_on_fixed_update_callback(
            [&ui_context, &editor_ctx](engine_context& ctx, auto dt) mutable { editor_ctx.draw(ctx, *ui_context); });

        engine.run();
    }
} // namespace tempest::editor

#if _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    tempest::editor::run();
    return 0;
}

#else

int main(int argc, char* argv[])
{
    tempest::editor::run();
    return 0;
}

#endif
