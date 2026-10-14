#include <tempest/windows/log_view_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/chrono.hpp>
#include <tempest/format.hpp>
#include <tempest/int.hpp>
#include <tempest/string.hpp>

#include <imgui.h>

namespace tempest::editor
{
    namespace
    {
        [[nodiscard]] auto trim_source_path(const char* file_name_ptr) noexcept -> string_view
        {
            if (file_name_ptr == nullptr)
            {
                return {};
            }
            const auto file_name = string_view{file_name_ptr};
            for (size_t i = 0; i < file_name.size(); ++i)
            {
                if (file_name[i] != '.' && file_name[i] != '/' && file_name[i] != '\\')
                {
                    return substr(file_name, i, file_name.size() - i);
                }
            }
            return file_name;
        }

        [[nodiscard]] auto get_level_color(log_level level) noexcept -> ImVec4
        {
            switch (level)
            {
            case log_level::trace:
                return ImVec4(0.6F, 0.6F, 0.6F, 1.0F);
            case log_level::debug:
                return ImVec4(0.3F, 0.8F, 0.9F, 1.0F);
            case log_level::info:
                return ImVec4(1.0F, 1.0F, 1.0F, 1.0F);
            case log_level::warn:
                return ImVec4(1.0F, 0.8F, 0.2F, 1.0F);
            case log_level::error:
                return ImVec4(1.0F, 0.3F, 0.3F, 1.0F);
            case log_level::critical:
            case log_level::fatal:
                return ImVec4(1.0F, 0.2F, 0.6F, 1.0F);
            default:
                return ImVec4(1.0F, 1.0F, 1.0F, 1.0F);
            }
        }

        [[nodiscard]] auto get_level_name(log_level level) noexcept -> string_view
        {
            switch (level)
            {
            case log_level::trace:
                return "Trace";
            case log_level::debug:
                return "Debug";
            case log_level::info:
                return "Info";
            case log_level::warn:
                return "Warn";
            case log_level::error:
                return "Error";
            case log_level::critical:
                return "Critical";
            case log_level::fatal:
                return "Fatal";
            default:
                return "Unknown";
            }
        }
    } // namespace

    log_view_window::log_view_window(log_store& target_store) : store{&target_store}
    {
    }

