#include <tempest/archetype.hpp>
#include <tempest/input.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/rhi.hpp>
#include <tempest/ui.hpp>

namespace tempest::editor
{
    struct editor_context
    {
        rhi::window_surface* surface;
        rhi::device* device;
        graphics::renderer* renderer;
    };

    void run()
    {
        auto renderer = make_unique<graphics::renderer>();
        auto window = renderer->create_window({
            .width = 1920,
            .height = 1080,
            .name = "Tempest Editor",
            .fullscreen = false,
        });
        auto ui_context = editor::ui::ui_context(window.get(), &renderer->get_device());

        bool should_close = false;

        window->register_close_callback([&should_close]() { should_close = true; });

        while (!should_close)
        {
            core::input::poll();

            ui_context.begin_ui_commands();
            ui_context.finish_ui_commands();
        }
    }
} // namespace tempest::editor

#if _WIN32

#include <windows.h>

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
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
