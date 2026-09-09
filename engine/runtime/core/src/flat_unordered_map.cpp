#include <tempest/flat_unordered_map.hpp>


    namespace tempest::detail
    {
        auto control_byte(uint8_t h2) noexcept -> uint8_t
        {
            // return the most significant bit
            return h2 & 0x80;
        }

        auto hash_byte(uint8_t h2) noexcept -> uint8_t
        {
            // return the least significant 7 bits
            return h2 & 0x7F;
        }

        auto is_empty(uint8_t entry) noexcept -> bool
        {
            return entry == empty_entry;
        }

        auto is_deleted(uint8_t entry) noexcept -> bool
        {
            return entry == deleted_entry;
        }

        auto metadata_entry_strategy::is_empty(metadata_entry entry) const noexcept -> bool
        {
            return entry == empty_entry;
        }

        auto metadata_entry_strategy::is_full(metadata_entry entry) const noexcept -> bool
        {
            return entry != empty_entry && entry != deleted_entry;
        }

        auto metadata_entry_strategy::is_deleted(metadata_entry entry) const noexcept -> bool
        {
            return entry == deleted_entry;
        }

        auto metadata_group::any_empty() const noexcept -> bool
        {
            return any_of(begin(entries), end(entries), is_empty);
        }

        auto metadata_group::match_byte(uint8_t h2) const noexcept -> uint16_t
        {
            uint16_t result = 0;

            for (size_t i = 0; i < group_size; ++i)
            {
                if (entries[i] == h2)
                {
                    result |= 1 << i;
                }
            }

            return result;
        }

        auto metadata_group::any_empty_or_deleted() const noexcept -> bool
        {
            return any_of(begin(entries), end(entries),
                               [](uint8_t entry) -> bool { return is_empty(entry) || is_deleted(entry); });
        }
    } // namespace tempest::detail
