#include <tempest/render_graph/temporal_texture.hpp>

namespace tempest::render_graph
{
    auto temporal_texture::init(rhi::device& dev, const temporal_texture_desc& desc, uint32_t surface_width,
                                uint32_t surface_height) -> bool
    {
        release(dev);

        _desc = desc;
        _current_slot = 0;
        _valid_history_frames = 0;

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
            tempest::min(tempest::max(_desc.history_count + 1U, 2U), static_cast<uint32_t>(max_temporal_slots));

        for (uint32_t i = 0; i < total_slots; ++i)
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
        }

        return !_textures.empty();
    }

    auto temporal_texture::on_resize(rhi::device& dev, uint32_t surface_width, uint32_t surface_height) -> void
    {
        if (_desc.desc.size.mode == size_mode::surface_relative)
        {
            init(dev, _desc, surface_width, surface_height);
        }
    }

    auto temporal_texture::release(rhi::device& dev) -> void
    {
        for (size_t i = 0; i < _views.size(); ++i)
        {
            dev.destroy_texture_view(_views[i]);
            dev.destroy_texture(_textures[i]);
        }
        _views.clear();
        _textures.clear();
        _current_slot = 0;
        _valid_history_frames = 0;
    }
} // namespace tempest::render_graph
