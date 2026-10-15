#include <tempest/property_grid.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/format.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/iterator.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/quat.hpp>
#include <tempest/string.hpp>
#include <tempest/utility.hpp>
#include <tempest/variant.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

#include <imgui.h>

namespace tempest::editor
{
    namespace
    {
        auto get_bound_float(const optional<ecs::ui_bound_value>& bound, float default_value) -> float
        {
            if (!bound.has_value())
            {
                return default_value;
            }
            const auto& val = *bound;
            if (holds_alternative<double>(val))
            {
                return static_cast<float>(get<double>(val));
            }
            if (holds_alternative<int64_t>(val))
            {
                return static_cast<float>(get<int64_t>(val));
            }
            if (holds_alternative<uint64_t>(val))
            {
                return static_cast<float>(get<uint64_t>(val));
            }
            return default_value;
        }

        auto get_bound_int(const optional<ecs::ui_bound_value>& bound, int default_value) -> int
        {
            if (!bound.has_value())
            {
                return default_value;
            }
            const auto& val = *bound;
            if (holds_alternative<int64_t>(val))
            {
                return static_cast<int>(get<int64_t>(val));
            }
            if (holds_alternative<uint64_t>(val))
            {
                return static_cast<int>(get<uint64_t>(val));
            }
            if (holds_alternative<double>(val))
            {
                return static_cast<int>(get<double>(val));
            }
            return default_value;
        }
    } // namespace

    auto property_grid::draw_component(property_grid_context& ctx, const ecs::type_descriptor& desc,
                                       void* component_ptr) -> bool
    {
        if (component_ptr == nullptr)
        {
            return false;
        }

        auto any_changed = false;
        for (const auto& field : desc.fields)
        {
            if ((field.flags & ecs::field_flags::hidden) != ecs::field_flags::none)
            {
                continue;
            }

            if (field.is_visible != nullptr && !field.is_visible(component_ptr))
            {
                continue;
            }

            if (draw_field(ctx, desc.name, field, component_ptr))
            {
                any_changed = true;
            }
        }

        return any_changed;
    }

