#include <tempest/imgui_context.hpp>

#include "../windowing/glfw_window.hpp"

#include <tempest/vector.hpp>

#include <backends/imgui_impl_glfw.h>
#include <imgui.h>

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

    void imgui_context::create_frame(function<void()> contents)
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        contents();

        ImGui::EndFrame();
    }

    void imgui_context::create_window(string_view name, function<void()> contents)
    {
        ImGui::Begin(name.data());
        contents();
        ImGui::End();
    }

    void imgui_context::create_table(string_view name, int cols, function<void()> contents)
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

    bool imgui_context::create_tree_node(string_view name, function<void()> contents, bool selected)
    {
        if (selected)
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImU32 col = ImColor(ImVec4(_tree_node_selected_color.x, _tree_node_selected_color.y,
                                       _tree_node_selected_color.z, _tree_node_selected_color.w));
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, ImVec2(pos.x + ImGui::GetContentRegionMax().x, pos.y + ImGui::GetTextLineHeight()), col);
        }

        if (ImGui::TreeNodeEx(name.data(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick))
        {
            bool is_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
            contents();
            ImGui::TreePop();
            return is_clicked;
        }
        else
        {
            return ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
        }
    }

    bool imgui_context::create_tree_node_leaf(string_view name, function<void()> contents, bool selected)
    {
        if (selected)
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImU32 col = ImColor(ImVec4(_tree_node_selected_color.x, _tree_node_selected_color.y,
                                       _tree_node_selected_color.z, _tree_node_selected_color.w));
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, ImVec2(pos.x + ImGui::GetContentRegionMax().x, pos.y + ImGui::GetTextLineHeight()), col);
        }

        if (ImGui::TreeNodeEx(name.data(), ImGuiTreeNodeFlags_Leaf))
        {
            bool is_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
            contents();
            ImGui::TreePop();
            return is_clicked;
        }
        else
        {
            return ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
        }
    }

    bool imgui_context::begin_tree_node(string_view name)
    {
        return ImGui::TreeNode(name.data());
    }

    void imgui_context::end_tree_node()
    {
        ImGui::TreePop();
    }

    void imgui_context::push_color_text(float red, float green, float blue, float alpha)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(red, green, blue, alpha));
    }

    void imgui_context::push_color_frame_background(float red, float green, float blue, float alpha)
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(red, green, blue, alpha));
    }

    void imgui_context::pop_color()
    {
        ImGui::PopStyleColor();
    }

    void imgui_context::create_header(string_view name, function<void()> contents)
    {
        if (ImGui::CollapsingHeader(name.data()))
        {
            contents();
        }
    }

    void imgui_context::label(string_view contents)
    {
        ImGui::Text("%s", contents.data());
    }

    void imgui_context::label(uint32_t contents)
    {
        ImGui::Text("%u", contents);
    }

    float imgui_context::float_slider(string_view name, float min, float max, float current_value)
    {
        float value = current_value;
        ImGui::SliderFloat(name.data(), &value, min, max);
        return value;
    }

    math::vec2<float> imgui_context::float2_slider(string_view name, float min, float max,
                                                   math::vec2<float> current_value)
    {
        ImGui::SliderFloat2(name.data(), current_value.data, min, max);
        return current_value;
    }

    int imgui_context::int_slider(string_view name, int min, int max, int current_value)
    {
        int value = current_value;
        ImGui::SliderInt(name.data(), &value, min, max, "%d");
        return value;
    }

    bool imgui_context::checkbox(string_view label, bool current_value)
    {
        ImGui::Checkbox(label.data(), &current_value);
        return current_value;
    }

    bool imgui_context::button(string label)
    {
        return ImGui::Button(label.data());
    }

    int imgui_context::combo_box(string_view label, int current_item, span<string_view> items)
    {
        vector<const char*> item_ptrs;
        for (auto& item : items)
        {
            item_ptrs.push_back(item.data());
        }

        ImGui::Combo(label.data(), &current_item, item_ptrs.data(), static_cast<int>(item_ptrs.size()));

        return current_item;
    }

    float imgui_context::input_float(string_view label, float current_value)
    {
        ImGui::InputFloat(label.data(), &current_value);
        return current_value;
    }

    math::vec3<float> imgui_context::input_color(string_view label, math::vec3<float> current_value, bool enabled)
    {
        float color[3] = {current_value.x, current_value.y, current_value.z};
        ImGui::ColorEdit3(label.data(), color,
                          enabled ? ImGuiColorEditFlags_NoInputs
                                  : (ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker));
        return {color[0], color[1], color[2]};
    }

    math::vec4<float> imgui_context::input_color(string_view label, math::vec4<float> current_value, bool enabled)
    {
        float color[4] = {current_value.x, current_value.y, current_value.z, current_value.w};
        ImGui::ColorEdit4(label.data(), color,
                          enabled ? ImGuiColorEditFlags_NoInputs
                                  : (ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker));
        return {color[0], color[1], color[2], color[3]};
    }

    void imgui_context::start_frame()
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void imgui_context::end_frame()
    {
        ImGui::EndFrame();
    }

    void imgui_context::shutdown()
    {
        ImGui_ImplGlfw_Shutdown();
    }
} // namespace tempest::graphics