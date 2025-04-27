#ifndef tempest_graphics_render_pipeline_hpp
#define tempest_graphics_render_pipeline_hpp

#include <tempest/rhi.hpp>

namespace tempest::graphics
{
    class render_pipeline;

    class renderer
    {
      public:
        renderer();

        unique_ptr<rhi::window_surface> create_window(const rhi::window_surface_desc& desc);
        void register_window(rhi::window_surface* window, unique_ptr<render_pipeline> pipeline);
        void unregister_window(rhi::window_surface* window);
        bool render();

      private:
        unique_ptr<rhi::instance> _rhi_instance;
        rhi::device* _rhi_device;

        struct window_payload
        {
            rhi::window_surface* win;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::RENDER_SURFACE> render_surface;
            unique_ptr<render_pipeline> pipeline;
        };

        vector<window_payload> _windows;
    };

    class render_pipeline
    {
      public:
        struct render_state
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::SEMAPHORE> start_sem;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::SEMAPHORE> end_sem;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::FENCE> end_fence;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> swapchain_image;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::RENDER_SURFACE> surface;
            uint32_t image_index;
        };

        enum render_result
        {
            SUCCESS,
            REQUEST_RECREATE_SWAPCHAIN,
            FAILURE,
        };

        render_pipeline() = default;
        render_pipeline(const render_pipeline&) = delete;
        render_pipeline(render_pipeline&&) = delete;
        virtual ~render_pipeline() = default;

        render_pipeline& operator=(const render_pipeline&) = delete;
        render_pipeline& operator=(render_pipeline&&) = delete;

        virtual void initialize(renderer& parent, rhi::device& dev) = 0;
        virtual render_result render(renderer& parent, rhi::device& dev, const render_state& rs) const = 0;
        virtual void destroy(renderer& parent, rhi::device& dev) = 0;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_pipeline_hpp
