#ifndef tempest_graphics_mesh_component_hpp
#define tempest_graphics_mesh_component_hpp

#include <cstdint>
#include <span>

namespace tempest::graphics
{
    struct mesh_component
    {
        std::uint32_t index_count;
        std::uint32_t first_index;
        std::int32_t vertex_offset;
    };

    class gpu_mesh
    {
      public:
        struct compressed_vertex_data_format
        {
            std::int16_t position_x, position_y, position_z;
            std::int16_t uv0_x, uv0_y;
            std::int16_t qtan_x, qtan_y, qtan_z, qtan_w;
        };

        struct naive_vertex_data_format
        {
            float position_x, position_y, position_z;
            float uv0_x, uv0_y;
            float normal_x, normal_y, normal_z;
            float tangent_x, tangent_y, tangent_z, tangent_w;
        };

        using vertex_format = naive_vertex_data_format;

        mesh_component append_mesh(std::span<vertex_format> vertices, std::span<std::byte> vertex_destination,
                                   std::span<std::uint32_t> indices, std::span<std::byte> index_destination);

      private:
        std::uint32_t _index_offset{0};
        std::int32_t _vertex_offset{0};

        struct mesh_write_result
        {
            std::size_t vertex_offset;
            std::size_t index_offset;
        };

        static std::size_t write_vertices(std::span<vertex_format> input_vertices, std::span<std::byte> destination,
                                          std::size_t offset);
        static std::size_t write_indices(std::span<std::uint32_t> indices, std::span<std::byte> destination,
                                         std::size_t offset);
        static mesh_write_result write_mesh(std::span<vertex_format> vertices, std::span<std::uint32_t> indices,
                                            std::span<std::byte> vertex_destination, std::size_t vertex_offset,
                                            std::span<std::byte> index_destination, std::size_t index_offset);
    };
} // namespace tempest::graphics

#endif // tempest_graphics_mesh_component_hpp