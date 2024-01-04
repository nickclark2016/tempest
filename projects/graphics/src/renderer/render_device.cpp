#include <tempest/render_device.hpp>

#include "vk/vk_render_device.hpp"

namespace tempest::graphics
{
    std::unique_ptr<render_context> render_context::create(core::allocator* alloc)
    {
        return std::make_unique<vk::render_context>(alloc);
    }

    render_context::render_context(core::allocator* alloc) : _alloc{alloc}
    {
    }

    std::vector<image_resource_handle> renderer_utilities::upload_textures(render_device& dev,
                                                                           std::span<texture_data_descriptor> textures,
                                                                           buffer_resource_handle staging_buffer,
                                                                           bool use_entire_buffer,
                                                                           bool generate_mip_maps)
    {
        std::vector<image_resource_handle> images;

        auto& cmd_executor = dev.get_command_executor();
        auto* cmds = &cmd_executor.get_commands();

        // first, create image for each texture and transition image to transfer destination
        for (texture_data_descriptor& tex_dat : textures)
        {
            auto mip_count = generate_mip_maps
                                 ? std::bit_width(std::min(tex_dat.mips[0].width, tex_dat.mips[0].height)) - 1
                                 : static_cast<std::uint32_t>(tex_dat.mips.size());

            image_create_info ci = {
                .type{image_type::IMAGE_2D},
                .width{tex_dat.mips[0].width},
                .height{tex_dat.mips[0].height},
                .depth{1},
                .layers{1},
                .mip_count{mip_count},
                .format{tex_dat.fmt},
                .samples{sample_count::COUNT_1},
                .transfer_source{true},
                .transfer_destination{true},
                .sampled{true},
                .name{tex_dat.name},
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
} // namespace tempest::graphics