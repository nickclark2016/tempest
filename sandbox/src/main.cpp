#include <tempest/tempest.hpp>

int main()
{
    auto eng = tempest::engine::initialize();
    
    eng.add_window(tempest::graphics::window_factory::create({
        .title{"Tempest"},
        .width{1920},
        .height{1080},
    }));

    eng.run();

    return 0;
}