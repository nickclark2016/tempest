#include <tempest/render_device.hpp>

#include "vk/vk_render_device.hpp"

namespace tempest::graphics
{
    std::unique_ptr<render_context> render_context::create(abstract_allocator* alloc)
    {
        return std::make_unique<vk::render_context>(alloc);
    }

    render_context::render_context(abstract_allocator* alloc) : _alloc{alloc}
    {
    }

    vector<image_resource_handle> renderer_utilities::upload_textures(render_device& dev,
                                                                      span<texture_data_descriptor> textures,
                                                                      buffer_resource_handle staging_buffer,
                                                                      bool use_entire_buffer, bool generate_mip_maps)
    {
        vector<image_resource_handle> images;

        auto& cmd_executor = dev.get_command_executor();
        auto* cmds = &cmd_executor.get_commands();

        // first, create image for each texture and transition image to transfer destination
        for (texture_data_descriptor& tex_dat : textures)
        {
            auto mip_count = generate_mip_maps
                                 ? std::bit_width(std::min(tex_dat.mips[0].width, tex_dat.mips[0].height)) - 1
                                 : static_cast<std::uint32_t>(tex_dat.mips.size());

            image_create_info ci = {
                .type = image_type::IMAGE_2D,
                .width = tex_dat.mips[0].width,
                .height = tex_dat.mips[0].height,
                .depth = 1,
                .layers = 1,
                .mip_count = mip_count,
                .format = tex_dat.fmt,
                .samples = sample_count::COUNT_1,
                .transfer_source = true,
                .transfer_destination = true,
                .sampled = true,
                .name = tex_dat.name,
            };

            images.push_back(dev.create_image(ci));
            cmds->transition_image(images.back(), image_resource_usage::UNDEFINED,
                                   image_resource_usage::TRANSFER_DESTINATION);
        }

        auto staging_buffer_bytes =
            use_entire_buffer ? dev.map_buffer(staging_buffer) : dev.map_buffer_frame(staging_buffer);

        std::size_t staging_buffer_size = staging_buffer_bytes.size_bytes();
        std::size_t staging_buffer_bytes_written = 0;

        std::size_t image_index = 0;
        std::size_t global_staging_buffer_offset = dev.get_buffer_frame_offset(staging_buffer);

        for (texture_data_descriptor& tex_data : textures)
        {
            std::uint32_t mip_index = 0;

            for (texture_mip_descriptor& mip_data : tex_data.mips)
            {
                std::size_t mip_size = mip_data.bytes.size_bytes();
                std::size_t mip_bytes_written = 0;
                std::size_t row_width_in_bytes = mip_data.width * bytes_per_element(tex_data.fmt);
                std::size_t row_index = 0;

                while (mip_bytes_written < mip_size)
                {
                    std::size_t bytes_available = staging_buffer_size - staging_buffer_bytes_written;
                    std::size_t bytes_to_write = std::min(mip_size - mip_bytes_written, bytes_available);
                    std::size_t row_count = bytes_to_write / row_width_in_bytes;

                    if (bytes_available < row_width_in_bytes)
                    {
                        // submit the commands and wait
                        cmd_executor.submit_and_wait();
                        cmds = &cmd_executor.get_commands();

                        staging_buffer_bytes_written = 0;
                        continue;
                    }

                    std::size_t buffer_offset = global_staging_buffer_offset + staging_buffer_bytes_written;

                    std::memcpy(staging_buffer_bytes.data() + staging_buffer_bytes_written,
                                mip_data.bytes.data() + mip_bytes_written, bytes_to_write);
                    cmds->copy(staging_buffer, images[image_index], buffer_offset, mip_data.width,
                               static_cast<std::uint32_t>(row_count), mip_index, 0, 0);

                    row_index += row_count;
                    mip_bytes_written += bytes_to_write;
                    staging_buffer_bytes_written += bytes_to_write;
                }

                mip_index += 1;
            }

            image_index += 1;
        }

        if (generate_mip_maps)
        {
            for (std::size_t i = 0; i < images.size(); ++i)
            {
                auto& tex_dat = textures[i];
                auto handle = images[i];

                auto mip_count = generate_mip_maps
                                     ? std::bit_width(std::min(tex_dat.mips[0].width, tex_dat.mips[0].height)) - 1
                                     : static_cast<std::uint32_t>(tex_dat.mips.size());

                cmds->generate_mip_chain(handle, image_resource_usage::TRANSFER_DESTINATION, 0, mip_count);
            }
        }

        for (image_resource_handle img : images)
        {
            cmds->transition_image(img, image_resource_usage::TRANSFER_DESTINATION, image_resource_usage::SAMPLED);
        }

        cmd_executor.submit_and_wait();

        dev.unmap_buffer(staging_buffer);

        return images;
    }

