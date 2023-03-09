#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/renderer.hpp>
#include <tempest/window.hpp>
#include <tempest/math.hpp>

#include <cassert>
#include <fstream>
#include <sstream>

namespace
{
    std::vector<uint32_t> read_spirv(const std::string& path)
    {
        std::ostringstream buf;
        std::ifstream input(path.c_str(), std::ios::ate | std::ios::binary);
        assert(input);
        size_t file_size = (size_t)input.tellg();
        std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(buffer.data()), file_size);
        return buffer;
    }
} // namespace

int main()
{
    auto logger = tempest::logger::logger_factory::create({
        .prefix{"Sandbox"},
    });

    logger->info("Starting Sandbox Application.");

    tempest::math::vec<std::uint32_t, 2> window_size(1920, 1080);

    auto win = tempest::graphics::window_factory::create({
        .title{"Sandbox"},
        .width{window_size.x},
        .height{window_size.y},
    });

    auto renderer = tempest::graphics::irenderer::create({
        .major{0},
        .minor{0},
        .patch{1},
    }, *win);

    while (!win->should_close())
    {
        tempest::input::poll();

        renderer->render();
    }

    logger->info("Exiting Sandbox Application.");

    return 0;
}