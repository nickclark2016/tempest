#ifndef tempest_graphics_types_hpp
#define tempest_graphics_types_hpp

#include <compare>
#include <cstddef>
#include <numeric>
#include <string>

namespace tempest::graphics
{
    enum class sample_count
    {
        COUNT_1 = 0b00001,
        COUNT_2 = 0b00010,
        COUNT_4 = 0b00100,
        COUNT_8 = 0b01000,
        COUNT_16 = 0b10000
    };

    enum class resource_format
    {
        RGBA8_SRGB,
        RG32_FLOAT,
        RG32_UINT,
        RGB32_FLOAT,
        RGBA32_FLOAT,
        D32_FLOAT,
    };

    enum class image_type
    {
        IMAGE_1D,
        IMAGE_2D,
        IMAGE_3D,
        IMAGE_CUBE_MAP,
        IMAGE_1D_ARRAY,
        IMAGE_2D_ARRAY,
        IMAGE_CUBE_MAP_ARRAY,
    };

    enum class shader_stage
    {
        NONE,
        VERTEX,
        FRAGMENT,
        COMPUTE
    };

    enum class buffer_resource_usage
    {
        STRUCTURED,
        RW_STRUCTURED,
        CONSTANT,
        VERTEX,
        INDEX,
        INDIRECT_ARGUMENT,
        TRANSFER_SOURCE,
        TRANSFER_DESTINATION,
    };

    enum class image_resource_usage
    {
        COLOR_ATTACHMENT,
        DEPTH_ATTACHMENT,
        SAMPLED,
        STORAGE,
        RW_STORAGE,
        TRANSFER_SOURCE,
        TRANSFER_DESTINATION,
        PRESENT,
    };

    enum class pipeline_stage
    {
        INFER,
        BEGIN,
        VERTEX,
        FRAGMENT,
        COLOR_OUTPUT,
        COMPUTE,
        TRANSFER,
        END,
    };

    enum class memory_location
    {
        DEVICE,
        HOST,
        AUTO,
    };

    struct image_desc
    {
        sample_count samples{sample_count::COUNT_1};
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t depth{1};
        std::uint32_t layers{1};
        resource_format fmt;
        image_type type;
        std::string_view name;
    };

    struct buffer_desc
    {
        std::size_t size;
        memory_location location{memory_location::AUTO};
    };

    struct buffer_create_info
    {
        memory_location loc;
        std::size_t size;
        bool transfer_source : 1;
        bool transfer_destination : 1;
        bool uniform_buffer : 1;
        bool storage_buffer : 1;
        bool index_buffer : 1;
        bool vertex_buffer : 1;
        bool indirect_buffer : 1;
        std::string_view name;
    };

    struct image_create_info
    {
        image_type type;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t depth;
        std::uint32_t layers;
        std::uint32_t mip_count;
        resource_format format;
        sample_count samples;
        bool transfer_source : 1;
        bool transfer_destination : 1;
        bool sampled : 1;
        bool storage : 1;
        bool color_attachment : 1;
        bool depth_attachment : 1;
        std::string name;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_types_hpp