    vector<mesh_layout> renderer_utilities::upload_meshes(render_device& device, span<core::mesh> meshes,
                                                          buffer_resource_handle target, std::uint32_t& offset)
    {
        std::size_t bytes_written = offset;
        std::size_t staging_buffer_bytes_written = 0;
        std::size_t last_write_index = offset;
        vector<graphics::mesh_layout> result;
        result.reserve(meshes.size());

        auto staging_buffer = device.get_staging_buffer();
        auto staging_buffer_ptr = device.map_buffer(staging_buffer);
        auto dst = staging_buffer_ptr.data();

        auto& executor = device.get_command_executor();

        for (auto& mesh : meshes)
        {
            if (!mesh.has_tangents)
            {
                mesh.compute_tangents();
            }

            graphics::mesh_layout layout = {
                .mesh_start_offset = static_cast<std::uint32_t>(bytes_written),
                .positions_offset = 0,
                .interleave_offset = 3 * static_cast<std::uint32_t>(sizeof(float) * mesh.vertices.size()),
                .uvs_offset = 0,
                .normals_offset = static_cast<std::uint32_t>(2 * sizeof(float)),
            };

            std::uint32_t last_offset = 5 * sizeof(float);

            if (mesh.has_tangents)
            {
                layout.tangents_offset = last_offset;
                last_offset += static_cast<std::uint32_t>(4 * sizeof(float));
            }

            if (mesh.has_colors)
            {
                layout.color_offset = last_offset;
                last_offset += static_cast<std::uint32_t>(4 * sizeof(float));
            }

            layout.interleave_stride = last_offset;
            layout.index_offset =
                layout.interleave_offset + layout.interleave_stride * static_cast<std::uint32_t>(mesh.vertices.size());
            layout.index_count = static_cast<std::uint32_t>(mesh.indices.size());

            result.push_back(layout);

            // upload the data

            if (staging_buffer_bytes_written + mesh.vertices.size() * 3 * sizeof(float) > staging_buffer_ptr.size())
            {
                auto& cmds = executor.get_commands();
                cmds.copy(staging_buffer, target, 0, last_write_index, staging_buffer_bytes_written);
                executor.submit_and_wait();
                staging_buffer_bytes_written = 0;
                last_write_index = bytes_written;
            }

            std::size_t vertices_written = 0;
            for (const auto& vertex : mesh.vertices)
            {
                std::memcpy(dst + staging_buffer_bytes_written + vertices_written * 3 * sizeof(float), &vertex.position,
                            3 * sizeof(float));

                ++vertices_written;
            }

            bytes_written += layout.interleave_offset;
            staging_buffer_bytes_written += layout.interleave_offset;

            if (bytes_written + mesh.vertices.size() * layout.interleave_stride > staging_buffer_ptr.size())
            {
                auto& cmds = executor.get_commands();
                cmds.copy(staging_buffer, target, 0, last_write_index, staging_buffer_bytes_written);
                executor.submit_and_wait();
                staging_buffer_bytes_written = 0;
                last_write_index = bytes_written;
            }

            vertices_written = 0;
            for (const auto& vertex : mesh.vertices)
            {
                std::memcpy(dst + staging_buffer_bytes_written + layout.uvs_offset +
                                vertices_written * layout.interleave_stride,
                            &vertex.uv, 2 * sizeof(float));
                std::memcpy(dst + staging_buffer_bytes_written + layout.normals_offset +
                                vertices_written * layout.interleave_stride,
                            &vertex.normal, 3 * sizeof(float));

                if (mesh.has_tangents)
                {
                    std::memcpy(dst + staging_buffer_bytes_written + layout.tangents_offset +
                                    vertices_written * layout.interleave_stride,
                                &vertex.tangent, 4 * sizeof(float));
                }

                if (mesh.has_colors)
                {
                    std::memcpy(dst + staging_buffer_bytes_written + layout.tangents_offset +
                                    vertices_written * layout.interleave_stride,
                                &vertex.tangent, 4 * sizeof(float));
                }

                ++vertices_written;
            }

            bytes_written += mesh.vertices.size() * layout.interleave_stride;
            staging_buffer_bytes_written += mesh.vertices.size() * layout.interleave_stride;

            if (staging_buffer_bytes_written + layout.index_count * sizeof(std::uint32_t) > staging_buffer_ptr.size())
            {
                auto& cmds = executor.get_commands();
                cmds.copy(staging_buffer, target, 0, last_write_index, staging_buffer_bytes_written);
                executor.submit_and_wait();
                staging_buffer_bytes_written = 0;
                last_write_index = bytes_written;
            }

            std::memcpy(dst + staging_buffer_bytes_written, mesh.indices.data(),
                        layout.index_count * sizeof(std::uint32_t));

            bytes_written += layout.index_count * sizeof(std::uint32_t);
            staging_buffer_bytes_written += layout.index_count * sizeof(std::uint32_t);
        }

        if (staging_buffer_bytes_written > 0)
        {
            auto& cmds = executor.get_commands();
            cmds.copy(staging_buffer, target, 0, last_write_index, staging_buffer_bytes_written);
            executor.submit_and_wait();
            staging_buffer_bytes_written = 0;
        }

        device.unmap_buffer(staging_buffer);

        offset = static_cast<std::uint32_t>(bytes_written);

        return result;
    }

