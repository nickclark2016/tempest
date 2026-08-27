#include <tempest/windows/entity_view_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/bit.hpp>
#include <tempest/iterator.hpp>
#include <tempest/string.hpp>
#include <tempest/ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <format>

namespace tempest::editor
{
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

            auto name_str = string{};
            auto selected_name_opt = entity_registry->name(target);
            if (selected_name_opt.has_value())
            {
                const auto& n = selected_name_opt.value();
                std::format_to(tempest::back_inserter(name_str), "{:.{}}", n.data(), n.size());
            }

            const auto id = bit_cast<void*>(target);
            ImGui::PushID(id);
            const auto name_modified = ui::input_text_with_hint("Name", "Unnamed", name_str);
            ImGui::PopID();

            if (name_modified)
            {
                entity_registry->name(target, name_str);
            }

            for (auto&& provider : providers)
            {
                provider->draw(entity_registry, target);
            }

            if (ui::centered_button("Add Component"))
            {
                ImGui::OpenPopup("AddComponentPopup");

                component_search.filtered_indices.clear();
                component_search.selected = -1;
                component_search.sorted_providers.clear();

                for (auto& provider : providers)
                {
                    component_search.sorted_providers.push_back(provider.get());
                }

                std::sort(component_search.sorted_providers.begin(), component_search.sorted_providers.end(),
                          [](auto* lhs, auto* rhs) {
                              const auto lhs_name = lhs->name();
                              const auto rhs_name = rhs->name();
                              return lhs_name < rhs_name;
                          });
            }

            if (ImGui::BeginPopup("AddComponentPopup", ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking))
            {
                auto filter_changed = component_search.text_filter.Draw("Search");
                if (component_search.text_filter.IsActive())
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear"))
                    {
                        component_search.text_filter.Clear();
                        filter_changed = true;
                        component_search.focus_search = true;
                    }
                }

                if (filter_changed || component_search.filtered_indices.empty())
                {
                    component_search.filtered_indices.clear();

                    for (auto idx = 0u; idx < component_search.sorted_providers.size(); ++idx)
                    {
                        auto& provider = component_search.sorted_providers[idx];
                        const auto provider_name = provider->name();
                        if (!component_search.text_filter.PassFilter(provider_name.c_str()))
                        {
                            continue;
                        }

                        component_search.filtered_indices.push_back(static_cast<int>(idx));
                    }

                    if (component_search.selected >= static_cast<int>(component_search.sorted_providers.size()))
                    {
                        component_search.selected = -1;
                    }
                }

                if (ImGui::BeginChild("Components", ImVec2(0, 400)))
                {
                    auto clipper = ImGuiListClipper{};
                    clipper.Begin((int)component_search.filtered_indices.size());

                    while (clipper.Step())
                    {
                        for (auto row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                        {
                            auto provider_index = component_search.filtered_indices[static_cast<size_t>(row)];
                            const auto& provider = component_search.sorted_providers[provider_index];
                            const auto selected = provider_index == component_search.selected;

                            if (ImGui::Selectable(provider->name().c_str(), selected))
                            {
                                component_search.selected = provider_index;
                            }
                        }
                    }

                    ImGui::BeginDisabled(component_search.selected < 0);

                    if (ui::centered_button("Assign"))
                    {
                        auto& provider = component_search.sorted_providers[component_search.selected];
                        provider->create_default(entity_registry, target);
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndDisabled();
                }

                ImGui::EndChild();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
} // namespace tempest::editor