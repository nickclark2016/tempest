#include <tempest/asset_serializers.hpp>

namespace tempest::serialization
{
    auto serializer<binary_archive, core::material>::serialize(binary_archive& archive,
                                                               const core::material& mat) -> void
    {
        // Serialize name
        serializer<binary_archive, string>::serialize(archive, string(mat.get_name()));

        // We serialize the material by iterating known property names.
        // Since material uses private maps accessed via getters, we serialize each map
        // by first writing the count, then key-value pairs.

        // To access the internal data, we use a pattern of known PBR property names.
        // We serialize as flat_unordered_map<string, T> using a capture approach.

        // Textures
        static constexpr string_view texture_names[] = {
            core::material::base_color_texture_name,
            core::material::metallic_roughness_texture_name,
            core::material::normal_texture_name,
            core::material::occlusion_texture_name,
            core::material::emissive_texture_name,
            core::material::transmissive_texture_name,
            core::material::volume_thickness_texture_name,
        };

        {
            // Count textures that have values
            uint64_t count = 0;
            for (auto name : texture_names)
            {
                if (mat.get_texture(name).has_value())
                {
                    ++count;
                }
            }
            serializer<binary_archive, uint64_t>::serialize(archive, count);
            for (auto name : texture_names)
            {
                auto val = mat.get_texture(name);
                if (val.has_value())
                {
                    serializer<binary_archive, string>::serialize(archive, string(name));
                    serializer<binary_archive, guid>::serialize(archive, val.value());
                }
            }
        }

        // Scalars
        static constexpr string_view scalar_names[] = {
            core::material::metallic_factor_name,
            core::material::roughness_factor_name,
            core::material::normal_scale_name,
            core::material::occlusion_strength_name,
            core::material::alpha_cutoff_name,
            core::material::transmissive_factor_name,
            core::material::volume_thickness_factor_name,
            core::material::volume_attenuation_distance_name,
        };

        {
            uint64_t count = 0;
            for (auto name : scalar_names)
            {
                if (mat.get_scalar(name).has_value())
                {
                    ++count;
                }
            }
            serializer<binary_archive, uint64_t>::serialize(archive, count);
            for (auto name : scalar_names)
            {
                auto val = mat.get_scalar(name);
                if (val.has_value())
                {
                    serializer<binary_archive, string>::serialize(archive, string(name));
                    serializer<binary_archive, float>::serialize(archive, val.value());
                }
            }
        }

        // Bools
        static constexpr string_view bool_names[] = {
            core::material::double_sided_name,
        };

        {
            uint64_t count = 0;
            for (auto name : bool_names)
            {
                if (mat.get_bool(name).has_value())
                {
                    ++count;
                }
            }
            serializer<binary_archive, uint64_t>::serialize(archive, count);
            for (auto name : bool_names)
            {
                auto val = mat.get_bool(name);
                if (val.has_value())
                {
                    serializer<binary_archive, string>::serialize(archive, string(name));
                    serializer<binary_archive, bool>::serialize(archive, val.value());
                }
            }
        }

        // Vec2s (none in standard PBR, but handle generically)
        {
            serializer<binary_archive, uint64_t>::serialize(archive, static_cast<uint64_t>(0));
        }

        // Vec3s
        static constexpr string_view vec3_names[] = {
            core::material::emissive_factor_name,
            core::material::volume_attenuation_color_name,
        };

        {
            uint64_t count = 0;
            for (auto name : vec3_names)
            {
                if (mat.get_vec3(name).has_value())
                {
                    ++count;
                }
            }
            serializer<binary_archive, uint64_t>::serialize(archive, count);
            for (auto name : vec3_names)
            {
                auto val = mat.get_vec3(name);
                if (val.has_value())
                {
                    serializer<binary_archive, string>::serialize(archive, string(name));
                    auto vec = val.value();
                    serializer<binary_archive, float>::serialize(archive, vec.x);
                    serializer<binary_archive, float>::serialize(archive, vec.y);
                    serializer<binary_archive, float>::serialize(archive, vec.z);
                }
            }
        }

        // Vec4s
        static constexpr string_view vec4_names[] = {
            core::material::base_color_factor_name,
        };

        {
            uint64_t count = 0;
            for (auto name : vec4_names)
            {
                if (mat.get_vec4(name).has_value())
                {
                    ++count;
                }
            }
            serializer<binary_archive, uint64_t>::serialize(archive, count);
            for (auto name : vec4_names)
            {
                auto val = mat.get_vec4(name);
                if (val.has_value())
                {
                    serializer<binary_archive, string>::serialize(archive, string(name));
                    auto vec = val.value();
                    serializer<binary_archive, float>::serialize(archive, vec.x);
                    serializer<binary_archive, float>::serialize(archive, vec.y);
                    serializer<binary_archive, float>::serialize(archive, vec.z);
                    serializer<binary_archive, float>::serialize(archive, vec.w);
                }
            }
        }

        // Strings
        static constexpr string_view string_names[] = {
            core::material::alpha_mode_name,
        };

        {
            uint64_t count = 0;
            for (auto name : string_names)
            {
                if (mat.get_string(name).has_value())
                {
                    ++count;
                }
            }
            serializer<binary_archive, uint64_t>::serialize(archive, count);
            for (auto name : string_names)
            {
                auto val = mat.get_string(name);
                if (val.has_value())
                {
                    serializer<binary_archive, string>::serialize(archive, string(name));
                    serializer<binary_archive, string>::serialize(archive, string(val.value()));
                }
            }
        }
    }

