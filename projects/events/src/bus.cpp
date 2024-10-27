#include <tempest/bus.hpp>

namespace tempest::events
{
    bus::bus(const size_t idx)
    {
        _raw_event_payload.resize(idx);
    }

    bool bus::try_publish(const uint32_t type_id, span<const byte> bytes)
    {
        return false;
    }

    optional<raw_byte_payload> bus::peek() const noexcept
    {
        return nullopt;
    }

    bool bus::try_pop()
    {
        return false;
    }
}