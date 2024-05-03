#include <tempest/flat_unordered_map.hpp>

#include <algorithm>

namespace tempest::core
{
    namespace detail
    {
        std::uint8_t control_byte(std::uint8_t h2) noexcept
        {
            // return the most significant bit
            return h2 & 0x80;
        }

        std::uint8_t hash_byte(std::uint8_t h2) noexcept
        {
            // return the least significant 7 bits
            return h2 & 0x7F;
        }

        bool is_empty(std::uint8_t entry) noexcept
        {
            return entry == empty_entry;
        }

        bool is_deleted(std::uint8_t entry) noexcept
        {
            return entry == deleted_entry;
        }

        bool metadata_entry_strategy::is_empty(metadata_entry entry) const noexcept
        {
            return entry == empty_entry;
        }

        bool metadata_entry_strategy::is_full(metadata_entry entry) const noexcept
        {
            return entry != empty_entry && entry != deleted_entry;
        }

        bool metadata_entry_strategy::is_deleted(metadata_entry entry) const noexcept
        {
            return entry == deleted_entry;
        }

        bool metadata_group::any_empty() const noexcept
        {
            return std::any_of(std::begin(entries), std::end(entries), is_empty);
        }

        std::uint16_t metadata_group::match_byte(std::uint8_t h2) const noexcept
        {
            std::uint16_t result = 0;

            for (std::size_t i = 0; i < group_size; ++i)
            {
                if (entries[i] == h2)
                {
                    result |= 1 << i;
                }
            }

            return result;
        }

        bool metadata_group::any_empty_or_deleted() const noexcept
        {
            return std::any_of(std::begin(entries), std::end(entries),
                               [](std::uint8_t entry) { return is_empty(entry) || is_deleted(entry); });
        }
    } // namespace detail
} // namespace tempest::core