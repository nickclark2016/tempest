#ifndef tempest_editor_ui_ui_hpp
#define tempest_editor_ui_ui_hpp

#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/rhi.hpp>
#include <tempest/string_view.hpp>
#include <tempest/variant.hpp>
#include <tempest/vec2.hpp>

namespace tempest::editor::ui
{
    class ui_context
    {
      public:
        struct default_position_tag_t
        {
        };

        static constexpr default_position_tag_t default_position_tag{};

        struct viewport_origin_tag_t
        {
        };

        static constexpr viewport_origin_tag_t viewport_origin_tag{};

        struct default_size_tag_t
        {
        };

        static constexpr default_size_tag_t default_size_tag{};

        struct fullscreen_tag_t
        {
        };

        static constexpr fullscreen_tag_t fullscreen_tag{};

        enum class window_flags
        {
            none = 0x000,
            no_title = 0x001,
            no_resize = 0x002,
            no_move = 0x004,
            no_collapse = 0x008,
            no_bring_to_front_on_focus = 0x010,
            no_navigation_focus = 0x020,
            no_decoration = 0x040,
            no_background = 0x080,
            no_scrollbar = 0x100,
            no_docking = 0x200,
            menubar = 0x400,
        };

        struct window_info
        {
            string_view name;
            variant<default_position_tag_t, viewport_origin_tag_t, math::vec2<float>> position;
            variant<default_size_tag_t, fullscreen_tag_t, math::vec2<float>> size;
            enum_mask<window_flags> flags;
        };

        using dockspace_identifier = uint32_t;

        struct dockspace_configure_node
        {
            unique_ptr<dockspace_configure_node> top;
            unique_ptr<dockspace_configure_node> bottom;
            unique_ptr<dockspace_configure_node> left;
            unique_ptr<dockspace_configure_node> right;

            float size;
            vector<string_view> docked_windows;
        };

        struct dockspace_configure_info
        {
            dockspace_configure_node root;
            string_view name;
        };

        struct dockspace_layout
        {
            unique_ptr<dockspace_layout> top_node;
            unique_ptr<dockspace_layout> bottom_node;
            unique_ptr<dockspace_layout> left_node;
            unique_ptr<dockspace_layout> right_node;

            dockspace_identifier central_node = 0;
        };

        struct dockspace_info
        {
            dockspace_identifier root;
        };

        ui_context(rhi::window_surface* surface, rhi::device* device, rhi::image_format target_fmt,
                   uint32_t frames_in_flight) noexcept;
        ui_context(const ui_context&) = delete;
        ui_context(ui_context&&) noexcept = delete;
        ~ui_context();

        ui_context& operator=(const ui_context&) = delete;
        ui_context& operator=(ui_context&&) noexcept = delete;

        void begin_ui_commands();
        void finish_ui_commands();

        void render_ui_commands(rhi::typed_rhi_handle<rhi::rhi_handle_type::command_list> command_list,
                                rhi::work_queue& wq) noexcept;

        static bool begin_window(window_info info);
        static void end_window();
        static math::vec2<uint32_t> get_current_window_size() noexcept;
        static dockspace_identifier get_dockspace_id(string_view name) noexcept;

        static dockspace_layout configure_dockspace(dockspace_configure_info&& info);
        static void dockspace(dockspace_identifier id);

      private:
        struct impl;
        unique_ptr<impl> _impl = nullptr;

        void _init_window_backend();
        void _init_render_backend();
        void _setup_font_textures();
    };

    class ui_pipeline : public graphics::render_pipeline
    {
      public:
        explicit ui_pipeline(ui_context* ui_ctx);

        void initialize(graphics::renderer& parent, rhi::device& dev) override;
        render_result render(graphics::renderer& parent, rhi::device& dev, const render_state& rs) override;
        void destroy(graphics::renderer& parent, rhi::device& dev) override;

        void set_viewport(uint32_t width, uint32_t height) override
        {
            _width = width;
            _height = height;
        }

      private:
        ui_context* _ui_ctx;
        uint64_t _frame_number = 0;
        uint32_t _frame_in_flight = 0;

        uint32_t _width = 0;
        uint32_t _height = 0;
    };

} // namespace tempest::editor::ui

#endif // tempest_editor_ui_ui_hpp
