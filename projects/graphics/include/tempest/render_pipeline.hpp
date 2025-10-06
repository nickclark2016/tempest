#ifndef tempest_graphics_render_pipeline_hpp
#define tempest_graphics_render_pipeline_hpp

#include <tempest/archetype.hpp>
#include <tempest/material.hpp>
#include <tempest/rhi.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

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

        void upload_objects_sync(span<const ecs::archetype_entity> entities, const core::mesh_registry& meshes,
                                 const core::texture_registry& textures, const core::material_registry& materials);

      private:
        unique_ptr<rhi::instance> _rhi_instance;
        rhi::device* _rhi_device;

        struct window_payload
        {
            rhi::window_surface* win;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> render_surface;
            unique_ptr<render_pipeline> pipeline;
            bool framebuffer_resized;
        };

        vector<window_payload> _windows;
        vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::fence>> _in_flight_fences;
        uint64_t _current_frame = 0;
    };

    class render_pipeline
    {
      public:
        enum class render_type
        {
            offscreen,
            swapchain,
        };

        struct render_state
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore> start_sem;
            uint64_t start_value;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore> end_sem;
            uint64_t end_value;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::fence> end_fence;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> swapchain_image;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> surface;
            uint32_t image_index;
            uint32_t image_width;
            uint32_t image_height;
            render_type render_mode;
        };

        enum class render_result
        {
            success,
            request_recreate_swapchain,
            failure,
        };

        struct render_target_info
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image;
            rhi::image_layout layout;
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

        virtual void set_viewport(uint32_t width, uint32_t height) = 0;

        virtual void upload_objects_sync(rhi::device& dev, span<const ecs::archetype_entity> entities,
                                         const core::mesh_registry& meshes, const core::texture_registry& textures,
                                         const core::material_registry& materials);

        virtual render_target_info get_render_target() const;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_pipeline_hpp
