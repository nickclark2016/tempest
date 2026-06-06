#include <tempest/windows/entity_view_window.hpp>

#include <imgui.h>

#include <tempest/bit.hpp>

namespace tempest::editor
{
    namespace
    {
        struct input_text_callback_user_data
        {
            string* str;
            ImGuiInputTextCallback chained_callback;
            void* chained_callback_user_data;
        };

        auto input_text_callback(ImGuiInputTextCallbackData* data) -> int
        {
            auto user_data = static_cast<input_text_callback_user_data*>(data->UserData);
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                // Resize string callback
                // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set
                // them back to what we want.
                auto str = user_data->str;
                IM_ASSERT(data->Buf == str->c_str());
                str->resize(data->BufTextLen);
                data->Buf = str->data();
            }
            else if (user_data->chained_callback)
            {
                // Forward to user callback, if any
                data->UserData = user_data->chained_callback_user_data;
                return user_data->chained_callback(data);
            }
            return 0;
        }
    } // namespace

    auto entity_view_window::desired_initial_dock() const -> editor_window::dock_location
    {
        return dock_location::right;
    }

    auto entity_view_window::window_name() const -> string_view
    {
        return "Entity Editor";
    }

    auto entity_view_window::draw() -> void
    {
        const auto name = window_name();
        if (ImGui::Begin(name.data(), &open))
        {
            if (target == ecs::null)
            {
                ImGui::Text("No Entity Selected");
                ImGui::End();
                return;
            }

            auto selected_entity_name =
                entity_registry->name(target).transform([](const auto& view) { return string{view}; }).value_or("");
            auto cb_user_data = input_text_callback_user_data{
                .str = &selected_entity_name,
                .chained_callback = nullptr,
                .chained_callback_user_data = nullptr,
            };

            const auto id = bit_cast<void*>(target);
            ImGui::PushID(id);
            const auto name_modified = ImGui::InputTextWithHint(
                "Name", "Unnamed", selected_entity_name.data(), selected_entity_name.capacity() + 1,
                ImGuiInputTextFlags_CallbackResize, input_text_callback, &cb_user_data);
            ImGui::PopID();

            if (name_modified)
            {
                entity_registry->name(target, selected_entity_name);
            }

            for (auto&& provider : providers)
            {
                ImGui::Separator();
                provider->draw(entity_registry, target);
            }
        }
        ImGui::End();
    }
} // namespace tempest::editor