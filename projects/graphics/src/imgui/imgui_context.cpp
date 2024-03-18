#include <tempest/imgui_context.hpp>

#include "../windowing/glfw_window.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

namespace tempest::graphics
{
    static bool global_init = false;

    void imgui_context::initialize_for_window(iwindow& win)
    {
        if (!global_init)
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::StyleColorsDark();

            global_init = true;
        }

        if (glfw::window* w = dynamic_cast<glfw::window*>(&win))
        {
            ImGui_ImplGlfw_InitForVulkan(w->raw(), true);
        }
    }

    void imgui_context::create_frame(std::function<void()> contents)
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        contents();

        ImGui::EndFrame();
    }

    void imgui_context::create_window(std::string_view name, std::function<void()> contents)
    {
        ImGui::Begin(name.data());
        contents();
        ImGui::End();
    }

    void imgui_context::create_table(std::string_view name, int cols, std::function<void()> contents)
    {
        ImGui::BeginTable(name.data(), cols);
        contents();
        ImGui::EndTable();
    }

    void imgui_context::next_column()
    {
        ImGui::TableNextColumn();
    }

    void imgui_context::next_row()
    {
        ImGui::TableNextRow();
    }

    void imgui_context::create_tree_node(std::string_view name, std::function<void()> contents)
    {
        if (ImGui::TreeNode(name.data()))
        {
            contents();
            ImGui::TreePop();
        }
    }

    void imgui_context::create_header(std::string_view name, std::function<void()> contents)
    {
        if (ImGui::CollapsingHeader(name.data()))
        {
            contents();
        }
    }

    void imgui_context::label(std::string_view contents)
    {
        ImGui::Text("%s", contents.data());
    }

    float imgui_context::float_slider(std::string_view name, float min, float max, float current_value)
    {
        float value = current_value;
        ImGui::SliderFloat(name.data(), &value, min, max);
        return value;
    }

    math::vec2<float> imgui_context::float2_slider(std::string_view name, float min, float max, math::vec2<float> current_value)
    {
        ImGui::SliderFloat2(name.data(), current_value.data, min, max);
        return current_value;
    }

    int imgui_context::int_slider(std::string_view name, int min, int max, int current_value)
    {
        int value = current_value;
        ImGui::SliderInt(name.data(), &value, min, max, "%d");
        return value;
    }

    bool imgui_context::checkbox(std::string_view label, bool current_value)
    {
        ImGui::Checkbox(label.data(), &current_value);
        return current_value;
    }

    bool imgui_context::button(std::string label)
    {
        return ImGui::Button(label.data());
    }

    void imgui_context::shutdown()
    {
        ImGui_ImplGlfw_Shutdown();
    }
}