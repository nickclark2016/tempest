#ifndef tempest_graphics_imgui_context
#define tempest_graphics_imgui_context

#include "window.hpp"

#include <functional>

namespace tempest::graphics
{
    class imgui_context
    {
      public:
        static void initialize_for_window(iwindow& win);
        static void shutdown();
      
        static void create_frame(std::function<void()> contents);
        static void create_window(std::string_view name, std::function<void()> contents);

        static void create_table(std::string_view name, int cols, std::function<void()> contents);
        static void next_column();
        static void next_row();

        static void create_tree_node(std::string_view name, std::function<void()> contents);

        static void label(std::string_view contents);
        static float float_slider(std::string_view name, float min, float max, float current_value);
        static int int_slider(std::string_view name, int min, int max, int current_value);
    };
}

#endif // tempest_graphics_imgui_context