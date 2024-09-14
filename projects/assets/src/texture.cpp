#include <tempest/texture.hpp>

#include <cassert>

namespace tempest::assets
{
    texture::texture(string name) : _name{tempest::move(name)}
    {
        _id = guid::generate_random_guid();
    }

    string_view texture::name() const noexcept
    {
        return _name;
    }

    guid texture::id() const noexcept
    {
        return _id;
    }

    size_t texture::width() const noexcept
    {
        return _width;
    }

    void texture::width(size_t value) noexcept
    {
        _width = value;
    }

    size_t texture::height() const noexcept
    {
        return _height;
    }

    void texture::height(size_t value) noexcept
    {
        _height = value;
    }

    texture_format texture::format() const noexcept
    {
        return _format;
    }

    void texture::format(texture_format value) noexcept
    {
        _format = value;
    }

    span<const byte> texture::data(size_t level) const noexcept
    {
        assert(level < _data.size());
        return _data[level];
    }

    void texture::set_mip_data(size_t level, vector<byte> data)
    {
        if (level >= _data.size())
        {
            _data.resize(level + 1);
        }

        _data[level] = tempest::move(data);
    }

    const texture::sampler_state& texture::sampler() const noexcept
    {
        return _smp;
    }

    void texture::sampler(sampler_state value) noexcept
    {
        _smp = value;
    }
} // namespace tempest::assets