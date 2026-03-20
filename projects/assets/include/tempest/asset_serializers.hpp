#ifndef tempest_assets_asset_serializers_hpp
#define tempest_assets_asset_serializers_hpp

#include <tempest/asset_type_id.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/guid.hpp>
#include <tempest/material.hpp>
#include <tempest/serial.hpp>
#include <tempest/texture.hpp>
#include <tempest/utility.hpp>
#include <tempest/vertex.hpp>

namespace tempest::serialization
{
    // guid is trivially copyable, so the default specialization handles it.
    // asset_type_id is trivially copyable, so the default specialization handles it.
    // content_hash (array<byte,32>) is trivially copyable, so the default specialization handles it.

    // --- core::texture ---

    template <>
    struct serializer<binary_archive, core::texture_mip_data>
    {
        static auto serialize(binary_archive& archive, const core::texture_mip_data& mip) -> void
        {
            serializer<binary_archive, vector<byte>>::serialize(archive, mip.data);
            serializer<binary_archive, uint32_t>::serialize(archive, mip.width);
            serializer<binary_archive, uint32_t>::serialize(archive, mip.height);
        }

        static auto deserialize(binary_archive& archive) -> core::texture_mip_data
        {
            auto data = serializer<binary_archive, vector<byte>>::deserialize(archive);
            auto width = serializer<binary_archive, uint32_t>::deserialize(archive);
            auto height = serializer<binary_archive, uint32_t>::deserialize(archive);
            return core::texture_mip_data{
                .data = tempest::move(data),
                .width = width,
                .height = height,
            };
        }
    };

    template <>
    struct serializer<binary_archive, core::sampler_state>
    {
        static auto serialize(binary_archive& archive, const core::sampler_state& sampler) -> void
        {
            serializer<binary_archive, uint32_t>::serialize(archive, static_cast<uint32_t>(sampler.mag_filter));
            serializer<binary_archive, uint32_t>::serialize(archive, static_cast<uint32_t>(sampler.min_filter));
            serializer<binary_archive, uint32_t>::serialize(archive, static_cast<uint32_t>(sampler.wrap_s));
            serializer<binary_archive, uint32_t>::serialize(archive, static_cast<uint32_t>(sampler.wrap_t));
        }

        static auto deserialize(binary_archive& archive) -> core::sampler_state
        {
            auto mag = serializer<binary_archive, uint32_t>::deserialize(archive);
            auto min = serializer<binary_archive, uint32_t>::deserialize(archive);
            auto wrap_s = serializer<binary_archive, uint32_t>::deserialize(archive);
            auto wrap_t = serializer<binary_archive, uint32_t>::deserialize(archive);
            return core::sampler_state{
                .mag_filter = static_cast<core::magnify_texture_filter>(mag),
                .min_filter = static_cast<core::minify_texture_filter>(min),
                .wrap_s = static_cast<core::texture_wrap_mode>(wrap_s),
                .wrap_t = static_cast<core::texture_wrap_mode>(wrap_t),
            };
        }
    };

    template <>
    struct serializer<binary_archive, core::texture>
    {
        static auto serialize(binary_archive& archive, const core::texture& tex) -> void
        {
            serializer<binary_archive, uint32_t>::serialize(archive, static_cast<uint32_t>(tex.format));
            serializer<binary_archive, uint32_t>::serialize(archive, static_cast<uint32_t>(tex.compression));
            serializer<binary_archive, uint32_t>::serialize(archive, tex.width);
            serializer<binary_archive, uint32_t>::serialize(archive, tex.height);
            serializer<binary_archive, core::sampler_state>::serialize(archive, tex.sampler);
            serializer<binary_archive, string>::serialize(archive, tex.name);
            serializer<binary_archive, vector<core::texture_mip_data>>::serialize(archive, tex.mips);
        }

        static auto deserialize(binary_archive& archive) -> core::texture
        {
            auto format = static_cast<core::texture_format>(
                serializer<binary_archive, uint32_t>::deserialize(archive));
            auto compression = static_cast<core::texture_compression>(
                serializer<binary_archive, uint32_t>::deserialize(archive));
            auto width = serializer<binary_archive, uint32_t>::deserialize(archive);
            auto height = serializer<binary_archive, uint32_t>::deserialize(archive);
            auto sampler = serializer<binary_archive, core::sampler_state>::deserialize(archive);
            auto name = serializer<binary_archive, string>::deserialize(archive);
            auto mips = serializer<binary_archive, vector<core::texture_mip_data>>::deserialize(archive);
            return core::texture{
                .mips = tempest::move(mips),
                .width = width,
                .height = height,
                .format = format,
                .compression = compression,
                .sampler = sampler,
                .name = tempest::move(name),
            };
        }
    };

    // --- core::mesh ---

