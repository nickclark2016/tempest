#include <tempest/gltf_importer.hpp>

#include "gltf/gltf.hpp"

#include <tempest/flat_unordered_map.hpp>
#include <tempest/logger.hpp>
#include <tempest/material.hpp>
#include <tempest/memory.hpp>
#include <tempest/mesh.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/texture.hpp>
#include <tempest/vector.hpp>

#include <filesystem>
#include <fstream>

#include <simdjson.h>
#include <stb_image.h>

namespace
{
    auto logger = tempest::logger::logger_factory::create({.prefix = "GLTF Importer"});
}

template <>
simdjson::simdjson_result<tempest::string> simdjson::ondemand::value::get() noexcept
{
    std::string_view sv;
    if (auto error = get_string().get(sv))
    {
        return error;
    }

    return tempest::string{sv.data(), sv.size()};
}

#define MAKE_SIMDJSON_ENUM_SPECIALIZATION(type)                                                                        \
    template <>                                                                                                        \
    simdjson_inline simdjson::simdjson_result<type> simdjson::ondemand::value::get() noexcept                          \
    {                                                                                                                  \
        tempest::conditional_t<tempest::is_unsigned_v<tempest::underlying_type_t<type>>, tempest::uint64_t,            \
                               tempest::int64_t>                                                                       \
            result;                                                                                                    \
        if (auto error = get(result))                                                                                  \
        {                                                                                                              \
            return error;                                                                                              \
        }                                                                                                              \
        return static_cast<type>(result);                                                                              \
    }

#define MAKE_SIMDJSON_ARRAY_SPECIALIZATION(type, count)                                                                \
    template <>                                                                                                        \
    simdjson_inline simdjson::simdjson_result<tempest::array<type, count>> simdjson::ondemand::value::get() noexcept   \
    {                                                                                                                  \
        tempest::array<type, count> result;                                                                            \
        tempest::size_t idx = 0;                                                                                       \
        for (auto element : get_array())                                                                               \
        {                                                                                                              \
            type value;                                                                                                \
            if (auto error = element.get(value))                                                                       \
            {                                                                                                          \
                return error;                                                                                          \
            }                                                                                                          \
            result[idx] = tempest::move(value);                                                                        \
        }                                                                                                              \
        return result;                                                                                                 \
    }

#define MAKE_SIMDJSON_VECTOR_SPECIALIZATION(type, allocator)                                                           \
    template <>                                                                                                        \
    simdjson::simdjson_result<tempest::vector<type, allocator>> simdjson::ondemand::value::get() noexcept              \
    {                                                                                                                  \
        tempest::vector<type, allocator> result;                                                                       \
        for (auto element : get_array())                                                                               \
        {                                                                                                              \
            type value;                                                                                                \
            if (auto error = element.get(value))                                                                       \
            {                                                                                                          \
                return error;                                                                                          \
            }                                                                                                          \
            result.push_back(value);                                                                                   \
        }                                                                                                              \
        return result;                                                                                                 \
    }

#define MAKE_SIMDJSON_OPTIONAL_SPECIALIZATION(type)                                                                    \
    template <>                                                                                                        \
    simdjson::simdjson_result<tempest::optional<type>> simdjson::ondemand::value::get() noexcept                       \
    {                                                                                                                  \
        if (is_null())                                                                                                 \
        {                                                                                                              \
            return tempest::optional<type>();                                                                          \
        }                                                                                                              \
        type value;                                                                                                    \
        if (auto error = get(value))                                                                                   \
        {                                                                                                              \
            return error;                                                                                              \
        }                                                                                                              \
        return tempest::some(value);                                                                                   \
    }

MAKE_SIMDJSON_ENUM_SPECIALIZATION(tempest::assets::gltf::component_type)
MAKE_SIMDJSON_ENUM_SPECIALIZATION(tempest::assets::gltf::buffer_view_target)
MAKE_SIMDJSON_ENUM_SPECIALIZATION(tempest::assets::gltf::mag_filter)
MAKE_SIMDJSON_ENUM_SPECIALIZATION(tempest::assets::gltf::min_filter)
MAKE_SIMDJSON_ENUM_SPECIALIZATION(tempest::assets::gltf::topology)
MAKE_SIMDJSON_ENUM_SPECIALIZATION(tempest::assets::gltf::wrap_mode)

MAKE_SIMDJSON_ARRAY_SPECIALIZATION(double, 3)
MAKE_SIMDJSON_ARRAY_SPECIALIZATION(double, 4)
MAKE_SIMDJSON_ARRAY_SPECIALIZATION(double, 16)

MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::string, tempest::allocator<tempest::string>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(double, tempest::allocator<double>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::size_t, tempest::allocator<tempest::size_t>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::accessor,
                                    tempest::allocator<tempest::assets::gltf::accessor>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::buffer, tempest::allocator<tempest::assets::gltf::buffer>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::buffer_view,
                                    tempest::allocator<tempest::assets::gltf::buffer_view>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::image, tempest::allocator<tempest::assets::gltf::image>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::material,
                                    tempest::allocator<tempest::assets::gltf::material>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::mesh_primitive,
                                    tempest::allocator<tempest::assets::gltf::mesh_primitive>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::mesh, tempest::allocator<tempest::assets::gltf::mesh>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::node, tempest::allocator<tempest::assets::gltf::node>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::sampler, tempest::allocator<tempest::assets::gltf::sampler>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::scene, tempest::allocator<tempest::assets::gltf::scene>)
MAKE_SIMDJSON_VECTOR_SPECIALIZATION(tempest::assets::gltf::texture, tempest::allocator<tempest::assets::gltf::texture>)

MAKE_SIMDJSON_OPTIONAL_SPECIALIZATION(tempest::string)
MAKE_SIMDJSON_OPTIONAL_SPECIALIZATION(double)
MAKE_SIMDJSON_OPTIONAL_SPECIALIZATION(tempest::size_t)
MAKE_SIMDJSON_OPTIONAL_SPECIALIZATION(tempest::assets::gltf::buffer_view_target)
MAKE_SIMDJSON_OPTIONAL_SPECIALIZATION(tempest::assets::gltf::texture_info)
MAKE_SIMDJSON_OPTIONAL_SPECIALIZATION(tempest::assets::gltf::normal_texture_info)
MAKE_SIMDJSON_OPTIONAL_SPECIALIZATION(tempest::assets::gltf::occlusion_texture_info)
MAKE_SIMDJSON_OPTIONAL_SPECIALIZATION(tempest::assets::gltf::pbr_metallic_roughness)

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::asset> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::asset result;

    if (auto error = obj["generator"].get(result.generator))
    {
        return error;
    }

    if (auto error = obj["version"].get(result.version))
    {
        return error;
    }

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::accessor> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::accessor result;

    if (auto error = obj["componentType"].get(result.comp_type))
    {
        return error;
    }

    if (auto error = obj["count"].get(result.count))
    {
        return error;
    }

    if (auto error = obj["type"].get(result.type))
    {
        return error;
    }

    if (obj["byteOffset"].get(result.byte_offset))
    {
        result.byte_offset = 0;
    }

    if (obj["normalized"].get(result.normalized))
    {
        result.normalized = false;
    }

    obj["bufferView"].get(result.buffer_view);
    obj["max"].get(result.max);
    obj["min"].get(result.min);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::buffer> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::buffer result;

    if (auto error = obj["byteLength"].get(result.byte_length))
    {
        return error;
    }

    // Optional fields
    obj["uri"].get(result.uri);
    obj["name"].get(result.name);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::buffer_view> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::buffer_view result;

    if (auto error = obj["buffer"].get(result.buffer))
    {
        return error;
    }

    if (auto error = obj["byteLength"].get(result.byte_length))
    {
        return error;
    }

    if (obj["byteOffset"].get(result.byte_offset))
    {
        result.byte_offset = 0;
    }

    if (obj["byteStride"].get(result.byte_stride))
    {
        result.byte_stride = 0;
    }
    else if (result.byte_stride < 4 || result.byte_stride > 252)
    {
        ::logger->error("Invalid byte stride: {}", result.byte_stride);
        return simdjson::error_code::INDEX_OUT_OF_BOUNDS;
    }

    if (auto error = obj["target"].get(result.target))
    {
        return error;
    }

    // Optional fields
    obj["name"].get(result.name);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::image> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::image result;

    if (auto error = obj["uri"].get(result.uri))
    {
        return error;
    }

    // Optional fields
    obj["mimeType"].get(result.mime_type);
    obj["bufferView"].get(result.buffer_view);
    obj["name"].get(result.name);

    if (result.uri.empty() && !result.buffer_view)
    {
        ::logger->error("Image has no URI or buffer view");
        return simdjson::error_code::INDEX_OUT_OF_BOUNDS;
    }

    if (!result.uri.empty() && result.buffer_view)
    {
        ::logger->error("Image has both URI and buffer view");
        return simdjson::error_code::INDEX_OUT_OF_BOUNDS;
    }

    if (result.buffer_view && result.mime_type.empty())
    {
        ::logger->error("Image has buffer view but no MIME type");
        return simdjson::error_code::INDEX_OUT_OF_BOUNDS;
    }

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::pbr_metallic_roughness> simdjson::ondemand::value::
    get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::pbr_metallic_roughness result;

    obj["baseColorFactor"].get(result.base_color_factor);
    obj["metallicFactor"].get(result.metallic_factor);
    obj["roughnessFactor"].get(result.roughness_factor);

    obj["baseColorTexture"].get(result.base_color_texture);
    obj["metallicRoughnessTexture"].get(result.metallic_roughness_texture);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::texture_info> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::texture_info result;

    if (auto error = obj["index"].get(result.index))
    {
        return error;
    }

    obj["texCoord"].get(result.tex_coord);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::normal_texture_info> simdjson::ondemand::value::
    get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::normal_texture_info result;

    if (auto error = obj["index"].get(result.index))
    {
        return error;
    }

    obj["texCoord"].get(result.tex_coord);
    obj["scale"].get(result.scale);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::occlusion_texture_info> simdjson::ondemand::value::
    get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::occlusion_texture_info result;

    if (auto error = obj["index"].get(result.index))
    {
        return error;
    }

    obj["texCoord"].get(result.tex_coord);
    obj["strength"].get(result.strength);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::material> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::material result;

    // Optional fields
    obj["name"].get(result.name);
    obj["pbrMetallicRoughness"].get(result.pbr_metallic_roughness);
    obj["normalTexture"].get(result.normal_texture);
    obj["occlusionTexture"].get(result.occlusion_texture);
    obj["emissiveTexture"].get(result.emissive_texture);
    obj["emissiveFactor"].get(result.emissive_factor);
    obj["alphaMode"].get(result.alpha_mode);
    obj["alphaCutoff"].get(result.alpha_cutoff);
    obj["doubleSided"].get(result.double_sided);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::mesh_primitive> simdjson::ondemand::value::
    get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::mesh_primitive result;

    simdjson::ondemand::object attributes = obj["attributes"];

    for (auto field : attributes)
    {
        auto name = std::string_view(field.unescaped_key());

        tempest::assets::gltf::mesh_primitive_attribute attr{.name = tempest::string(name.data(), name.size())};

        if (auto error = field.value().get(attr.accessor))
        {
            return error;
        }

        result.attributes.push_back(tempest::move(attr));
    }

    // Optional fields
    obj["indices"].get(result.indices);
    obj["material"].get(result.material);
    obj["mode"].get(result.mode);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::mesh> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::mesh result;

    // Optional fields
    obj["name"].get(result.name);

    obj["primitives"].get(result.primitives);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::node> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::node result;

    // Optional fields
    obj["name"].get(result.name);
    obj["children"].get(result.children);
    obj["matrix"].get(result.matrix);
    obj["mesh"].get(result.mesh);
    obj["rotation"].get(result.rotation);
    obj["scale"].get(result.scale);
    obj["translation"].get(result.translation);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::sampler> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::sampler result;

    // Optional fields
    obj["magFilter"].get(result.mag);
    obj["minFilter"].get(result.min);
    obj["wrapS"].get(result.wrap_s);
    obj["wrapT"].get(result.wrap_t);
    obj["name"].get(result.name);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::scene> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::scene result;

    // Optional fields
    obj["name"].get(result.name);
    obj["nodes"].get(result.nodes);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::texture> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::texture result;

    obj["sampler"].get(result.sampler);

    if (auto error = obj["source"].get(result.source))
    {
        ::logger->error("Texture has no source. The source field is required.");
        return error;
    }

    // Optional fields
    obj["name"].get(result.name);

    return result;
}

