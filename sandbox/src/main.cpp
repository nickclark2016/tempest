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

    logger->info("Pi: {0}, Episilon: {1}, Infinity: {2}", tempest::math::constants::pi<float>, tempest::math::constants::epsilon<float>, tempest::math::constants::infinity<float>);

    tempest::math::vec<float, 2> v1(10, 10);
    tempest::math::vec<float, 2> v2(10, 15);

    logger->info("V1 x({0}), y({1})", v1.x, v1.y);
    logger->info("V2 x({0}), y({1})", v2.x, v2.y);

    if (v1 == v2)
    {
        logger->info("V1 Equal to V2");
    }

    v1 = v2;

    if (v1 == v2)
    {
        logger->info("V1 Equal to V2");
    }

    tempest::math::vec<float, 2> v3 = v1 + v2;

    v3 += v1;

    logger->info("V3 x({0}), y({1})", v3.x, v3.y);

    auto win = tempest::graphics::window_factory::create({
        .title{"Sandbox"},
        .width{1920},
        .height{1080},
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