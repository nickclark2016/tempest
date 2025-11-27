#ifndef tempest_editor_ui_ui_hpp
#define tempest_editor_ui_ui_hpp

#include <tempest/frame_graph.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/slot_map.hpp>
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

        enum class tree_node_flags
        {
            none = 0x000,
            selected = 0x001,
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

        void render_ui_commands(graphics::graphics_task_execution_context& exec_ctx) noexcept;

        static bool begin_window(window_info info);
        static void end_window();
        static math::vec2<uint32_t> get_available_content_region() noexcept;
        static dockspace_identifier get_dockspace_id(string_view name) noexcept;

        static dockspace_layout configure_dockspace(dockspace_configure_info&& info);
        static void dockspace(dockspace_identifier id);

        /// <summary>
        /// Begins a menu bar context and returns if the context is active. Only one menu bar context can be active at a
        /// time. A context is ended by calling <see cref="end_menu_bar"/>.
        /// </summary>
        /// <returns>True if context is active, else false</returns>
        static bool begin_menu_bar();

        /// <summary>
        /// Ends the active menu bar context. If no context is active, this function must not be called.
        /// </summary>
        static void end_menu_bar();

        /// <summary>
        /// Begins a new menu context with the given name. If the menu is not enabled, it will be added to the menu bar,
        /// but it will be disabled. A context must be surrounded by a menu bar context (see <see
        /// cref="begin_menu_bar"/>). A menu context is ended by calling <see cref="end_window"/>. A menu context can
        /// exist inside another menu context, allowing for nested menus.
        /// </summary>
        /// <param name="name">Name of the menu</param>
        /// <param name="enabled">True if the menu should be enabled</param>
        /// <returns>True if the menu context is active, else false</returns>
        static bool begin_menu(string_view name, bool enabled = true);

        /// <summary>
        /// Ends the active menu context. If no context is active, this function must not be called.
        /// </summary>
        static void end_menu();

        /// <summary>
        /// Displays a menu item with the given name. If the item is enabled, it will be clickable. A menu item must be
        /// inside a menu context (see <see cref="begin_menu"/>).
        /// </summary>
        /// <param name="name">Display name of the menu item</param>
        /// <param name="enabled">True if the menu item should be enabled</param>
        /// <returns>True if the menu item was selected</returns>
        static bool menu_item(string_view name, bool enabled = true);

        /// <summary>
        /// Displays the provided text. The string must be null terminated and can contain formatting arguments similar
        /// to printf.
        /// </summary>
        /// <param name="content">Text to display</param>
        /// <param name="...">Optional arguments for formatting the text</param>
        static void text(string_view content, ...);

        /// <summary>
        /// Displays the provided text. The string must be null terminated.
        /// </summary>
        /// <param name="selected">Set to true if the text is selected, else false</param>
        /// <param name="content">Text to display</param>
        /// <returns>True if text is selected, else false</returns>
        static bool selectable_text(bool selected, string_view content);

        /// <summary>
        /// Displays an image with the given width and height. The image must be created with the sampled usage flag.
        /// </summary>
        /// <param name="img">Image to display. At the time of UI pipeline execution, this image must be in a
        /// rhi::image_layout::shader_read_only layout.</param>
        /// <param name="width">Display width</param>
        /// <param name="height">Display height</param>
        static void image(rhi::typed_rhi_handle<rhi::rhi_handle_type::image> img, uint32_t width, uint32_t height);

        static bool tree_node(const void* id, enum_mask<tree_node_flags> flags, string_view label, ...);
        static bool tree_leaf(const void* id, enum_mask<tree_node_flags> flags, string_view label, ...);
        static void tree_pop();

        /// <summary>
        /// Pushes a new window padding value. This value is applied to all four sides of the window. Style values are
        /// pushed like a stack, so pushes must be matched with a pop (see <see cref="pop_style"/>).
        /// </summary>
        /// <param name="px">Padding along the x axis</param>
        /// <param name="py">Padding along the y axis</param>
        static void push_window_padding(float px, float py);

        /// <summary>
        /// Pops a previously pushed style variable.
        /// </summary>
        static void pop_style();

        static bool button(string_view label);

        static bool is_clicked();
        static bool is_hovered();
        static bool is_double_clicked(core::mouse_button button);

        static void no_line_break();

        static void horizontal_separator();

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
        using viewport_pipeline_handle = slot_map<unique_ptr<graphics::render_pipeline>>::key_type;

        explicit ui_pipeline(ui_context* ui_ctx);

        void initialize(graphics::renderer& parent, rhi::device& dev) override;
        render_result render(graphics::renderer& parent, rhi::device& dev, const render_state& rs) override;
        void destroy(graphics::renderer& parent, rhi::device& dev) override;

        void set_viewport(uint32_t width, uint32_t height) override;
        void set_viewport(viewport_pipeline_handle handle, uint32_t width, uint32_t height) noexcept;

        viewport_pipeline_handle register_viewport_pipeline(unique_ptr<graphics::render_pipeline> pipeline) noexcept;
        bool unregister_viewport_pipeline(viewport_pipeline_handle handle) noexcept;
        graphics::render_pipeline* get_viewport_pipeline(viewport_pipeline_handle handle) const noexcept;

        void upload_objects_sync(rhi::device& dev, span<const ecs::archetype_entity> entities,
                                 const core::mesh_registry& meshes, const core::texture_registry& textures,
                                 const core::material_registry& materials) override;

      private:
        ui_context* _ui_ctx;
        uint64_t _frame_number = 0;
        uint32_t _frame_in_flight = 0;

        uint32_t _width = 0;
        uint32_t _height = 0;

        graphics::renderer* _renderer = nullptr;
        rhi::device* _device = nullptr;

        rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore> _timeline_sem =
            rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore>::null_handle;
        uint64_t _timeline_value = 0;

        struct viewport_pipeline_payload
        {
            rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore> timeline_sem;
            uint64_t timeline_value;

            unique_ptr<graphics::render_pipeline> pipeline;
        };

        slot_map<viewport_pipeline_payload> _child_pipelines;
    };

    graphics::graph_resource_handle<rhi::rhi_handle_type::image> create_ui_pass(
        string name, ui_context& ui_ctx, graphics::graph_builder& builder, rhi::device& dev,
        graphics::graph_resource_handle<rhi::rhi_handle_type::image> render_target);
} // namespace tempest::editor::ui

#endif // tempest_editor_ui_ui_hpp
