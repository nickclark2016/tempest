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
        render_pipeline* register_window(rhi::window_surface* window, unique_ptr<render_pipeline> pipeline);
        void unregister_window(rhi::window_surface* window);
        bool render();

        template <typename T, typename... Args>
            requires derived_from<T, render_pipeline>
        T* register_window(rhi::window_surface* window, Args&&... args)
        {
            auto pipeline = tempest::make_unique<T>(tempest::forward<Args>(args)...);
            auto register_result = register_window(window, tempest::move(pipeline));
            return static_cast<T*>(register_result);
        }

        rhi::device& get_device() noexcept
        {
            return *_rhi_device;
        }

        const rhi::device& get_device() const noexcept
        {
            return *_rhi_device;
        }

      private:
        unique_ptr<rhi::instance> _rhi_instance;
        rhi::device* _rhi_device;

        struct window_payload
        {
            rhi::window_surface* win;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> render_surface;
            unique_ptr<render_pipeline> pipeline;
        };

        vector<window_payload> _windows;
    };

    class render_pipeline
    {
      public:
        struct render_state
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore> start_sem;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore> end_sem;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::fence> end_fence;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> swapchain_image;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> surface;
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
        virtual render_result render(renderer& parent, rhi::device& dev, const render_state& rs) = 0;
        virtual void destroy(renderer& parent, rhi::device& dev) = 0;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_pipeline_hpp
