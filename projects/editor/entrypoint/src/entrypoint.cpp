#include <tempest/tempest.hpp>
#include <tempest/ui.hpp>

namespace tempest::editor
{
    struct window_context
    {
        unique_ptr<rhi::window_surface> surface;
        unique_ptr<ui::ui_context> ui_context;
    };

    struct editor_context
    {
        vector<window_context> windows;
        unique_ptr<graphics::renderer> renderer;
    };

    void run()
    {
        auto engine = tempest::engine_context{};
        auto [win_surface, inputs] = engine.register_window({
            .width = 1920,
            .height = 1080,
            .name = "Tempest Editor",
            .fullscreen = false,
        });

        auto ui_context = tempest::make_unique<ui::ui_context>(win_surface, &engine.get_renderer().get_device(), rhi::image_format::bgra8_srgb);
        auto ui_pipeline = engine.register_pipeline<ui::ui_pipeline>(win_surface, ui_context.get());
        ui_pipeline->set_size(win_surface->framebuffer_width(), win_surface->framebuffer_height());

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
