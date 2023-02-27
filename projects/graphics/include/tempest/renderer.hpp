#ifndef tempest_renderer_hpp__
#define tempest_renderer_hpp__

#include "instance.hpp"
#include "window.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace tempest::graphics
{
    class irenderer
    {
      public:
        virtual ~irenderer() = default;

        virtual void draw() = 0;

        static std::unique_ptr<irenderer> create();
    };
} // namespace tempest::graphics

#endif // tempest_renderer_hpp__
