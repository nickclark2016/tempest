#include <tempest/serial.hpp>

namespace tempest::serialization
{
    auto binary_archive::write(span<const byte> data) -> void
    {
        _buffer.insert(_buffer.end(), data.begin(), data.end());
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
}