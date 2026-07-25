#ifndef tempest_graphics_renderer_hpp
#define tempest_graphics_renderer_hpp

#include <tempest/api.hpp>
#include <tempest/camera_system.hpp>
#include <tempest/functional.hpp>
#include <tempest/logger.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/rhi.hpp>

namespace tempest::graphics
{
    class TEMPEST_API renderer
    {
      public:
        class TEMPEST_API builder
        {
          public:
            builder() = default;

            builder& set_pbr_frame_graph_config(const pbr_frame_graph_config& config)
            {
                _pbr_cfg = config;
                return *this;
            }

            builder& set_pbr_frame_graph_inputs(const pbr_frame_graph_inputs& inputs)
            {
                _pbr_inputs = inputs;
                return *this;
            }

            builder& add_pbr_customization(function<void(pbr_frame_graph&)> callback);

            [[nodiscard]] renderer build(logger& log);

          private:
            pbr_frame_graph_config _pbr_cfg;
            pbr_frame_graph_inputs _pbr_inputs;
            vector<function<void(pbr_frame_graph&)>> _pbr_customization_callbacks;
        };

        renderer(const renderer&) = delete;
        renderer(renderer&&) noexcept = default;
        renderer& operator=(const renderer&) = delete;
        renderer& operator=(renderer&&) noexcept = default;

        ~renderer() = default;

        tuple<unique_ptr<rhi::window_surface>, rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>> create_window(
            const rhi::window_surface_desc& desc, bool install_swapchain_blit = true);

        void destroy_window(rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> handle);

        void upload_objects_sync(span<const ecs::entity> entities, const core::mesh_registry& meshes,
                                 const core::texture_registry& textures, const core::material_registry& materials);

        void finalize_graph();

        void render();

        pbr_frame_graph& get_frame_graph() noexcept
        {
            return *_graph;
        }

        const pbr_frame_graph& get_frame_graph() const noexcept
        {
            return *_graph;
        }

        pbr_frame_graph& get_pbr_frame_graph() noexcept
        {
            return *_graph;
        }

        const pbr_frame_graph& get_pbr_frame_graph() const noexcept
        {
            return *_graph;
        }

        rhi::device& get_device() noexcept
        {
            return *_device;
        }

        const rhi::device& get_device() const noexcept
        {
            return *_device;
        }

        /// @brief Gets the camera system associated with this renderer.
        [[nodiscard]] camera_system& get_camera_system() noexcept
        {
            return *_camera_system;
        }

        /// @brief Gets the camera system associated with this renderer.
        [[nodiscard]] const camera_system& get_camera_system() const noexcept
        {
            return *_camera_system;
        }

      private:
        explicit renderer(logger& log, unique_ptr<rhi::instance> instance, rhi::device& device,
                          unique_ptr<pbr_frame_graph> graph, unique_ptr<camera_system> camera_sys);

        logger* _log;
        unique_ptr<rhi::instance> _instance;
        rhi::device* _device;
        unique_ptr<pbr_frame_graph> _graph;
        unique_ptr<camera_system> _camera_system;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_renderer_hpp
