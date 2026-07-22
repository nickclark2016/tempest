#include <tempest/serial.hpp>

#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::serialization
{
    auto binary_archive::write(span<const byte> data) -> void
    {
        const auto current_size = _buffer.size();
        const auto proposed_size = current_size + data.size();
        unsafe::resize_no_init(_buffer, proposed_size);
        tempest::memcpy(_buffer.data() + current_size, data.data(), data.size());
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
        if (_read_offset + count <= _buffer.size())
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
} // namespace tempest::serialization