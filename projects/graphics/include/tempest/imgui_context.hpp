#ifndef tempest_graphics_imgui_context
#define tempest_graphics_imgui_context

#include "window.hpp"

#include <tempest/functional.hpp>
#include <tempest/span.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec4.hpp>

#include <string_view>

namespace tempest::graphics
{
    class imgui_context
    {
      public:
        static void initialize_for_window(iwindow& win);
        static void shutdown();

        static void create_frame(function<void()> contents);
        static void create_window(std::string_view name, function<void()> contents);

        static void create_table(std::string_view name, int cols, function<void()> contents);
        static void next_column();
        static void next_row();

        static void create_header(std::string_view name, function<void()> contents);

        static bool create_tree_node(std::string_view name, function<void()> contents, bool selected = false);
        static bool create_tree_node_leaf(std::string_view name, function<void()> contents,
                                          bool selected = false);
        static bool begin_tree_node(std::string_view name);
        static void end_tree_node();

        static void push_color_text(float red, float green, float blue, float alpha);
        static void push_color_frame_background(float red, float green, float blue, float alpha);
        static void pop_color();

        static void label(std::string_view contents);
        static float float_slider(std::string_view name, float min, float max, float current_value);
        static math::vec2<float> float2_slider(std::string_view name, float min, float max,
                                               math::vec2<float> current_value);
        static int int_slider(std::string_view name, int min, int max, int current_value);
        static bool checkbox(std::string_view label, bool current_value);
        static bool button(std::string label);
        static int combo_box(std::string_view label, int current_item, span<std::string_view> items);
        static float input_float(std::string_view label, float current_value);

        static void start_frame();
        static void end_frame();

      private:
        static constexpr math::vec4<float> _tree_node_selected_color = { 0.3f, 0.3f, 0.3f, 1.0f };
    };
} // namespace tempest::graphics

#endif // tempest_graphics_imgui_context