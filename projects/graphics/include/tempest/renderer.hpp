#ifndef tempest_renderer_hpp__
#define tempest_renderer_hpp__

#include "instance.hpp"
#include "window.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace tempest::graphics
{
    class icommands
    {
      public:
        virtual ~icommands() = default;
    };

    class irenderer
    {
      public:
        using draw_command = std::function<void(icommands&)>;

        virtual ~irenderer() = default;

        virtual void draw(const draw_command& cmd) = 0;

        static std::unique_ptr<irenderer> create(const iinstance& inst, const idevice& dev, const iwindow& win);
    };
} // namespace tempest::graphics

#endif // tempest_renderer_hpp__
