#include <tempest/imgui_context.hpp>

#include "../windowing/glfw_window.hpp"

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

    bool imgui_context::create_tree_node(std::string_view name, std::function<void()> contents, bool selected)
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

    bool imgui_context::create_tree_node_leaf(std::string_view name, std::function<void()> contents, bool selected)
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

    bool imgui_context::begin_tree_node(std::string_view name)
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

    math::vec2<float> imgui_context::float2_slider(std::string_view name, float min, float max,
                                                   math::vec2<float> current_value)
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

    int imgui_context::combo_box(std::string_view label, int current_item, std::span<std::string_view> items)
    {
        std::vector<const char*> item_ptrs;
        for (auto& item : items)
        {
            item_ptrs.push_back(item.data());
        }

        ImGui::Combo(label.data(), &current_item, item_ptrs.data(), item_ptrs.size());

        return current_item;
    }

    float imgui_context::input_float(std::string_view label, float current_value)
    {
        ImGui::InputFloat(label.data(), &current_value);
        return current_value;
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