    auto property_grid::draw_field(property_grid_context& ctx, string_view component_name,
                                   const ecs::field_descriptor& field, void* component_ptr) -> bool
    {
        if (component_ptr == nullptr)
        {
            return false;
        }

        auto is_overridden = false;
        if (ctx.prefab_instance != nullptr)
        {
            for (const auto& ov : ctx.prefab_instance->overrides.field_overrides)
            {
                if (ov.target == ctx.prefab_target_path && ov.component_name == component_name &&
                    ov.field_name == field.name)
                {
                    is_overridden = true;
                    break;
                }
            }
        }

        if (is_overridden)
        {
            const auto cursor_pos = ImGui::GetCursorScreenPos();
            const auto line_height = ImGui::GetTextLineHeightWithSpacing();
            auto* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(ImVec2(cursor_pos.x - 4.0F, cursor_pos.y),
                                     ImVec2(cursor_pos.x - 1.0F, cursor_pos.y + line_height),
                                     IM_COL32(30, 144, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35F, 0.75F, 1.0F, 1.0F));
        }

        const auto display_name = field.hints.display_name.empty() ? field.name : field.hints.display_name;
        char label_buf[256]{};
        format_to_buffer(label_buf, "{}##{}_{}", display_name, component_name, field.name);

        const auto read_bytes = [&](void* out_ptr, size_t size) {
            if (field.get != nullptr)
            {
                field.get(component_ptr, out_ptr);
            }
            else if (field.offset != ecs::field_descriptor::no_offset)
            {
                tempest::memcpy(out_ptr, static_cast<const byte*>(component_ptr) + field.offset, size);
            }
        };

        auto field_mutated = false;
        const auto dispatch_change = [&](const void* new_data, size_t size) {
            field_mutated = true;
            auto new_bytes = vector<byte>(size);
            tempest::memcpy(new_bytes.data(), new_data, size);

            if (ctx.history != nullptr && ctx.cmd_ctx != nullptr)
            {
                ctx.history->execute(
                    make_unique<set_field_command>(ctx.address, component_name, field.name, tempest::move(new_bytes)),
                    *ctx.cmd_ctx);
            }
            else
            {
                if (field.set != nullptr)
                {
                    field.set(component_ptr, new_data);
                }
                else if (field.offset != ecs::field_descriptor::no_offset)
                {
                    tempest::memcpy(static_cast<byte*>(component_ptr) + field.offset, new_data, size);
                }
            }
        };

        const auto is_read_only = (field.flags & ecs::field_flags::read_only) != ecs::field_flags::none;
        ImGui::BeginDisabled(is_read_only);

        switch (field.kind)
        {
        case ecs::field_kind::boolean: {
            auto val = false;
            read_bytes(&val, sizeof(val));
            if (ImGui::Checkbox(label_buf, &val))
            {
                dispatch_change(&val, sizeof(val));
            }
            break;
        }

        case ecs::field_kind::int32: {
            auto val = int32_t{0};
            read_bytes(&val, sizeof(val));
            const auto min_val = get_bound_int(field.hints.min_value, 0);
            const auto max_val = get_bound_int(field.hints.max_value, 0);
            const auto* min_ptr = field.hints.min_value.has_value() ? &min_val : nullptr;
            const auto* max_ptr = field.hints.max_value.has_value() ? &max_val : nullptr;
            const auto speed = field.hints.drag_speed > 0.0F ? field.hints.drag_speed : 1.0F;

            const auto changed = ImGui::DragScalar(label_buf, ImGuiDataType_S32, &val, speed, min_ptr, max_ptr);
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                dispatch_change(&val, sizeof(val));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::uint32: {
            auto val = uint32_t{0};
            read_bytes(&val, sizeof(val));
            const auto min_val = static_cast<uint32_t>(get_bound_int(field.hints.min_value, 0));
            const auto max_val = static_cast<uint32_t>(get_bound_int(field.hints.max_value, 0));
            const auto* min_ptr = field.hints.min_value.has_value() ? &min_val : nullptr;
            const auto* max_ptr = field.hints.max_value.has_value() ? &max_val : nullptr;
            const auto speed = field.hints.drag_speed > 0.0F ? field.hints.drag_speed : 1.0F;

            const auto changed =
                ImGui::DragScalar(label_buf, ImGuiDataType_U32, &val, speed, min_ptr, max_ptr);
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                dispatch_change(&val, sizeof(val));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::int64: {
            auto val = int64_t{0};
            read_bytes(&val, sizeof(val));
            const auto min_val = static_cast<int64_t>(get_bound_int(field.hints.min_value, 0));
            const auto max_val = static_cast<int64_t>(get_bound_int(field.hints.max_value, 0));
            const auto* min_ptr = field.hints.min_value.has_value() ? &min_val : nullptr;
            const auto* max_ptr = field.hints.max_value.has_value() ? &max_val : nullptr;
            const auto speed = field.hints.drag_speed > 0.0F ? field.hints.drag_speed : 1.0F;

            const auto changed = ImGui::DragScalar(label_buf, ImGuiDataType_S64, &val, speed, min_ptr, max_ptr);
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                dispatch_change(&val, sizeof(val));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::uint64: {
            auto val = uint64_t{0};
            read_bytes(&val, sizeof(val));
            const auto min_val = static_cast<uint64_t>(get_bound_int(field.hints.min_value, 0));
            const auto max_val = static_cast<uint64_t>(get_bound_int(field.hints.max_value, 0));
            const auto* min_ptr = field.hints.min_value.has_value() ? &min_val : nullptr;
            const auto* max_ptr = field.hints.max_value.has_value() ? &max_val : nullptr;
            const auto speed = field.hints.drag_speed > 0.0F ? field.hints.drag_speed : 1.0F;

            const auto changed = ImGui::DragScalar(label_buf, ImGuiDataType_U64, &val, speed, min_ptr, max_ptr);
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                dispatch_change(&val, sizeof(val));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::float32: {
            auto val = 0.0F;
            read_bytes(&val, sizeof(val));
            const auto min_val = get_bound_float(field.hints.min_value, 0.0F);
            const auto max_val = get_bound_float(field.hints.max_value, 0.0F);
            const auto* min_ptr = field.hints.min_value.has_value() ? &min_val : nullptr;
            const auto* max_ptr = field.hints.max_value.has_value() ? &max_val : nullptr;
            const auto speed = field.hints.drag_speed > 0.0F ? field.hints.drag_speed : 0.1F;

            const auto changed = ImGui::DragScalar(label_buf, ImGuiDataType_Float, &val, speed, min_ptr, max_ptr, "%.3f");
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                dispatch_change(&val, sizeof(val));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::float64: {
            auto val = 0.0;
            read_bytes(&val, sizeof(val));
            const auto min_val = field.hints.min_value.has_value() ? static_cast<double>(get_bound_float(field.hints.min_value, 0.0F)) : 0.0;
            const auto max_val = field.hints.max_value.has_value() ? static_cast<double>(get_bound_float(field.hints.max_value, 0.0F)) : 0.0;
            const auto* min_ptr = field.hints.min_value.has_value() ? &min_val : nullptr;
            const auto* max_ptr = field.hints.max_value.has_value() ? &max_val : nullptr;
            const auto speed = field.hints.drag_speed > 0.0F ? field.hints.drag_speed : 0.1F;

            const auto changed = ImGui::DragScalar(label_buf, ImGuiDataType_Double, &val, speed, min_ptr, max_ptr, "%.4f");
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                dispatch_change(&val, sizeof(val));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::vec2: {
            auto val = math::vec2<float>{0.0F, 0.0F};
            read_bytes(&val, sizeof(val));
            float arr[2] = {val.x, val.y};
            const auto speed = field.hints.drag_speed > 0.0F ? field.hints.drag_speed : 0.1F;

            const auto changed = ImGui::DragFloat2(label_buf, arr, speed, 0.0F, 0.0F, "%.3f");
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                val = math::vec2<float>{arr[0], arr[1]};
                dispatch_change(&val, sizeof(val));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::vec3: {
            auto val = math::vec3<float>{0.0F, 0.0F, 0.0F};
            read_bytes(&val, sizeof(val));

            if (field.hints.widget == ecs::ui_widget_type::color_picker)
            {
                float col[3] = {val.x, val.y, val.z};
                const auto changed = ImGui::ColorEdit3(label_buf, col);
                if (ImGui::IsItemActivated())
                {
                    if (ctx.history != nullptr)
                    {
                        ctx.history->begin_gesture();
                    }
                }
                if (changed)
                {
                    val = math::vec3<float>{col[0], col[1], col[2]};
                    dispatch_change(&val, sizeof(val));
                }
                if (ImGui::IsItemDeactivated())
                {
                    if (ctx.history != nullptr)
                    {
                        ctx.history->end_gesture();
                    }
                }
            }
            else
            {
                float arr[3] = {val.x, val.y, val.z};
                const auto speed = field.hints.drag_speed > 0.0F ? field.hints.drag_speed : 0.1F;
                const auto changed = ImGui::DragFloat3(label_buf, arr, speed, 0.0F, 0.0F, "%.3f");
                if (ImGui::IsItemActivated())
                {
                    if (ctx.history != nullptr)
                    {
                        ctx.history->begin_gesture();
                    }
                }
                if (changed)
                {
                    val = math::vec3<float>{arr[0], arr[1], arr[2]};
                    dispatch_change(&val, sizeof(val));
                }
                if (ImGui::IsItemDeactivated())
                {
                    if (ctx.history != nullptr)
                    {
                        ctx.history->end_gesture();
                    }
                }
            }
            break;
        }

        case ecs::field_kind::vec4: {
            auto val = math::vec4<float>{0.0F, 0.0F, 0.0F, 0.0F};
            read_bytes(&val, sizeof(val));

            if (field.hints.widget == ecs::ui_widget_type::color_picker)
            {
                float col[4] = {val.x, val.y, val.z, val.w};
                const auto changed = ImGui::ColorEdit4(label_buf, col);
                if (ImGui::IsItemActivated())
                {
                    if (ctx.history != nullptr)
                    {
                        ctx.history->begin_gesture();
                    }
                }
                if (changed)
                {
                    val = math::vec4<float>{col[0], col[1], col[2], col[3]};
                    dispatch_change(&val, sizeof(val));
                }
                if (ImGui::IsItemDeactivated())
                {
                    if (ctx.history != nullptr)
                    {
                        ctx.history->end_gesture();
                    }
                }
            }
            else
            {
                float arr[4] = {val.x, val.y, val.z, val.w};
                const auto speed = field.hints.drag_speed > 0.0F ? field.hints.drag_speed : 0.1F;
                const auto changed = ImGui::DragFloat4(label_buf, arr, speed, 0.0F, 0.0F, "%.3f");
                if (ImGui::IsItemActivated())
                {
                    if (ctx.history != nullptr)
                    {
                        ctx.history->begin_gesture();
                    }
                }
                if (changed)
                {
                    val = math::vec4<float>{arr[0], arr[1], arr[2], arr[3]};
                    dispatch_change(&val, sizeof(val));
                }
                if (ImGui::IsItemDeactivated())
                {
                    if (ctx.history != nullptr)
                    {
                        ctx.history->end_gesture();
                    }
                }
            }
            break;
        }

        case ecs::field_kind::color3: {
            auto val = math::vec3<float>{1.0F, 1.0F, 1.0F};
            read_bytes(&val, sizeof(val));
            float col[3] = {val.x, val.y, val.z};
            const auto changed = ImGui::ColorEdit3(label_buf, col);
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                val = math::vec3<float>{col[0], col[1], col[2]};
                dispatch_change(&val, sizeof(val));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::color4: {
            auto val = math::vec4<float>{1.0F, 1.0F, 1.0F, 1.0F};
            read_bytes(&val, sizeof(val));
            float col[4] = {val.x, val.y, val.z, val.w};
            const auto changed = ImGui::ColorEdit4(label_buf, col);
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                val = math::vec4<float>{col[0], col[1], col[2], col[3]};
                dispatch_change(&val, sizeof(val));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::quat: {
            auto val = math::quat<float>{0.0F, 0.0F, 0.0F, 1.0F};
            read_bytes(&val, sizeof(val));

            const auto euler_rad = math::euler(val);
            const auto euler_deg = math::as_degrees(euler_rad);
            float arr[3] = {euler_deg.x, euler_deg.y, euler_deg.z};

            const auto changed = ImGui::DragFloat3(label_buf, arr, 0.5F, 0.0F, 0.0F, "%.2f deg");
            if (ImGui::IsItemActivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->begin_gesture();
                }
            }
            if (changed)
            {
                const auto new_deg = math::vec3<float>{arr[0], arr[1], arr[2]};
                const auto new_rad = math::as_radians(new_deg);
                const auto new_quat = math::quat<float>(new_rad);
                dispatch_change(&new_quat, sizeof(new_quat));
            }
            if (ImGui::IsItemDeactivated())
            {
                if (ctx.history != nullptr)
                {
                    ctx.history->end_gesture();
                }
            }
            break;
        }

        case ecs::field_kind::enumeration: {
            auto current_val = int64_t{0};
            read_bytes(&current_val, field.size);
            if (field.size == 1)
            {
                current_val = *reinterpret_cast<const int8_t*>(&current_val);
            }
            else if (field.size == 2)
            {
                current_val = *reinterpret_cast<const int16_t*>(&current_val);
            }
            else if (field.size == 4)
            {
                current_val = *reinterpret_cast<const int32_t*>(&current_val);
            }

            auto current_idx = -1;
            for (size_t i = 0; i < field.enum_entries.size(); ++i)
            {
                if (field.enum_entries[i].value == current_val)
                {
                    current_idx = static_cast<int>(i);
                    break;
                }
            }

            const auto preview = (current_idx >= 0 && current_idx < static_cast<int>(field.enum_entries.size()))
                                     ? field.enum_entries[static_cast<size_t>(current_idx)].name
                                     : "Unknown";

            char preview_buf[128]{};
            const auto preview_len = tempest::min(preview.size(), sizeof(preview_buf) - 1);
            tempest::memcpy(preview_buf, preview.data(), preview_len);
            preview_buf[preview_len] = '\0';

            if (ImGui::BeginCombo(label_buf, preview_buf))
            {
                for (size_t i = 0; i < field.enum_entries.size(); ++i)
                {
                    const auto is_sel = (current_idx == static_cast<int>(i));
                    char item_buf[128]{};
                    const auto item_len = tempest::min(field.enum_entries[i].name.size(), sizeof(item_buf) - 1);
                    tempest::memcpy(item_buf, field.enum_entries[i].name.data(), item_len);
                    item_buf[item_len] = '\0';

                    if (ImGui::Selectable(item_buf, is_sel))
                    {
                        const auto new_val = field.enum_entries[i].value;
                        dispatch_change(&new_val, field.size);
                    }
                    if (is_sel)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }

        case ecs::field_kind::guid:
        case ecs::field_kind::asset_ref: {
            if (field.kind == ecs::field_kind::guid && field.asset_type_hash == 0)
            {
                auto id = guid{};
                read_bytes(&id, sizeof(id));
                const auto id_str = to_string(id);
                char buf[64]{};
                tempest::memcpy(buf, id_str.data(), tempest::min(id_str.size(), sizeof(buf) - 1));
                buf[tempest::min(id_str.size(), sizeof(buf) - 1)] = '\0';

                ImGui::InputText(label_buf, buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
                break;
            }

            auto id = guid{};
            read_bytes(&id, sizeof(id));

            auto display_text = string{};
            if (id == guid{})
            {
                display_text = "<None>";
            }
            else if (ctx.asset_db != nullptr)
            {
                display_text = ctx.asset_db->get_asset_display_name(id);
            }
            else
            {
                display_text = to_string(id);
            }

            char popup_id[128]{};
            format_to_buffer(popup_id, "AssetPickerPopup##{}_{}", component_name, field.name);

            ImGui::Text("%.*s", static_cast<int>(display_name.size()), display_name.data());
            ImGui::SameLine();

            char button_label[256]{};
            format_to_buffer(button_label, "{}##btn_{}_{}", display_text, component_name, field.name);

            if (ImGui::Button(button_label))
            {
                ImGui::OpenPopup(popup_id);
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const auto* payload = ImGui::AcceptDragDropPayload("TEMPEST_ASSET_PAYLOAD"))
                {
                    if (payload->DataSize >= sizeof(guid))
                    {
                        const auto dropped_guid = *static_cast<const guid*>(payload->Data);
                        dispatch_change(&dropped_guid, sizeof(dropped_guid));
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (id != guid{})
            {
                ImGui::SameLine();
                char clear_label[64]{};
                format_to_buffer(clear_label, "X##clear_{}_{}", component_name, field.name);
                if (ImGui::Button(clear_label))
                {
                    const auto null_guid = guid{};
                    dispatch_change(&null_guid, sizeof(null_guid));
                }
            }

            if (ImGui::BeginPopup(popup_id))
            {
                auto local_filter = ImGuiTextFilter{};
                auto& filter = (ctx.search_filter != nullptr) ? *ctx.search_filter : local_filter;
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                    filter.Clear();
                }

                filter.Draw("##search_filter", -1.0F);

                if (ImGui::Selectable("<None>", id == guid{}))
                {
                    const auto null_guid = guid{};
                    dispatch_change(&null_guid, sizeof(null_guid));
                    ImGui::CloseCurrentPopup();
                }

                if (ctx.asset_db != nullptr)
                {
                    const auto target_type = assets::asset_type_id::from_hash(field.asset_type_hash);
                    const auto items = ctx.asset_db->get_assets_by_type(target_type, false);

                    if (ImGui::BeginChild("##AssetScrollList", ImVec2(280.0F, 250.0F)))
                    {
                        for (const auto& item : items)
                        {
                            if (!filter.PassFilter(item.display_name.c_str()))
                            {
                                continue;
                            }

                            if (ImGui::Selectable(item.display_name.c_str(), item.id == id))
                            {
                                dispatch_change(&item.id, sizeof(item.id));
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
                    ImGui::EndChild();
                }

                ImGui::EndPopup();
            }
            break;
        }

        case ecs::field_kind::entity_ref: {
            auto ref_ent = ecs::entity{ecs::tombstone};
            read_bytes(&ref_ent, sizeof(ref_ent));
            auto ent_name = string{};
            if (ctx.registry->is_valid(ref_ent))
            {
                const auto name_opt = ctx.registry->name(ref_ent);
                if (name_opt.has_value() && !name_opt->empty())
                {
                    ent_name = string(*name_opt);
                }
                else
                {
                    ent_name = format("Entity #{}", static_cast<uint32_t>(ref_ent));
                }
            }
            else
            {
                ent_name = "None (Null Entity)";
            }
            ImGui::Text("%.*s: %s", static_cast<int>(display_name.size()), display_name.data(), ent_name.c_str());
            break;
        }

        case ecs::field_kind::fixed_string: {
            char buf[256]{};
            read_bytes(buf, tempest::min(field.size, sizeof(buf) - 1));
            ImGui::InputText(label_buf, buf, sizeof(buf));
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                dispatch_change(buf, tempest::min(field.size, sizeof(buf)));
            }
            break;
        }

        default:
            break;
        }

        ImGui::EndDisabled();

        if (!field.hints.tooltip.empty() && ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(field.hints.tooltip.data(), field.hints.tooltip.data() + field.hints.tooltip.size());
            ImGui::EndTooltip();
        }

        if (is_overridden)
        {
            ImGui::PopStyleColor();
        }

        return field_mutated;
    }
} // namespace tempest::editor
