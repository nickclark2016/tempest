#ifndef tempest_graphics_pbr_pipeline_hpp
#define tempest_graphics_pbr_pipeline_hpp

#include <tempest/render_pipeline.hpp>

namespace tempest::graphics
{
    class pbr_pipeline : public render_pipeline
    {
      public:
        pbr_pipeline(uint32_t width, uint32_t height);
        pbr_pipeline(const pbr_pipeline&) = delete;
        pbr_pipeline(pbr_pipeline&&) = delete;
        ~pbr_pipeline() override = default;
        pbr_pipeline& operator=(const pbr_pipeline&) = delete;
        pbr_pipeline& operator=(pbr_pipeline&&) = delete;

        void initialize(renderer& parent, rhi::device& dev) override;
        render_result render(renderer& parent, rhi::device& dev, const render_state& rs) const override;
        void destroy(renderer& parent, rhi::device& dev) override;

      private:
        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::DESCRIPTOR_SET_LAYOUT> desc_set_0_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::PIPELINE_LAYOUT> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline;
        } _z_prepass = {};

        [[maybe_unused]] struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::PIPELINE_LAYOUT> build_cluster_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::COMPUTE_PIPELINE> build_clusters;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::PIPELINE_LAYOUT> fill_cluster_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::COMPUTE_PIPELINE> fill_clusters;
        } _forward_light_clustering = {};

        [[maybe_unused]] struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::PIPELINE_LAYOUT> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline;
        } _skybox_pass = {};

        [[maybe_unused]] struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::PIPELINE_LAYOUT> layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> pipeline;
        } _pbr_opaque = {};

        [[maybe_unused]] struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::PIPELINE_LAYOUT> oit_gather_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> oit_gather_pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::PIPELINE_LAYOUT> oit_resolve_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> oit_resolve_pipeline;

            rhi::typed_rhi_handle<rhi::rhi_handle_type::PIPELINE_LAYOUT> oit_blend_layout;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::GRAPHICS_PIPELINE> oit_blend_pipeline;
        } _pbr_transparencies = {};

        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> depth;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> color;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> encoded_normals;
            rhi::typed_rhi_handle<rhi::rhi_handle_type::IMAGE> positions;
        } _render_targets = {};

        static constexpr rhi::image_format depth_format = rhi::image_format::D32_FLOAT;
        static constexpr rhi::image_format color_format = rhi::image_format::RGBA8_SRGB;
        static constexpr rhi::image_format encoded_normals_format = rhi::image_format::RG16_FLOAT;
        static constexpr rhi::image_format positions_format = rhi::image_format::RGBA16_FLOAT;

        uint32_t _render_target_width;
        uint32_t _render_target_height;

        void _initialize_z_prepass(renderer& parent, rhi::device& dev);
        void _initialize_render_targets(renderer& parent, rhi::device& dev);
    };
} // namespace tempest::graphics

#endif // tempest_graphics_pbr_pipeline_hpp