template <>
simdjson_inline simdjson::simdjson_result<tempest::assets::gltf::gltf> simdjson::ondemand::value::get() noexcept
{
    ondemand::object obj;

    if (auto error = get_object().get(obj))
    {
        return error;
    }

    tempest::assets::gltf::gltf result;

    if (auto error = obj["asset"].get(result.asset))
    {
        return error;
    }

    // Optional fields
    obj["accessors"].get(result.accessors);
    obj["buffers"].get(result.buffers);
    obj["bufferViews"].get(result.buffer_views);
    obj["images"].get(result.images);
    obj["materials"].get(result.materials);
    obj["meshes"].get(result.meshes);
    obj["nodes"].get(result.nodes);
    obj["samplers"].get(result.samplers);
    obj["scenes"].get(result.scenes);
    obj["textures"].get(result.textures);

    // Extensions
    obj["extensionsUsed"].get(result.extensions_used);
    obj["extensionsRequired"].get(result.extensions_required);

    return result;
}

namespace tempest::assets
{
    enum class accesssor_data_format
    {
        INVALID,
        SCALAR,
        VEC2,
        VEC3,
        VEC4,
        MAT2,
        MAT3,
        MAT4
    };

    enum class accessor_data_type
    {
        INVALID,
        BYTE,
        UNSIGNED_BYTE,
        SHORT,
        UNSIGNED_SHORT,
        UNSIGNED_INT,
        FLOAT
    };

    struct accessor_result
    {
        vector<byte> data;
        accesssor_data_format format;
        accessor_data_type type;
    };

