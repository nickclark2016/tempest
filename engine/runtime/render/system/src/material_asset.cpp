#include <tempest/material_asset.hpp>

#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/utility.hpp>

namespace tempest::assets
{
    namespace
    {
        auto parse_guid_from_value(const json_value& val) -> optional<guid>
        {
            if (val.is_string())
            {
                const auto str_opt = val.as_string();
                if (str_opt.has_value())
                {
                    return parse_guid(*str_opt);
                }
            }
            return nullopt;
        }

        auto extract_vec4_from_array(const json_value& array_val,
                                     const math::vec4<float>& default_val) -> math::vec4<float>
        {
            auto arr = json_array{};
            if (array_val.get(arr) && arr.size() >= 4)
            {
                return math::vec4<float>{
                    static_cast<float>(arr[0].as_number().value_or(default_val.x)),
                    static_cast<float>(arr[1].as_number().value_or(default_val.y)),
                    static_cast<float>(arr[2].as_number().value_or(default_val.z)),
                    static_cast<float>(arr[3].as_number().value_or(default_val.w)),
                };
            }
            return default_val;
        }

        auto extract_vec3_from_array(const json_value& array_val,
                                     const math::vec3<float>& default_val) -> math::vec3<float>
        {
            auto arr = json_array{};
            if (array_val.get(arr) && arr.size() >= 3)
            {
                return math::vec3<float>{
                    static_cast<float>(arr[0].as_number().value_or(default_val.x)),
                    static_cast<float>(arr[1].as_number().value_or(default_val.y)),
                    static_cast<float>(arr[2].as_number().value_or(default_val.z)),
                };
            }
            return default_val;
        }
    } // namespace

    auto material_asset::to_core_material() const -> core::material
    {
        auto mat = core::material{};
        mat.set_name(name);
        mat.set_string("t__shading_model", shading_model);
        mat.set_vec4(core::material::base_color_factor_name, base_color_factor);
        mat.set_scalar(core::material::roughness_factor_name, roughness_factor);
        mat.set_scalar(core::material::metallic_factor_name, metallic_factor);
        mat.set_scalar(core::material::transmissive_factor_name, transmission_factor);
        mat.set_vec3(core::material::emissive_factor_name, emissive_factor);
        mat.set_scalar(core::material::normal_scale_name, normal_scale);
        mat.set_scalar(core::material::occlusion_strength_name, occlusion_strength);
        mat.set_scalar(core::material::alpha_cutoff_name, alpha_cutoff);
        mat.set_string(core::material::alpha_mode_name, alpha_mode);
        mat.set_bool(core::material::double_sided_name, double_sided);

        if (base_color_map.has_value())
        {
            mat.set_texture(core::material::base_color_texture_name, *base_color_map);
        }
        if (metallic_roughness_map.has_value())
        {
            mat.set_texture(core::material::metallic_roughness_texture_name, *metallic_roughness_map);
        }
        if (normal_map.has_value())
        {
            mat.set_texture(core::material::normal_texture_name, *normal_map);
        }
        if (occlusion_map.has_value())
        {
            mat.set_texture(core::material::occlusion_texture_name, *occlusion_map);
        }
        if (emissive_map.has_value())
        {
            mat.set_texture(core::material::emissive_texture_name, *emissive_map);
        }
        if (transmission_map.has_value())
        {
            mat.set_texture(core::material::transmissive_texture_name, *transmission_map);
        }

        for (const auto& scalar_pair : custom_scalars)
        {
            mat.set_scalar(scalar_pair.first, scalar_pair.second);
        }
        for (const auto& vector_pair : custom_vec4s)
        {
            mat.set_vec4(vector_pair.first, vector_pair.second);
        }
        for (const auto& texture_pair : custom_textures)
        {
            mat.set_texture(texture_pair.first, texture_pair.second);
        }

        return mat;
    }

