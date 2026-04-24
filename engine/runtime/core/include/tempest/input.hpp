#ifndef tempest_input_hpp
#define tempest_input_hpp

#include <tempest/api.hpp>
#include <tempest/keyboard.hpp>
#include <tempest/mouse.hpp>

namespace tempest::core
{
    struct TEMPEST_API input
    {
        static void poll();
    };

    struct TEMPEST_API input_group
    {
        keyboard* kb;
        mouse* ms;
    };
}

#endif // tempest_input_hpp