    optional<accessor_result> read_accessor(const gltf::gltf& root, const gltf::accessor& accessor,
                                            const flat_unordered_map<size_t, vector<byte>>& buffer_data)
    {
        if (!accessor.buffer_view)
        {
            ::logger->error("Sparse accessors are not supported");
            return none();
        }

        if (*accessor.buffer_view >= root.buffer_views.size())
        {
            ::logger->error("Accessor buffer view out of bounds: {}", accessor.buffer_view.value());
            return none();
        }

        const auto& buffer_view = root.buffer_views[*accessor.buffer_view];
        if (buffer_view.buffer >= root.buffers.size())
        {
            ::logger->error("Buffer view buffer out of bounds: {}", buffer_view.buffer);
            return none();
        }

        // Get a reference to the data buffer
        const auto& source_vec_iter = buffer_data.find(buffer_view.buffer);
        if (source_vec_iter == buffer_data.end())
        {
            ::logger->error("Buffer not found: {}", buffer_view.buffer);
            return none();
        }

        const auto& source_vec = source_vec_iter->second;

        // Calculate the data format
        const accesssor_data_format format = [&]() {
            if (accessor.type == "SCALAR")
            {
                return accesssor_data_format::SCALAR;
            }
            else if (accessor.type == "VEC2")
            {
                return accesssor_data_format::VEC2;
            }
            else if (accessor.type == "VEC3")
            {
                return accesssor_data_format::VEC3;
            }
            else if (accessor.type == "VEC4")
            {
                return accesssor_data_format::VEC4;
            }
            else if (accessor.type == "MAT2")
            {
                return accesssor_data_format::MAT2;
            }
            else if (accessor.type == "MAT3")
            {
                return accesssor_data_format::MAT3;
            }
            else if (accessor.type == "MAT4")
            {
                return accesssor_data_format::MAT4;
            }
            else
            {
                ::logger->error("Invalid accessor type: {}", accessor.type.c_str());
                return accesssor_data_format::INVALID;
            }
        }();

        // Calculate the data type
        const accessor_data_type type = [&]() {
            switch (accessor.comp_type)
            {
            case gltf::component_type::BYTE:
                return accessor_data_type::BYTE;
            case gltf::component_type::UNSIGNED_BYTE:
                return accessor_data_type::UNSIGNED_BYTE;
            case gltf::component_type::SHORT:
                return accessor_data_type::SHORT;
            case gltf::component_type::UNSIGNED_SHORT:
                return accessor_data_type::UNSIGNED_SHORT;
            case gltf::component_type::UNSIGNED_INT:
                return accessor_data_type::UNSIGNED_INT;
            case gltf::component_type::FLOAT:
                return accessor_data_type::FLOAT;
            default:
                ::logger->error("Invalid accessor component type: {}", tempest::to_underlying(accessor.comp_type));
                return accessor_data_type::INVALID;
            }
        }();

        // Calculate the data size
        const size_t data_size = [&]() {
            switch (type)
            {
            case accessor_data_type::BYTE:
            case accessor_data_type::UNSIGNED_BYTE:
                return 1;
            case accessor_data_type::SHORT:
            case accessor_data_type::UNSIGNED_SHORT:
                return 2;
            case accessor_data_type::UNSIGNED_INT:
            case accessor_data_type::FLOAT:
                return 4;
            default:
                return 0;
            }
        }();

        // Calculate the data stride
        const size_t data_stride = [&]() {
            if (buffer_view.byte_stride)
            {
                return buffer_view.byte_stride;
            }
            else
            {
                return data_size * [&]() {
                    switch (format)
                    {
                    case accesssor_data_format::SCALAR:
                        return 1;
                    case accesssor_data_format::VEC2:
                        return 2;
                    case accesssor_data_format::VEC3:
                        return 3;
                    case accesssor_data_format::VEC4:
                        return 4;
                    case accesssor_data_format::MAT2:
                        return 4;
                    case accesssor_data_format::MAT3:
                        return 9;
                    case accesssor_data_format::MAT4:
                        return 16;
                    default:
                        return 0;
                    }
                }();
            }
        }();

        // Calculate the data offset
        const size_t data_offset = buffer_view.byte_offset;

        // Calculate the data count
        const size_t data_count = accessor.count;

        // Calculate the data size
        const size_t data_size_total = data_size * [&]() {
            switch (format)
            {
            case accesssor_data_format::SCALAR:
                return 1;
            case accesssor_data_format::VEC2:
                return 2;
            case accesssor_data_format::VEC3:
                return 3;
            case accesssor_data_format::VEC4:
                return 4;
            case accesssor_data_format::MAT2:
                return 4;
            case accesssor_data_format::MAT3:
                return 9;
            case accesssor_data_format::MAT4:
                return 16;
            default:
                return 0;
            }
        }();

        // Copy the data
        vector<byte> data(buffer_view.byte_length);

        for (size_t i = 0; i < data_count; i++)
        {
            const size_t source_offset = data_offset + i * data_stride;
            const size_t dest_offset = i * data_size_total;

            std::memcpy(data.data() + dest_offset, source_vec.data() + source_offset, data_size_total);
        }

        return accessor_result{tempest::move(data), format, type};
    }