    auto material_asset::from_core_material(const core::material& mat) -> material_asset
    {
        auto asset = material_asset{};
        asset.name = string(mat.get_name());

        const auto shading_model_opt = mat.get_string("t__shading_model");
        if (shading_model_opt.has_value())
        {
            asset.shading_model = string(*shading_model_opt);
        }

        const auto base_color_opt = mat.get_vec4(core::material::base_color_factor_name);
        if (base_color_opt.has_value())
        {
            asset.base_color_factor = *base_color_opt;
        }

        const auto roughness_opt = mat.get_scalar(core::material::roughness_factor_name);
        if (roughness_opt.has_value())
        {
            asset.roughness_factor = *roughness_opt;
        }

        const auto metallic_opt = mat.get_scalar(core::material::metallic_factor_name);
        if (metallic_opt.has_value())
        {
            asset.metallic_factor = *metallic_opt;
        }

        const auto transmissive_opt = mat.get_scalar(core::material::transmissive_factor_name);
        if (transmissive_opt.has_value())
        {
            asset.transmission_factor = *transmissive_opt;
        }

        const auto emissive_opt = mat.get_vec3(core::material::emissive_factor_name);
        if (emissive_opt.has_value())
        {
            asset.emissive_factor = *emissive_opt;
        }

        const auto normal_scale_opt = mat.get_scalar(core::material::normal_scale_name);
        if (normal_scale_opt.has_value())
        {
            asset.normal_scale = *normal_scale_opt;
        }

        const auto occlusion_opt = mat.get_scalar(core::material::occlusion_strength_name);
        if (occlusion_opt.has_value())
        {
            asset.occlusion_strength = *occlusion_opt;
        }

        const auto cutoff_opt = mat.get_scalar(core::material::alpha_cutoff_name);
        if (cutoff_opt.has_value())
        {
            asset.alpha_cutoff = *cutoff_opt;
        }

        const auto alpha_mode_opt = mat.get_string(core::material::alpha_mode_name);
        if (alpha_mode_opt.has_value())
        {
            asset.alpha_mode = string(*alpha_mode_opt);
        }

        const auto double_sided_opt = mat.get_bool(core::material::double_sided_name);
        if (double_sided_opt.has_value())
        {
            asset.double_sided = *double_sided_opt;
        }

        asset.base_color_map = mat.get_texture(core::material::base_color_texture_name);
        asset.metallic_roughness_map = mat.get_texture(core::material::metallic_roughness_texture_name);
        asset.normal_map = mat.get_texture(core::material::normal_texture_name);
        asset.occlusion_map = mat.get_texture(core::material::occlusion_texture_name);
        asset.emissive_map = mat.get_texture(core::material::emissive_texture_name);
        asset.transmission_map = mat.get_texture(core::material::transmissive_texture_name);

        return asset;
    }

    auto material_asset::to_string() const -> string
    {
        auto writer = json_writer{};
        auto root = writer.create_object();

        root.set("name", name);
        root.set("shading_model", shading_model);

        auto base_color_array = root.create_child_array("base_color_factor");
        base_color_array.push_back(base_color_factor.x);
        base_color_array.push_back(base_color_factor.y);
        base_color_array.push_back(base_color_factor.z);
        base_color_array.push_back(base_color_factor.w);

        root.set("roughness_factor", roughness_factor);
        root.set("metallic_factor", metallic_factor);
        root.set("transmission_factor", transmission_factor);

        auto emissive_array = root.create_child_array("emissive_factor");
        emissive_array.push_back(emissive_factor.x);
        emissive_array.push_back(emissive_factor.y);
        emissive_array.push_back(emissive_factor.z);

        root.set("normal_scale", normal_scale);
        root.set("occlusion_strength", occlusion_strength);
        root.set("alpha_cutoff", alpha_cutoff);
        root.set("alpha_mode", alpha_mode);
        root.set("double_sided", double_sided);

        auto textures_object = root.create_child_object("textures");
        if (base_color_map.has_value())
        {
            textures_object.set("base_color_map", tempest::to_string(*base_color_map));
        }
        if (metallic_roughness_map.has_value())
        {
            textures_object.set("metallic_roughness_map", tempest::to_string(*metallic_roughness_map));
        }
        if (normal_map.has_value())
        {
            textures_object.set("normal_map", tempest::to_string(*normal_map));
        }
        if (occlusion_map.has_value())
        {
            textures_object.set("occlusion_map", tempest::to_string(*occlusion_map));
        }
        if (emissive_map.has_value())
        {
            textures_object.set("emissive_map", tempest::to_string(*emissive_map));
        }
        if (transmission_map.has_value())
        {
            textures_object.set("transmission_map", tempest::to_string(*transmission_map));
        }

        if (!custom_scalars.empty())
        {
            auto custom_scalars_object = root.create_child_object("custom_scalars");
            for (const auto& scalar_pair : custom_scalars)
            {
                custom_scalars_object.set(scalar_pair.first, scalar_pair.second);
            }
        }

        if (!custom_vec4s.empty())
        {
            auto custom_vec4s_object = root.create_child_object("custom_vec4s");
            for (const auto& vector_pair : custom_vec4s)
            {
                auto vector_array = custom_vec4s_object.create_child_array(vector_pair.first);
                vector_array.push_back(vector_pair.second.x);
                vector_array.push_back(vector_pair.second.y);
                vector_array.push_back(vector_pair.second.z);
                vector_array.push_back(vector_pair.second.w);
            }
        }

        if (!custom_textures.empty())
        {
            auto custom_textures_object = root.create_child_object("custom_textures");
            for (const auto& texture_pair : custom_textures)
            {
                custom_textures_object.set(texture_pair.first, tempest::to_string(texture_pair.second));
            }
        }

        writer.set_root(root);
        return writer.to_string(json_format::pretty);
    }

