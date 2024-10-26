#ifndef tempest_events_bus_hpp
#define tempest_events_bus_hpp

#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::events
{
    struct data_header
    {
        uint32_t type_id;
        uint32_t size_bytes;
    };

    struct raw_byte_payload
    {
        data_header header;
        span<byte> bytes;
    };

    class bus
    {
      public:
        explicit bus(size_t sz);

        bool try_publish(uint32_t type_id, span<const byte> payload);
        optional<raw_byte_payload> peek() const noexcept;
        bool try_pop();

      private:
        vector<byte> _raw_event_payload;
        size_t _read_idx{0};
        size_t _write_idx{0};
        size_t _occupancy{0};
    };
} // namespace tempest::events

#endif // tempest_events_bus_hpp