    sampler_filter to_sampler_filter(gltf::mag_filter filter)
    {
        switch (filter)
        {
        case gltf::mag_filter::NEAREST:
            return sampler_filter::NEAREST;
        case gltf::mag_filter::LINEAR:
            return sampler_filter::LINEAR;
        default:
            return sampler_filter::NEAREST;
        }
    }

    sampler_filter to_sampler_filter(gltf::min_filter filter)
    {
        switch (filter)
        {
        case gltf::min_filter::NEAREST:
            return sampler_filter::NEAREST;
        case gltf::min_filter::LINEAR:
            return sampler_filter::LINEAR;
        case gltf::min_filter::NEAREST_MIPMAP_NEAREST:
            return sampler_filter::NEAREST_MIPMAP_NEAREST;
        case gltf::min_filter::LINEAR_MIPMAP_NEAREST:
            return sampler_filter::LINEAR_MIPMAP_NEAREST;
        case gltf::min_filter::NEAREST_MIPMAP_LINEAR:
            return sampler_filter::NEAREST_MIPMAP_LINEAR;
        case gltf::min_filter::LINEAR_MIPMAP_LINEAR:
            return sampler_filter::LINEAR_MIPMAP_LINEAR;
        default:
            return sampler_filter::NEAREST;
        }
    }

    sampler_wrap to_sampler_wrap(gltf::wrap_mode mode)
    {
        switch (mode)
        {
        case gltf::wrap_mode::REPEAT:
            return sampler_wrap::REPEAT;
        case gltf::wrap_mode::MIRRORED_REPEAT:
            return sampler_wrap::MIRRORED_REPEAT;
        case gltf::wrap_mode::CLAMP_TO_EDGE:
            return sampler_wrap::CLAMP_TO_EDGE;
        default:
            return sampler_wrap::REPEAT;
        }
    }

    flat_unordered_map<size_t, guid> load_materials(asset_import_context& context, const gltf::gltf& gltf_root,
                                                    const flat_unordered_map<size_t, guid>& textures)
    {
        flat_unordered_map<size_t, guid> material_guids;

        for (const auto& material : gltf_root.materials)
        {
            auto mat = make_unique<assets::material>(material.name);
            if (material.pbr_metallic_roughness)
            {
                if (material.pbr_metallic_roughness->base_color_texture)
                {
                    auto [_, id] = *textures.find(material.pbr_metallic_roughness->base_color_texture->index);
                    mat->add_texture(assets::material::base_color, id);
                }

                if (material.pbr_metallic_roughness->metallic_roughness_texture)
                {
                    auto [_, id] = *textures.find(material.pbr_metallic_roughness->metallic_roughness_texture->index);
                    mat->add_texture(assets::material::metallic_roughness, id);
                }

                mat->add_vec4(assets::material::base_color_factor,
                              math::vec4<float>(material.pbr_metallic_roughness->base_color_factor[0],
                                                material.pbr_metallic_roughness->base_color_factor[1],
                                                material.pbr_metallic_roughness->base_color_factor[2],
                                                material.pbr_metallic_roughness->base_color_factor[3]));
                mat->add_float(assets::material::metallic_factor, material.pbr_metallic_roughness->metallic_factor);
                mat->add_float(assets::material::roughness_factor, material.pbr_metallic_roughness->roughness_factor);
            }

            if (material.normal_texture)
            {
                auto [_, id] = *textures.find(material.normal_texture->index);
                mat->add_texture(assets::material::normal, id);

                mat->add_float(assets::material::normal_scale, material.normal_texture->scale);
            }

            if (material.occlusion_texture)
            {
                auto [_, id] = *textures.find(material.occlusion_texture->index);
                mat->add_texture(assets::material::occlusion, id);

                mat->add_float(assets::material::occlusion_strength, material.occlusion_texture->strength);
            }

            if (material.emissive_texture)
            {
                auto [_, id] = *textures.find(material.emissive_texture->index);
                mat->add_texture(assets::material::emissive, id);
            }

            mat->add_vec3(assets::material::emissive_factor,
                          math::vec3<float>(material.emissive_factor[0], material.emissive_factor[1],
                                            material.emissive_factor[2]));

            mat->add_float(assets::material::alpha_cutoff, material.alpha_cutoff);
            mat->add_string(assets::material::alpha_mode, material.alpha_mode);
            mat->add_bool(assets::material::double_sided, material.double_sided);

            auto idx = material_guids.size();
            material_guids[idx] = mat->id();

            context.add_asset(tempest::move(mat));
        }

        return material_guids;
    }

