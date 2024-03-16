#include <tempest/tempest.hpp>

int main()
{   
    auto eng = tempest::engine::initialize();

    eng.add_window(tempest::graphics::window_factory::create({
        .title{"Tempest"},
        .width{1920},
        .height{1080},
    }));

    eng.on_initialize([](tempest::engine& eng) {
        auto sponza = eng.load_asset("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf");
    });

    eng.run();

    return 0;
}