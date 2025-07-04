#ifndef tempest_editor_ui_ui_hpp
#define tempest_editor_ui_ui_hpp

#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/rhi.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vec2.hpp>

namespace tempest::editor::ui
{
    class ui_context
    {
      public:
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
            optional<math::vec2<float>> position;
            optional<math::vec2<float>> size;
            enum_mask<window_flags> flags;
        };

        using dockspace_identifier = uint32_t;

        struct dockspace_configure_info
        {
            string_view name;

            optional<float> left_width;
            optional<float> right_width;
            optional<float> top_height;
            optional<float> bottom_height;
            optional<dockspace_identifier> parent;

            optional<string_view> left_window_name;
            optional<string_view> right_window_name;
            optional<string_view> top_window_name;
            optional<string_view> bottom_window_name;
            optional<string_view> center_window_name;
        };

        struct dockspace_identifiers
        {
            optional<dockspace_identifier> left_id;
            optional<dockspace_identifier> right_id;
            optional<dockspace_identifier> top_id;
            optional<dockspace_identifier> bottom_id;
            dockspace_identifier center_id;
            dockspace_identifier root_id;
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

        static dockspace_identifiers configure_dockspace(dockspace_configure_info info);
        static void dockspace(dockspace_info info);

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

        void set_size(uint32_t width, uint32_t height) noexcept
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