    flat_unordered_map<size_t, guid> load_meshes(asset_import_context& context, const gltf::gltf& gltf_root,
                                                 const flat_unordered_map<size_t, vector<byte>>& buffer_data,
                                                 const flat_unordered_map<size_t, guid>& materials,
                                                 flat_unordered_map<size_t, size_t>& mesh_primitive_start_index)
    {
        flat_unordered_map<size_t /* mesh primitive index */, guid> mesh_guids;
        size_t mesh_primitive_index = 0;
        size_t mesh_index = 0;

        for (const auto& mesh : gltf_root.meshes)
        {
            auto group = make_unique<mesh_group>(mesh.name);

            mesh_primitive_start_index[mesh_index] = mesh_primitive_index;

            for (const auto& primitive : mesh.primitives)
            {
                auto mesh_asset = make_unique<assets::mesh>(mesh.name);
                group->meshes().push_back(mesh_asset->id());

                for (const auto& attr : primitive.attributes)
                {
                    auto accessor = gltf_root.accessors[attr.accessor];
                    auto accessor_data_opt = read_accessor(gltf_root, accessor, buffer_data);

                    if (!accessor_data_opt)
                    {
                        ::logger->error("Failed to read accessor data");
                        continue;
                    }

                    auto& accessor_data = accessor_data_opt.value();

                    if (attr.name == "POSITION")
                    {
                        assert(accessor_data.format == accesssor_data_format::VEC3);
                        assert(accessor_data.type == accessor_data_type::FLOAT);

                        for (size_t i = 0; i < accessor_data.data.size(); i += sizeof(float) * 3)
                        {
                            mesh::position pos;
                            std::memcpy(&pos, accessor_data.data.data() + i, sizeof(float) * 3);
                            mesh_asset->positions().push_back(pos);
                        }

                        // Fetch the bounding box
                        if (accessor.min.size() != 3 || accessor.max.size() != 3)
                        {
                            ::logger->warn("Position accessor has no min/max bounds");
                            continue;
                        }

                        mesh_asset->min_bounds() = {
                            static_cast<float>(accessor.min[0]),
                            static_cast<float>(accessor.min[1]),
                            static_cast<float>(accessor.min[2]),
                        };

                        mesh_asset->max_bounds() = {
                            static_cast<float>(accessor.max[0]),
                            static_cast<float>(accessor.max[1]),
                            static_cast<float>(accessor.max[2]),
                        };
                    }
                    else if (attr.name == "TEXCOORD_0")
                    {
                        assert(accessor_data.format == accesssor_data_format::VEC2);
                        assert(accessor_data.type == accessor_data_type::FLOAT);

                        for (size_t i = 0; i < accessor_data.data.size(); i += sizeof(float) * 2)
                        {
                            mesh::uv uv;
                            std::memcpy(&uv, accessor_data.data.data() + i, sizeof(float) * 2);
                            mesh_asset->uvs().push_back(uv);
                        }
                    }
                    else if (attr.name == "NORMAL")
                    {
                        assert(accessor_data.format == accesssor_data_format::VEC3);
                        assert(accessor_data.type == accessor_data_type::FLOAT);

                        for (size_t i = 0; i < accessor_data.data.size(); i += sizeof(float) * 3)
                        {
                            mesh::normal normal;
                            std::memcpy(&normal, accessor_data.data.data() + i, sizeof(float) * 3);
                            mesh_asset->normals().push_back(normal);
                        }
                    }
                    else if (attr.name == "TANGENT")
                    {
                        assert(accessor_data.format == accesssor_data_format::VEC4);
                        assert(accessor_data.type == accessor_data_type::FLOAT);

                        for (size_t i = 0; i < accessor_data.data.size(); i += sizeof(float) * 4)
                        {
                            mesh::tangent tangent;
                            std::memcpy(&tangent, accessor_data.data.data() + i, sizeof(float) * 4);
                            mesh_asset->tangents().push_back(tangent);
                        }
                    }
                    else if (attr.name == "COLOR_0")
                    {
                        assert(accessor_data.format == accesssor_data_format::VEC4);
                        assert(accessor_data.type == accessor_data_type::FLOAT);

                        for (size_t i = 0; i < accessor_data.data.size(); i += sizeof(float) * 4)
                        {
                            mesh::color color;
                            std::memcpy(&color, accessor_data.data.data() + i, sizeof(float) * 4);
                            mesh_asset->colors().push_back(color);
                        }
                    }
                    else
                    {
                        ::logger->warn("Unknown attribute: {}", attr.name.c_str());
                    }
                }

                if (primitive.indices)
                {
                    auto accessor = gltf_root.accessors[*primitive.indices];
                    auto accessor_data_opt = read_accessor(gltf_root, accessor, buffer_data);

                    if (accessor_data_opt)
                    {
                        auto& accessor_data = accessor_data_opt.value();
                        assert(accessor_data.format == accesssor_data_format::SCALAR);

                        if (accessor_data.type == accessor_data_type::UNSIGNED_INT)
                        {
                            mesh_asset->indices() = std::move(accessor_data.data.reinterpret_as<mesh::index>());
                        }
                        else if (accessor_data.type == accessor_data_type::UNSIGNED_BYTE)
                        {
                            vector<mesh::index> indices(accessor_data.data.size());
                            for (size_t i = 0; i < indices.size(); i++)
                            {
                                indices[i] = static_cast<mesh::index>(accessor_data.data[i]);
                            }
                            mesh_asset->indices() = tempest::move(indices);
                        }
                        else if (accessor_data.type == accessor_data_type::UNSIGNED_SHORT)
                        {
                            vector<mesh::index> indices(accessor_data.data.size() / sizeof(uint16_t));
                            for (size_t i = 0; i < indices.size(); i++)
                            {
                                indices[i] = static_cast<mesh::index>(accessor_data.data[i]);
                            }
                            mesh_asset->indices() = tempest::move(indices);
                        }
                        else
                        {
                            ::logger->error("Invalid index type: {}", tempest::to_underlying(accessor_data.type));
                        }
                    }
                }

                if (primitive.material)
                {
                    const auto mat_it = materials.find(*primitive.material);
                    if (mat_it != materials.end())
                    {
                        mesh_asset->set_material(mat_it->second);
                    }
                }

                mesh_guids.insert({mesh_primitive_index, mesh_asset->id()});
                context.add_asset(tempest::move(mesh_asset));

                mesh_primitive_index++;
            }

            context.add_asset(tempest::move(group));
            mesh_index++;
        }

        return mesh_guids;
    }

