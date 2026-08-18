#ifndef tempest_render_system_renderer_hpp
#define tempest_render_system_renderer_hpp

#include <tempest/api.hpp>
#include <tempest/functional.hpp>
#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/rhi.hpp>

namespace tempest::render_system
{
    struct TEMPEST_API renderer_config
    {
        uint32_t render_width{1920};
        uint32_t render_height{1080};
        rhi::data_format hdr_color_format{rhi::data_format::rgba16_float};
        rhi::data_format depth_format{rhi::data_format::depth32_float};
        rhi::data_format tonemapped_color_format{rhi::data_format::rgba8_srgb};
        resource_pool_config pool_config{};
    };

    struct TEMPEST_API renderer_inputs
    {
        ecs::registry* entity_registry{nullptr};
        camera_system* camera_sys{nullptr};
    };

    class TEMPEST_API renderer
    {
      public:
        class TEMPEST_API builder
        {
          public:
            builder() = default;

            builder& set_config(const renderer_config& cfg)
            {
                _cfg = cfg;
                return *this;
            }

            builder& set_inputs(const renderer_inputs& inputs)
            {
                _inputs = inputs;
                return *this;
            }

            [[nodiscard]] auto build(rhi::device& dev, logger& log) -> unique_ptr<renderer>;

          private:
            renderer_config _cfg{};
            renderer_inputs _inputs{};
        };

        explicit renderer(rhi::device& dev, logger& log, renderer_config cfg, renderer_inputs inputs,
                          unique_ptr<camera_system> camera_sys = nullptr);
        ~renderer();

        renderer(const renderer&) = delete;
        renderer& operator=(const renderer&) = delete;
        renderer(renderer&&) noexcept;
        renderer& operator=(renderer&&) noexcept;

        /// @brief Loads mesh, texture, and material assets and extracts object instances from the given entities.
        void upload_objects_sync(span<const ecs::entity> entities, const core::mesh_registry& meshes,
                                 const core::texture_registry& textures, const core::material_registry& materials);

        /// @brief Builds the complete Render Graph DAG for the frame.
        void prepare_frame(uint32_t width, uint32_t height, optional<rhi::texture_handle> swapchain_tex = nullopt,
                            optional<rhi::texture_view_handle> swapchain_view = nullopt);

        /// @brief Executes the compiled Render Graph DAG on the GPU.
        auto render(const render_graph::frame_sync_options& sync = {}) -> expected<void, render_graph::execution_error>;

        /// @brief Resizes render targets and surface.
        void resize(uint32_t width, uint32_t height);

        [[nodiscard]] auto get_device() noexcept -> rhi::device&
        {
            return *_device;
        }

        [[nodiscard]] auto get_camera_system() noexcept -> camera_system&
        {
            return *_camera_system;
        }

        [[nodiscard]] auto get_resource_pool() noexcept -> resource_pool&
        {
            return _pool;
        }

        [[nodiscard]] auto get_shader_manager() noexcept -> shader_manager&
        {
            return _shaders;
        }

        [[nodiscard]] auto get_render_graph() noexcept -> render_graph::render_graph&
        {
            return _graph;
        }

        [[nodiscard]] auto get_tonemapped_color_texture() const noexcept -> render_graph::rg_texture_id
        {
            return _tonemapped_color_target;
        }

      private:
        rhi::device* _device{nullptr};
        logger* _log{nullptr};
        renderer_config _cfg;
        renderer_inputs _inputs;
        unique_ptr<camera_system> _owned_camera_system;
        camera_system* _camera_system{nullptr};

        resource_pool _pool;
        shader_manager _shaders;
        render_graph::render_graph _graph;

        // Render Targets (Transient in Render Graph)
        render_graph::rg_texture_id _hdr_color_target{};
        render_graph::rg_texture_id _depth_target{};
        render_graph::rg_texture_id _tonemapped_color_target{};

        uint32_t _active_draw_count{0};
    };
} // namespace tempest::render_system

#endif // tempest_render_system_renderer_hpp
