#include <tempest/serial.hpp>

#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::serialization
{
    auto binary_archive::write(span<const byte> data) -> void
    {
        if (data.empty())
        {
            return;
        }

        const auto current_size = _buffer.size();
        if (data.size() > (static_cast<size_t>(-1) - current_size))
        {
            return;
        }

        const auto proposed_size = current_size + data.size();
        if (proposed_size > _buffer.capacity())
        {
            _buffer.reserve(tempest::bit_ceil(proposed_size));
        }
        unsafe::resize_no_init(_buffer, proposed_size);
        tempest::memcpy(_buffer.data() + current_size, data.data(), data.size());
    }

    auto binary_archive::reserve(size_t capacity) -> void
    {
        _buffer.reserve(capacity);
    }

    auto binary_archive::write(vector<byte> data) -> void
    {
        if (_buffer.empty())
        {
            _buffer = tempest::move(data);
        }
        else
        {
            _buffer.insert(_buffer.end(), data.begin(), data.end());
        }
    }

    auto binary_archive::read(size_t count) -> span<const byte>
    {
        if (count == 0)
        {
            return {};
        }

        if (_read_offset <= _buffer.size() && count <= _buffer.size() - _read_offset)
        {
            auto data = span<const byte>{_buffer.data() + _read_offset, count};
            _read_offset += count;
            return data;
        }

        return {};
    }

    auto binary_archive::written_size() const noexcept -> size_t
    {
        return _buffer.size();
    }

    auto binary_archive::read_offset() const noexcept -> size_t
    {
        return _read_offset;
    }

    auto binary_archive::remaining_bytes() const noexcept -> size_t
    {
        return _read_offset <= _buffer.size() ? (_buffer.size() - _read_offset) : 0;
    }
} // namespace tempest::serialization