    auto log_view_window::draw() -> void
    {
        if (!open || store == nullptr || ImGui::GetCurrentContext() == nullptr)
        {
            return;
        }

        if (ImGui::Begin(window_name().data(), &open))
        {
            // Toolbar
            if (ImGui::Button("Clear"))
            {
                store->clear();
            }

            ImGui::SameLine();

            // Level filter toggle buttons
            const auto& filter_state = store->filter();

            struct level_button_def
            {
                log_level level;
                const char* name;
                size_t count;
                ImVec4 color;
            };

            const level_button_def level_buttons[] = {
                {log_level::trace, "Trace", filter_state.trace_count, ImVec4(0.6F, 0.6F, 0.6F, 1.0F)},
                {log_level::debug, "Debug", filter_state.debug_count, ImVec4(0.3F, 0.8F, 0.9F, 1.0F)},
                {log_level::info, "Info", filter_state.info_count, ImVec4(1.0F, 1.0F, 1.0F, 1.0F)},
                {log_level::warn, "Warn", filter_state.warn_count, ImVec4(1.0F, 0.8F, 0.2F, 1.0F)},
                {log_level::error, "Error", filter_state.error_count, ImVec4(1.0F, 0.3F, 0.3F, 1.0F)},
                {log_level::critical, "Critical", filter_state.critical_count, ImVec4(1.0F, 0.2F, 0.6F, 1.0F)},
                {log_level::fatal, "Fatal", filter_state.fatal_count, ImVec4(1.0F, 0.2F, 0.6F, 1.0F)},
            };

            for (const auto& btn : level_buttons)
            {
                const auto is_enabled = filter_state.is_level_enabled(btn.level);
                auto label = tempest::format("{} ({})", btn.name, btn.count);

                if (is_enabled)
                {
                    const auto bg = ImVec4(btn.color.x * 0.35F, btn.color.y * 0.35F, btn.color.z * 0.35F, 0.85F);
                    const auto bg_hover = ImVec4(btn.color.x * 0.50F, btn.color.y * 0.50F, btn.color.z * 0.50F, 1.0F);
                    const auto bg_active = ImVec4(btn.color.x * 0.65F, btn.color.y * 0.65F, btn.color.z * 0.65F, 1.0F);

                    ImGui::PushStyleColor(ImGuiCol_Button, bg);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_hover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_active);
                    ImGui::PushStyleColor(ImGuiCol_Text, btn.color);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15F, 0.15F, 0.15F, 0.4F));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25F, 0.25F, 0.25F, 0.6F));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35F, 0.35F, 0.35F, 0.8F));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5F, 0.5F, 0.5F, 0.5F));
                }

                if (ImGui::Button(label.c_str()))
                {
                    store->set_filter_level(btn.level, !is_enabled);
                }

                ImGui::PopStyleColor(4);
                ImGui::SameLine();
            }

            // Search input
            ImGui::SetNextItemWidth(200.0F);
            if (ImGui::InputTextWithHint("##LogSearch", "Search...", search_buffer, sizeof(search_buffer)))
            {
                store->set_filter_search(search_buffer);
            }

            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &auto_scroll);

            // Virtualized Table
            const auto table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                     ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY |
                                     ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                     ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit;

            if (ImGui::BeginTable("##LogViewTable", 5, table_flags))
            {
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 90.0F);
                ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 60.0F);
                ImGui::TableSetupColumn("Thread", ImGuiTableColumnFlags_WidthFixed, 60.0F);
                ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 160.0F);
                ImGui::TableHeadersRow();

                const auto& matches = store->filter().matched_sequences;
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(matches.size()));

                while (clipper.Step())
                {
                    for (auto row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                    {
                        const auto seq = matches[static_cast<size_t>(row)];
                        const auto* entry = store->get_entry(seq);
                        if (entry == nullptr)
                        {
                            continue;
                        }

                        ImGui::TableNextRow();
                        ImGui::PushID(static_cast<int>(seq));

                        const auto tp = chrono::system_clock::time_point(
                            chrono::nanoseconds(static_cast<int64_t>(entry->timestamp_ns())));
                        const auto local_cal = chrono::to_local_time(tp);
                        const auto time_str = tempest::format("{:02d}:{:02d}:{:02d}.{:03d}", local_cal.hour,
                                                              local_cal.minute, local_cal.second,
                                                              local_cal.millisecond);
                        const auto level_color = get_level_color(entry->level());
                        const auto level_name = get_level_name(entry->level());

                        // Column 0: Time with row-spanning Selectable for hover and context menu
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Selectable(time_str.c_str(), false,
                                          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                        const auto is_row_hovered = ImGui::IsItemHovered();

                        if (ImGui::BeginPopupContextItem("##EntryContext"))
                        {
                            if (ImGui::MenuItem("Copy Message"))
                            {
                                const auto msg = entry->message();
                                auto msg_str = tempest::string{msg.data(), msg.size()};
                                ImGui::SetClipboardText(msg_str.c_str());
                            }
                            if (ImGui::MenuItem("Copy Full Row"))
                            {
                                const auto file_name = trim_source_path(entry->source().file_name());
                                auto row_str = tempest::format("[{}] [{}] [0x{:04X}] {} ({}:{})",
                                                               time_str, level_name, entry->thread_id(),
                                                               entry->message(), file_name, entry->source().line());
                                ImGui::SetClipboardText(row_str.c_str());
                            }
                            if (ImGui::MenuItem("Copy Source Location"))
                            {
                                const auto file_name = trim_source_path(entry->source().file_name());
                                auto loc_str = tempest::format("{}:{}", file_name, entry->source().line());
                                ImGui::SetClipboardText(loc_str.c_str());
                            }
                            ImGui::EndPopup();
                        }

                        if (is_row_hovered)
                        {
                            ImGui::BeginTooltip();
                            const auto file_name = trim_source_path(entry->source().file_name());
                            ImGui::TextColored(level_color, "[%.*s]", static_cast<int>(level_name.size()), level_name.data());
                            ImGui::SameLine();
                            ImGui::TextUnformatted(time_str.c_str());
                            ImGui::SameLine();
                            ImGui::TextDisabled("Thread: 0x%04X", entry->thread_id());
                            ImGui::Separator();
                            const auto msg = entry->message();
                            ImGui::TextUnformatted(msg.data(), msg.data() + msg.size());
                            ImGui::Separator();
                            const auto src_loc_text =
                                tempest::format("Source: {}:{}", file_name, entry->source().line());
                            ImGui::TextDisabled("%s", src_loc_text.c_str());
                            ImGui::EndTooltip();
                        }

                        // Column 1: Level
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(level_color, "%.*s", static_cast<int>(level_name.size()), level_name.data());

                        // Column 2: Thread
                        ImGui::TableSetColumnIndex(2);
                        const auto thread_str = tempest::format("0x{:04X}", entry->thread_id());
                        ImGui::TextUnformatted(thread_str.c_str());

                        // Column 3: Message
                        ImGui::TableSetColumnIndex(3);
                        const auto msg = entry->message();
                        ImGui::TextUnformatted(msg.data(), msg.data() + msg.size());

                        // Column 4: Source
                        ImGui::TableSetColumnIndex(4);
                        const auto file_name = trim_source_path(entry->source().file_name());
                        const auto source_str = tempest::format("{}:{}", file_name, entry->source().line());
                        ImGui::TextUnformatted(source_str.c_str());

                        ImGui::PopID();
                    }
                }

                // Auto-scroll handling
                if (request_scroll_to_bottom || (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                {
                    ImGui::SetScrollHereY(1.0F);
                    request_scroll_to_bottom = false;
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
} // namespace tempest::editor