    staging_buffer_writer::staging_buffer_writer(render_device& dev)
        : _dev{&dev}, _staging_buffer_offset{_dev->get_buffer_frame_offset(_dev->get_staging_buffer())},
          _staging_buffer{_dev->get_staging_buffer()}
    {
    }

    staging_buffer_writer::staging_buffer_writer(render_device& dev, buffer_resource_handle staging_buffer,
                                                 uint32_t staging_buffer_offset)
        : _dev{&dev}, _staging_buffer_offset{staging_buffer_offset}, _staging_buffer{staging_buffer}
    {
    }

    staging_buffer_writer& staging_buffer_writer::write(command_list& cmds, span<const byte> data,
                                                        buffer_resource_handle target, uint32_t write_offset)
    {
        if (_mapped_buffer.empty())
        {
            _mapped_buffer = _dev->map_buffer_frame(_staging_buffer);
        }

        const auto target_offset = _dev->get_buffer_frame_offset(target);

        std::size_t staging_buffer_write_offset = _staging_buffer_offset + _bytes_written;
        std::size_t bytes_to_write = data.size_bytes();

        if (bytes_to_write + _bytes_written > _mapped_buffer.size_bytes())
        {
            // TODO: log error
        }

        std::memcpy(_mapped_buffer.data() + _bytes_written, data.data(), bytes_to_write);
        cmds.copy(_staging_buffer, target, staging_buffer_write_offset, target_offset + write_offset, bytes_to_write);

        _bytes_written += bytes_to_write;

        return *this;
    }

    void staging_buffer_writer::finish()
    {
        _dev->unmap_buffer(_staging_buffer);
        _staging_buffer = {};
    }

    void staging_buffer_writer::reset(uint32_t staging_buffer_offset)
    {
        _staging_buffer_offset = staging_buffer_offset;
        _bytes_written = 0;

        finish();
    }

    void staging_buffer_writer::mark(size_t offset) noexcept
    {
        _bytes_written = offset;
    }
} // namespace tempest::graphics
