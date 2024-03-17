#ifndef tempest_input_hpp
#define tempest_input_hpp

#include "keyboard.hpp"

namespace tempest::core
{
    struct input
    {
        static void poll();
    };

    struct input_group
    {
        keyboard* kb;
    };
}

#endif // tempest_input_hpp
