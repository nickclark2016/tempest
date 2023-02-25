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

        static std::unique_ptr<irenderer> create(const iinstance& inst, const idevice& dev, const iwindow& win);
    };
} // namespace tempest::graphics

#endif // tempest_renderer_hpp__
