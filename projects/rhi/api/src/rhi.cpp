#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>

namespace tempest::rhi
{
    bool descriptor_resource_binding::image_binding::operator==(const image_binding& other) const noexcept
    {
        return image == other.image && sampler == other.sampler && layout == other.layout;
    }

    bool descriptor_resource_binding::image_binding::operator!=(const image_binding& other) const noexcept
    {
        return !(*this == other);
    }

    bool descriptor_resource_binding::buffer_binding::operator==(const buffer_binding& other) const noexcept
    {
        return buffer == other.buffer && offset == other.offset && size == other.size;
    }

    bool descriptor_resource_binding::buffer_binding::operator!=(const buffer_binding& other) const noexcept
    {
        return !(*this == other);
    }

    void descriptor_resource_binding::bind_image(uint32_t set, uint32_t binding,
                                                 typed_rhi_handle<rhi_handle_type::IMAGE> image,
                                                 typed_rhi_handle<rhi_handle_type::SAMPLER> sampler,
                                                 image_layout layout) noexcept
    {
        _image_bindings[make_key(set, binding)] = {image, sampler, layout};
    }

    void descriptor_resource_binding::bind_buffer(uint32_t set, uint32_t binding,
                                                  typed_rhi_handle<rhi_handle_type::BUFFER> buffer, size_t offset,
                                                  size_t size) noexcept
    {
        _buffer_bindings[make_key(set, binding)] = {buffer, offset, size};
    }

    const flat_unordered_map<uint64_t, descriptor_resource_binding::image_binding>& descriptor_resource_binding::
        get_image_bindings() const noexcept
    {
        return _image_bindings;
    }

    const flat_unordered_map<uint64_t, descriptor_resource_binding::buffer_binding>& descriptor_resource_binding::
        get_buffer_bindings() const noexcept
    {
        return _buffer_bindings;
    }

    bool descriptor_resource_binding::operator==(const descriptor_resource_binding& other) const noexcept
    {
        return _image_bindings == other._image_bindings && _buffer_bindings == other._buffer_bindings;
    }

    bool descriptor_resource_binding::operator!=(const descriptor_resource_binding& other) const noexcept
    {
        return !(*this == other);
    }

    uint64_t descriptor_resource_binding::make_key(uint32_t set, uint32_t binding) noexcept
    {
        return (static_cast<uint64_t>(set) << 32) | binding;
    }

    void descriptor_resource_binding::split_key(uint64_t key, uint32_t& set, uint32_t& binding) noexcept
    {
        set = static_cast<uint32_t>(key >> 32);
        binding = static_cast<uint32_t>(key & 0xFFFFFFFF);
    }
} // namespace tempest::rhi