    flat_unordered_map<size_t, guid> load_textures(asset_import_context& context, const gltf::gltf& gltf_root,
                                                   const std::filesystem::path& path)
    {
        // Check image usages to determine light transport
        flat_unordered_map<size_t, bool> image_usage_srgb;
        for (const auto& mat : gltf_root.materials)
        {
            if (mat.pbr_metallic_roughness)
            {
                if (mat.pbr_metallic_roughness->base_color_texture)
                {
                    image_usage_srgb[mat.pbr_metallic_roughness->base_color_texture->index] = true;
                }

                if (mat.pbr_metallic_roughness->metallic_roughness_texture)
                {
                    image_usage_srgb[mat.pbr_metallic_roughness->metallic_roughness_texture->index] = false;
                }
            }

            if (mat.normal_texture)
            {
                image_usage_srgb[mat.normal_texture->index] = false;
            }

            if (mat.occlusion_texture)
            {
                image_usage_srgb[mat.occlusion_texture->index] = false;
            }

            if (mat.emissive_texture)
            {
                image_usage_srgb[mat.emissive_texture->index] = true;
            }
        }

        flat_unordered_map<size_t, guid> texture_guids;
        size_t texture_index = 0;

        for (const auto& texture : gltf_root.textures)
        {
            const auto& image = gltf_root.images[texture.source];

            texture::sampler_state sampler_state = [&]() {
                if (texture.sampler)
                {
                    const auto& sampler = gltf_root.samplers[*texture.sampler];
                    return texture::sampler_state{
                        .min_filter = to_sampler_filter(sampler.min),
                        .mag_filter = to_sampler_filter(sampler.mag),
                        .wrap_s = to_sampler_wrap(sampler.wrap_s),
                        .wrap_t = to_sampler_wrap(sampler.wrap_t),
                    };
                }
                else
                {
                    return texture::sampler_state{
                        .min_filter = sampler_filter::LINEAR,
                        .mag_filter = sampler_filter::LINEAR,
                        .wrap_s = sampler_wrap::REPEAT,
                        .wrap_t = sampler_wrap::REPEAT,
                    };
                }
            }();

            auto texture_asset = make_unique<assets::texture>(image.name);
            texture_asset->sampler(sampler_state);

            // Load the image
            if (image.uri.empty())
            {
                ::logger->error("Image has no URI");
                continue;
            }

            std::filesystem::path image_path = path / image.uri.data();
            if (!std::filesystem::exists(image_path))
            {
                ::logger->error("Image file not found: {}", image_path.string());
                continue;
            }

            auto data = vector<byte>{};
            std::ifstream file(image_path, std::ios::binary);
            if (file.is_open())
            {
                file.seekg(0, std::ios::end);
                data.resize(file.tellg());
                file.seekg(0, std::ios::beg);
                file.read(reinterpret_cast<char*>(data.data()), data.size());
                file.close();
            }
            else
            {
                ::logger->error("Failed to open image file: {}", image_path.string());
                continue;
            }

            int width, height, components;
            bool is_16_bit = stbi_is_16_bit_from_memory(reinterpret_cast<const stbi_uc*>(data.data()),
                                                        static_cast<int>(data.size()));

            if (is_16_bit)
            {
                auto shorts = stbi_load_16_from_memory(reinterpret_cast<const stbi_uc*>(data.data()),
                                                       static_cast<int>(data.size()), &width, &height, &components, 4);
                auto fmt = image_usage_srgb[texture_index] ? texture_format::RGBA16_SRGB : texture_format::RGBA16_UINT;

                tempest::vector<byte> tex_data(width * height * 4 * 2);
                std::memcpy(tex_data.data(), shorts, tex_data.size());
                stbi_image_free(shorts);

                texture_asset->set_mip_data(0, std::move(tex_data));
                texture_asset->format(fmt);
                texture_asset->width(width);
                texture_asset->height(height);
            }
            else
            {
                auto bytes = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data.data()),
                                                   static_cast<int>(data.size()), &width, &height, &components, 4);
                auto fmt = image_usage_srgb[texture_index] ? texture_format::RGBA8_SRGB : texture_format::RGBA8_UINT;

                vector<byte> tex_data(width * height * 4);
                std::memcpy(tex_data.data(), bytes, tex_data.size());
                texture_asset->set_mip_data(0, tempest::move(tex_data));
                texture_asset->format(fmt);
                texture_asset->width(width);
                texture_asset->height(height);

                stbi_image_free(bytes);
            }

