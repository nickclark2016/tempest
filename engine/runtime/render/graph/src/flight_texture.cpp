#include <tempest/render_graph/flight_texture.hpp>

namespace tempest::render_graph
{
    auto flight_texture::init(rhi::device& dev, const flight_texture_desc& desc, uint32_t surface_width,
                              uint32_t surface_height) -> bool
    {
        release(dev);

        _desc = desc;

        const auto res_size = _desc.desc.size.evaluate(surface_width, surface_height);

        auto usage = _desc.desc.usage;
        if (usage == rhi::texture_usage::none)
        {
            usage = rhi::texture_usage::sampled | rhi::texture_usage::storage | rhi::texture_usage::color_attachment;
        }

        const auto req_desc = rhi::texture_desc{
            .width = res_size.width,
            .height = res_size.height,
            .depth = res_size.depth,
            .mip_levels = _desc.desc.mip_levels,
            .array_layers = _desc.desc.array_layers,
            .format = _desc.desc.format,
            .memory_usage = _desc.desc.memory_usage,
            .usage = usage,
            .name = _desc.desc.name,
        };

        const auto total_slots =
            tempest::min(tempest::max(_desc.flight_slots, 1U), static_cast<uint32_t>(max_flight_slots));

        const auto allocate_sampled = static_cast<bool>(req_desc.usage & rhi::texture_usage::sampled);

        for (uint32_t slot_idx = 0; slot_idx < total_slots; ++slot_idx)
        {
            const auto tex = dev.create_texture(req_desc);
            const auto view = dev.create_texture_view(tex, rhi::texture_view_desc{
                                                               .override_format = nullopt,
                                                               .base_mip_level = 0,
                                                               .mip_level_count = req_desc.mip_levels,
                                                               .base_array_layer = 0,
                                                               .array_layer_count = req_desc.array_layers,
                                                           });
            _textures.push_back(tex);
            _views.push_back(view);

            if (allocate_sampled)
            {
                const auto sampled_desc = dev.allocate_descriptor(rhi::descriptor_type::sampled_image);
                dev.write_sampled_image_descriptor(sampled_desc, view, rhi::image_layout::general);
                _sampled_descriptors.push_back(sampled_desc);
            }
        }

        return !_textures.empty();
    }

    auto flight_texture::on_resize(rhi::device& dev, uint32_t surface_width, uint32_t surface_height) -> void
    {
        if (_desc.desc.size.mode == size_mode::surface_relative)
        {
            init(dev, _desc, surface_width, surface_height);
        }
    }

    auto flight_texture::release(rhi::device& dev) -> void
    {
        for (size_t desc_idx = 0; desc_idx < _sampled_descriptors.size(); ++desc_idx)
        {
            if (_sampled_descriptors[desc_idx].index != ~0U)
            {
                dev.free_descriptor(rhi::descriptor_type::sampled_image, _sampled_descriptors[desc_idx]);
            }
        }
        _sampled_descriptors.clear();

        for (size_t tex_idx = 0; tex_idx < _views.size(); ++tex_idx)
        {
            dev.destroy_texture_view(_views[tex_idx]);
            dev.destroy_texture(_textures[tex_idx]);
        }
        _views.clear();
        _textures.clear();
    }
} // namespace tempest::render_graph
