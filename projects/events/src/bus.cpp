#include <tempest/bus.hpp>

namespace tempest::events
{
    bus::bus(const size_t idx)
    {
        _raw_event_payload.resize(idx);
    }

    bool bus::try_publish(const uint32_t type_id, span<const byte> bytes)
    {
        
    }
}