    auto serializer<binary_archive, core::material>::deserialize(binary_archive& archive) -> core::material
    {
        core::material mat;

        auto name = serializer<binary_archive, string>::deserialize(archive);
        mat.set_name(tempest::move(name));

        // Textures
        {
            auto count = serializer<binary_archive, uint64_t>::deserialize(archive);
            for (uint64_t idx = 0; idx < count; ++idx)
            {
                auto key = serializer<binary_archive, string>::deserialize(archive);
                auto val = serializer<binary_archive, guid>::deserialize(archive);
                mat.set_texture(key, val);
            }
        }

        // Scalars
        {
            auto count = serializer<binary_archive, uint64_t>::deserialize(archive);
            for (uint64_t idx = 0; idx < count; ++idx)
            {
                auto key = serializer<binary_archive, string>::deserialize(archive);
                auto val = serializer<binary_archive, float>::deserialize(archive);
                mat.set_scalar(key, val);
            }
        }

        // Bools
        {
            auto count = serializer<binary_archive, uint64_t>::deserialize(archive);
            for (uint64_t idx = 0; idx < count; ++idx)
            {
                auto key = serializer<binary_archive, string>::deserialize(archive);
                auto val = serializer<binary_archive, bool>::deserialize(archive);
                mat.set_bool(key, val);
            }
        }

        // Vec2s
        {
            auto count = serializer<binary_archive, uint64_t>::deserialize(archive);
            for (uint64_t idx = 0; idx < count; ++idx)
            {
                auto key = serializer<binary_archive, string>::deserialize(archive);
                auto vec_x = serializer<binary_archive, float>::deserialize(archive);
                auto vec_y = serializer<binary_archive, float>::deserialize(archive);
                mat.set_vec2(key, math::vec2<float>{vec_x, vec_y});
            }
        }

        // Vec3s
        {
            auto count = serializer<binary_archive, uint64_t>::deserialize(archive);
            for (uint64_t idx = 0; idx < count; ++idx)
            {
                auto key = serializer<binary_archive, string>::deserialize(archive);
                auto vec_x = serializer<binary_archive, float>::deserialize(archive);
                auto vec_y = serializer<binary_archive, float>::deserialize(archive);
                auto vec_z = serializer<binary_archive, float>::deserialize(archive);
                mat.set_vec3(key, math::vec3<float>{vec_x, vec_y, vec_z});
            }
        }

        // Vec4s
        {
            auto count = serializer<binary_archive, uint64_t>::deserialize(archive);
            for (uint64_t idx = 0; idx < count; ++idx)
            {
                auto key = serializer<binary_archive, string>::deserialize(archive);
                auto vec_x = serializer<binary_archive, float>::deserialize(archive);
                auto vec_y = serializer<binary_archive, float>::deserialize(archive);
                auto vec_z = serializer<binary_archive, float>::deserialize(archive);
                auto vec_w = serializer<binary_archive, float>::deserialize(archive);
                mat.set_vec4(key, math::vec4<float>{vec_x, vec_y, vec_z, vec_w});
            }
        }

        // Strings
        {
            auto count = serializer<binary_archive, uint64_t>::deserialize(archive);
            for (uint64_t idx = 0; idx < count; ++idx)
            {
                auto key = serializer<binary_archive, string>::deserialize(archive);
                auto val = serializer<binary_archive, string>::deserialize(archive);
                mat.set_string(key, tempest::move(val));
            }
        }

        return mat;
    }
} // namespace tempest::serialization