    auto material_asset::load_from_string(string_view json_str) -> optional<material_asset>
    {
        auto allocator = system_allocator{};
        auto doc_res = json_document::from_string(json_str, allocator);
        if (!doc_res.has_value())
        {
            return nullopt;
        }

        const auto root = doc_res->root();
        if (!root.is_object())
        {
            return nullopt;
        }

        auto result = material_asset{};

        const auto name_val = root["name"].as_string();
        if (name_val.has_value())
        {
            result.name = string(*name_val);
        }

        const auto shading_model_val = root["shading_model"].as_string();
        if (shading_model_val.has_value())
        {
            result.shading_model = string(*shading_model_val);
        }

        result.base_color_factor = extract_vec4_from_array(root["base_color_factor"], default_base_color_factor);
        result.roughness_factor =
            static_cast<float>(root["roughness_factor"].as_number().value_or(default_roughness_factor));
        result.metallic_factor =
            static_cast<float>(root["metallic_factor"].as_number().value_or(default_metallic_factor));
        result.transmission_factor =
            static_cast<float>(root["transmission_factor"].as_number().value_or(default_transmission_factor));
        result.emissive_factor = extract_vec3_from_array(root["emissive_factor"], default_emissive_factor);
        result.normal_scale = static_cast<float>(root["normal_scale"].as_number().value_or(default_normal_scale));
        result.occlusion_strength =
            static_cast<float>(root["occlusion_strength"].as_number().value_or(default_occlusion_strength));
        result.alpha_cutoff = static_cast<float>(root["alpha_cutoff"].as_number().value_or(default_alpha_cutoff));

        const auto alpha_mode_val = root["alpha_mode"].as_string();
        if (alpha_mode_val.has_value())
        {
            result.alpha_mode = string(*alpha_mode_val);
        }

        result.double_sided = root["double_sided"].as_bool().value_or(default_double_sided);

        const auto textures_node = root["textures"];
        const auto get_texture_node = [&](string_view texture_key) -> json_value {
            if (textures_node.is_object() && textures_node.contains(texture_key))
            {
                return textures_node[texture_key];
            }
            return root[texture_key];
        };

        result.base_color_map = parse_guid_from_value(get_texture_node("base_color_map"));
        result.metallic_roughness_map = parse_guid_from_value(get_texture_node("metallic_roughness_map"));
        result.normal_map = parse_guid_from_value(get_texture_node("normal_map"));
        result.occlusion_map = parse_guid_from_value(get_texture_node("occlusion_map"));
        result.emissive_map = parse_guid_from_value(get_texture_node("emissive_map"));
        result.transmission_map = parse_guid_from_value(get_texture_node("transmission_map"));

        const auto custom_scalars_val = root["custom_scalars"];
        if (custom_scalars_val.is_object())
        {
            const auto obj = custom_scalars_val.as_object().value();
            for (const auto& member : obj)
            {
                const auto number_val = member.value.as_number();
                if (number_val.has_value())
                {
                    result.custom_scalars.insert({string(member.key), static_cast<float>(*number_val)});
                }
            }
        }

        const auto custom_vec4s_val = root["custom_vec4s"];
        if (custom_vec4s_val.is_object())
        {
            const auto obj = custom_vec4s_val.as_object().value();
            for (const auto& member : obj)
            {
                const auto parsed_vec = extract_vec4_from_array(member.value, math::vec4<float>{0.0f, 0.0f, 0.0f, 0.0f});
                result.custom_vec4s.insert({string(member.key), parsed_vec});
            }
        }

        const auto custom_textures_val = root["custom_textures"];
        if (custom_textures_val.is_object())
        {
            const auto obj = custom_textures_val.as_object().value();
            for (const auto& member : obj)
            {
                const auto guid_opt = parse_guid_from_value(member.value);
                if (guid_opt.has_value())
                {
                    result.custom_textures.insert({string(member.key), *guid_opt});
                }
            }
        }

        return result;
    }

    auto material_asset::save_to_file(string_view path) const -> bool
    {
        const auto json_str = to_string();
        const auto write_result = write_file_from_bytes(
            filesystem::path(path),
            span<const byte>{reinterpret_cast<const byte*>(json_str.data()), json_str.size()});
        return write_result.has_value();
    }

    auto material_asset::load_from_file(string_view path) -> optional<material_asset>
    {
        const auto content = read_file_to_string(filesystem::path(path));
        if (!content.has_value())
        {
            return nullopt;
        }
        return load_from_string(*content);
    }
} // namespace tempest::assets
