#ifndef tempest_input_hpp
#define tempest_input_hpp

#include <tempest/keyboard.hpp>
#include <tempest/mouse.hpp>

namespace tempest::core
{
    struct input
    {
        static void poll();
    };

    struct input_group
    {
        keyboard* kb;
        mouse* ms;
    };
}

#endif // tempest_input_hpp