    template <>
    struct serializer<binary_archive, core::vertex>
    {
        static auto serialize(binary_archive& archive, const core::vertex& vert) -> void
        {
            // vertex is POD with vec3, vec2, vec3, vec4, vec4 - serialize as raw bytes
            const auto byte_span = as_bytes(span<const core::vertex>{&vert, 1});
            archive.write(byte_span);
        }

        static auto deserialize(binary_archive& archive) -> core::vertex
        {
            const auto byte_span = archive.read(sizeof(core::vertex));
            core::vertex vert;
            tempest::memcpy(&vert, byte_span.data(), sizeof(core::vertex));
            return vert;
        }
    };

    template <>
    struct serializer<binary_archive, core::mesh>
    {
        static auto serialize(binary_archive& archive, const core::mesh& mesh) -> void
        {
            serializer<binary_archive, vector<core::vertex>>::serialize(archive, mesh.vertices);
            serializer<binary_archive, vector<uint32_t>>::serialize(archive, mesh.indices);
            serializer<binary_archive, string>::serialize(archive, mesh.name);
            serializer<binary_archive, bool>::serialize(archive, mesh.has_normals);
            serializer<binary_archive, bool>::serialize(archive, mesh.has_tangents);
            serializer<binary_archive, bool>::serialize(archive, mesh.has_colors);
        }

        static auto deserialize(binary_archive& archive) -> core::mesh
        {
            auto vertices = serializer<binary_archive, vector<core::vertex>>::deserialize(archive);
            auto indices = serializer<binary_archive, vector<uint32_t>>::deserialize(archive);
            auto name = serializer<binary_archive, string>::deserialize(archive);
            auto has_normals = serializer<binary_archive, bool>::deserialize(archive);
            auto has_tangents = serializer<binary_archive, bool>::deserialize(archive);
            auto has_colors = serializer<binary_archive, bool>::deserialize(archive);
            core::mesh result;
            result.vertices = tempest::move(vertices);
            result.indices = tempest::move(indices);
            result.name = tempest::move(name);
            result.has_normals = has_normals;
            result.has_tangents = has_tangents;
            result.has_colors = has_colors;
            return result;
        }
    };

    // --- core::material ---

    template <>
    struct serializer<binary_archive, core::material>
    {
        static auto serialize(binary_archive& archive, const core::material& mat) -> void;
        static auto deserialize(binary_archive& archive) -> core::material;
    };

    // --- entity_hierarchy ---

    template <>
    struct serializer<binary_archive, assets::entity_hierarchy::entity_record>
    {
        static auto serialize(binary_archive& archive,
                              const assets::entity_hierarchy::entity_record& record) -> void
        {
            auto num_components = static_cast<uint64_t>(record.components.size());
            serializer<binary_archive, uint64_t>::serialize(archive, num_components);
            for (const auto& comp : record.components)
            {
                serializer<binary_archive, uint64_t>::serialize(archive, static_cast<uint64_t>(comp.first));
                serializer<binary_archive, vector<byte>>::serialize(archive, comp.second);
            }
            serializer<binary_archive, vector<size_t>>::serialize(archive, record.child_indices);
        }

        static auto deserialize(binary_archive& archive) -> assets::entity_hierarchy::entity_record
        {
            auto num_components = serializer<binary_archive, uint64_t>::deserialize(archive);
            assets::entity_hierarchy::entity_record record;
            record.components.reserve(num_components);
            for (uint64_t idx = 0; idx < num_components; ++idx)
            {
                auto hash = static_cast<size_t>(serializer<binary_archive, uint64_t>::deserialize(archive));
                auto data = serializer<binary_archive, vector<byte>>::deserialize(archive);
                record.components.push_back(pair<size_t, vector<byte>>{hash, tempest::move(data)});
            }
            record.child_indices = serializer<binary_archive, vector<size_t>>::deserialize(archive);
            return record;
        }
    };

    template <>
    struct serializer<binary_archive, assets::entity_hierarchy>
    {
        static auto serialize(binary_archive& archive, const assets::entity_hierarchy& hierarchy) -> void
        {
            serializer<binary_archive, vector<assets::entity_hierarchy::entity_record>>::serialize(
                archive, hierarchy.records);
            serializer<binary_archive, uint64_t>::serialize(archive, static_cast<uint64_t>(hierarchy.root_index));
        }

        static auto deserialize(binary_archive& archive) -> assets::entity_hierarchy
        {
            auto records =
                serializer<binary_archive, vector<assets::entity_hierarchy::entity_record>>::deserialize(archive);
            auto root_index = static_cast<size_t>(serializer<binary_archive, uint64_t>::deserialize(archive));
            return assets::entity_hierarchy{
                .records = tempest::move(records),
                .root_index = root_index,
            };
        }
    };
} // namespace tempest::serialization

#endif // tempest_assets_asset_serializers_hpp
