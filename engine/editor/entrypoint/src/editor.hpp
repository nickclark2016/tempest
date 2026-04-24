#ifndef tempest_editor_entrypoint_editor_hpp
#define tempest_editor_entrypoint_editor_hpp

#include "ui.hpp"

#include <tempest/tempest.hpp>

namespace tempest::editor
{
    class editor
    {
      public:
        struct draw_data
        {
        };

        editor(engine_context& ctx, rhi::window_surface* win_surface, ui::ui_context* ui_ctx);
        void draw(const draw_data& data);

        graphics::graph_resource_handle<rhi::rhi_handle_type::image> get_final_color_target() const noexcept
        {
            return _final_color_target;
        }

      private:
        engine_context* _ctx;
        rhi::window_surface* _win_surface;
        ui::ui_context* _ui_ctx;

        struct
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::image> viewport_texture = rhi::null_handle;
            math::vec2<uint32_t> viewport_image_size{};
            bool open = true;
        } _viewport_state;

        struct
        {
            ecs::archetype_entity selected_entity = ecs::null;
            bool open = true;
        } _entity_hierarchy_state;

        struct
        {
            bool open = true;
        } _entity_properties_state;

        graphics::graph_resource_handle<rhi::rhi_handle_type::image> _final_color_target{};

        void _configure_dockspace();
        void _draw_viewport();
        void _draw_scene_hierarchy();
        void _draw_entity_properties();
    };
} // namespace tempest::editor

#endif // tempest_editor_entrypoint_editor_hpp