            texture_guids.insert({texture_index, texture_asset->id()});
            context.add_asset(tempest::move(texture_asset));

            texture_index++;
        }

        return texture_guids;
    }

    void load_nodes(asset_import_context& context, const gltf::gltf& root,
                    const flat_unordered_map<size_t, guid>& meshes, const flat_unordered_map<size_t, guid>& materials,
                    const flat_unordered_map<size_t, size_t>& mesh_primitive_start_index)
    {
        for (const auto& node : root.nodes)
        {
            auto n = make_unique<prefab_node>(node.name);
            if (node.mesh)
            {
                auto mesh_idx = *node.mesh;
                auto mesh_prim_start = mesh_primitive_start_index.find(mesh_idx)->second;
                size_t primitive_idx = 0;
                for (const auto& primitive : root.meshes[mesh_idx].primitives)
                {
                    auto child = make_unique<prefab_node>(root.meshes[mesh_idx].name);

                    auto mesh_guid = meshes.find(mesh_prim_start + primitive_idx);
                    child->children().push_back(mesh_guid->second);

                    if (primitive.material)
                    {
                        auto mat_guid = materials.find(*primitive.material);
                        child->children().push_back(mat_guid->second);
                    }

                    n->children().push_back(child->id());
                    context.add_asset(tempest::move(child));

                    primitive_idx++;
                }
            }

            context.add_asset(tempest::move(n));
        }
    }

    void gltf_importer::on_asset_load(asset_import_context& context)
    {
        auto data = context.data();
        auto padded = simdjson::padded_string(reinterpret_cast<const char*>(data.data()), data.size());

        simdjson::ondemand::parser parser;
        simdjson::ondemand::document doc = parser.iterate(padded);

        gltf::gltf gltf_root;
        if (auto error = doc.get_value().get(gltf_root))
        {
            return;
        }

        auto t_path = context.path();
        std::filesystem::path path = t_path.data();
        path.remove_filename();

        flat_unordered_map<size_t /* buffer id */, vector<byte>> buffer_data;
        size_t buffer_id = 0;
        for (const auto& buffer : gltf_root.buffers)
        {
            if (buffer.uri.empty())
            {
                ::logger->error("Buffer has no URI");
                buffer_id++;
                continue;
            }

            std::filesystem::path buffer_path = path / buffer.uri.data();

            if (std::filesystem::exists(buffer_path))
            {
                // Load from file
                auto data = vector<byte>{};
                std::ifstream file(buffer_path, std::ios::binary);
                if (file.is_open())
                {
                    file.seekg(0, std::ios::end);
                    data.resize(file.tellg());
                    file.seekg(0, std::ios::beg);
                    file.read(reinterpret_cast<char*>(data.data()), data.size());
                    file.close();
                }
                else
                {
                    ::logger->error("Failed to open buffer file: {}", buffer_path.string());
                    continue;
                }

                buffer_data.insert({buffer_id, tempest::move(data)});
            }
            else
            {
                // Load data from URI
                ::logger->error("Buffer URI not supported");
            }

            buffer_id++;
        }

        flat_unordered_map<size_t, guid> texture_guids = load_textures(context, gltf_root, path);
        flat_unordered_map<size_t, guid> material_guids = load_materials(context, gltf_root, texture_guids);
        flat_unordered_map<size_t, size_t> mesh_primitive_start_index;
        flat_unordered_map<size_t, guid> mesh_guids =
            load_meshes(context, gltf_root, buffer_data, material_guids, mesh_primitive_start_index);
        load_nodes(context, gltf_root, mesh_guids, material_guids, mesh_primitive_start_index);

        context.set_name(std::filesystem::path(context.path().data()).stem().string().c_str());
    }
} // namespace tempest::assets