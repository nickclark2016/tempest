#ifndef tempest_profiler_web_assets_hpp
#define tempest_profiler_web_assets_hpp

#include <tempest/string_view.hpp>

namespace tempest::profiler
{
    auto get_embedded_index_html() noexcept -> string_view;
    auto get_embedded_app_js() noexcept -> string_view;
    auto get_embedded_styles_css() noexcept -> string_view;
} // namespace tempest::profiler

#endif // tempest_profiler_web_assets_hpp
