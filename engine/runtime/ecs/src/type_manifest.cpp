#include <tempest/type_manifest.hpp>

#include <tempest/format.hpp>
#include <tempest/string_view.hpp>

namespace tempest::ecs
{
    namespace
    {
        constexpr auto field_kind_to_string_view(field_kind kind) noexcept -> string_view
        {
            switch (kind)
            {
                case field_kind::boolean:
                    return "boolean";
                case field_kind::int32:
                    return "int32";
                case field_kind::uint32:
                    return "uint32";
                case field_kind::int64:
                    return "int64";
                case field_kind::uint64:
                    return "uint64";
                case field_kind::float32:
                    return "float32";
                case field_kind::float64:
                    return "float64";
                case field_kind::vec2:
                    return "vec2";
                case field_kind::vec3:
                    return "vec3";
                case field_kind::vec4:
                    return "vec4";
                case field_kind::quat:
                    return "quat";
                case field_kind::color3:
                    return "color3";
                case field_kind::color4:
                    return "color4";
                case field_kind::guid:
                    return "guid";
                case field_kind::entity_ref:
                    return "entity_ref";
                case field_kind::asset_ref:
                    return "asset_ref";
                case field_kind::enumeration:
                    return "enumeration";
                case field_kind::fixed_string:
                    return "fixed_string";
            }
            return "unknown";
        }
    } // namespace

    auto type_manifest::to_json(core::json_format fmt) const -> string
    {
        auto writer = core::json_writer{};
        auto root = writer.create_object();

        const auto hash_str = tempest::format("0x{:016x}", schema_hash);
        root.set("schema_hash", string_view{hash_str.data(), hash_str.size()});

        auto types_arr = root.create_child_array("types");
        for (const auto& entry : types)
        {
            auto type_obj = types_arr.create_child_object();
            type_obj.set("name", string_view{entry.name.data(), entry.name.size()});
            type_obj.set("size", static_cast<uint64_t>(entry.size));
            type_obj.set("alignment", static_cast<uint64_t>(entry.alignment));
            type_obj.set("version", entry.version);
            type_obj.set("is_serializable", entry.is_serializable);
            type_obj.set("is_authorable", entry.is_authorable);

            auto fields_arr = type_obj.create_child_array("fields");
            for (const auto& field : entry.fields)
            {
                auto field_obj = fields_arr.create_child_object();
                field_obj.set("name", string_view{field.name.data(), field.name.size()});
                field_obj.set("kind", field_kind_to_string_view(field.kind));
                field_obj.set("flags", static_cast<uint32_t>(field.flags));
                field_obj.set("offset", static_cast<uint64_t>(field.offset));
                field_obj.set("size", static_cast<uint64_t>(field.size));
            }
        }

        writer.set_root(root);
        return writer.to_string(fmt);
    }
} // namespace tempest::ecs
