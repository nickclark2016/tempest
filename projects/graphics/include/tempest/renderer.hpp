#ifndef tempest_irenderer_hpp__
#define tempest_irenderer_hpp__

#include <tempest/version.hpp>
#include <tempest/window.hpp>

#include <memory>
#include <string_view>

namespace tempest::graphics
{
    class irenderer
    {
      public:
        static constexpr std::string_view BACKBUFFER_NAME = "DEFAULT_BACK_BUFFER";

        static std::unique_ptr<irenderer> create(const core::version& version_info, iwindow& win);

        ~irenderer();

        void render();
      private:
        struct impl;
        std::unique_ptr<impl> _impl;

        irenderer(const core::version& version_info, iwindow& win);
    };
}

#endif // tempest_irenderer_hpp__
