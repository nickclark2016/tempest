#include <tempest/mesh_component.hpp>

#include <cassert>

namespace tempest::graphics
{
    mesh_component gpu_mesh::append_mesh(std::span<vertex_format> vertices, std::span<std::byte> vertex_destination,
                                         std::span<std::uint32_t> indices, std::span<std::byte> index_destination)
    {
        mesh_component mesh{
            .index_count{static_cast<std::uint32_t>(indices.size())},
            .first_index{_index_offset},
            .vertex_offset{_vertex_offset},
        };

        return mesh;
    }

    std::size_t gpu_mesh::write_vertices(std::span<vertex_format> input_vertices, std::span<std::byte> destination,
                                         std::size_t offset)
    {
        const auto requested_write_size = input_vertices.size_bytes();
        const auto write_dest_size = destination.size_bytes() - offset;

        assert(requested_write_size >= write_dest_size);

        std::size_t write_offset = offset;
        for (const vertex_format& fmt : input_vertices)
        {
            std::byte* ptr = destination.data() + write_offset;
            std::memcpy(ptr, &fmt, sizeof(vertex_format));
        }

        return write_offset;
    }

    std::size_t gpu_mesh::write_indices(std::span<std::uint32_t> indices, std::span<std::byte> destination,
                                        std::size_t offset)
    {
        const auto requested_write_size = indices.size_bytes();
        const auto write_dest_size = destination.size_bytes() - offset;

        assert(requested_write_size >= write_dest_size);

        std::memcpy(destination.data() + offset, indices.data(), requested_write_size);

        return offset + requested_write_size;
    }

    gpu_mesh::mesh_write_result gpu_mesh::write_mesh(std::span<vertex_format> vertices,
                                                     std::span<std::uint32_t> indices,
                                                     std::span<std::byte> vertex_destination, std::size_t vertex_offset,
                                                     std::span<std::byte> index_destination, std::size_t index_offset)
    {
        const auto final_vertex_offset = write_vertices(vertices, vertex_destination, vertex_offset);
        const auto final_index_offset = write_indices(indices, index_destination, index_offset);

        return mesh_write_result{
            .vertex_offset{final_vertex_offset},
            .index_offset{final_index_offset},
        };
    }
} // namespace tempest::graphics