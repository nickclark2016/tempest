#ifndef tempest_core_shelf_pack_hpp
#define tempest_core_shelf_pack_hpp

#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/span.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vector.hpp>

namespace tempest
{
    namespace detail
    {
        struct shelf
        {
            using shelf_index = uint16_t;
            using item_index = uint16_t;

            static constexpr shelf_index NONE = ~static_cast<shelf_index>(0);

            math::vec2<uint16_t> position;
            uint16_t height;

            shelf_index previous;
            shelf_index next;

            item_index first_item;
            item_index first_unallocated_index;

            bool is_empty;
        };

        struct item
        {
            static constexpr uint16_t NONE = ~static_cast<uint16_t>(0);

            uint16_t x;
            uint16_t width;

            shelf::item_index previous;
            shelf::item_index next;

            shelf::item_index previous_unallocated;
            shelf::item_index next_unallocated;

            shelf::shelf_index shelf_id;

            bool allocated;
            uint16_t generation;
        };
    } // namespace detail

    class shelf_pack_allocator
    {
      public:
        struct allocator_options
        {
            math::vec2<uint32_t> alignment = {1, 1};
            uint32_t column_count = 1;
        };

        struct allocation_id
        {
            allocation_id(uint16_t index, uint16_t generation);

            uint32_t value;

            uint16_t index() const;
            uint16_t generation() const;
        };

        struct allocation
        {
            allocation_id id;
            math::vec2<uint32_t> position;
            math::vec2<uint32_t> extent;
        };

        enum class error_code
        {
            OUT_OF_MEMORY,
            ALLOCATION_TOO_LARGE,
            ZERO_SIZED_ALLOCATION,
            INVALID_ID,
        };

        explicit shelf_pack_allocator(math::vec2<uint32_t> extent, const allocator_options& options);

        bool empty() const noexcept;
        uint32_t used_memory() const noexcept;
        uint32_t free_memory() const noexcept;

        expected<allocation, error_code> allocate(math::vec2<uint32_t> extent);
        void deallocate(allocation_id id);
        void clear();

        expected<allocation, error_code> get(allocation_id id) const;

        math::vec2<uint32_t> extent() const noexcept;

      private:
        vector<detail::shelf> _shelves;
        vector<detail::item> _items;
        math::vec2<uint32_t> _extent;
        math::vec2<uint32_t> _alignment;

        detail::shelf::shelf_index _first_shelf{0};
        detail::shelf::item_index _first_unallocated_item{detail::item::NONE};
        detail::shelf::shelf_index _first_unallocated_shelf{detail::shelf::NONE};

        uint32_t _shelf_width;
        uint32_t _allocated_memory{0};

        detail::shelf::shelf_index _add_shelf(detail::shelf shelf);
        detail::shelf::item_index _add_item(detail::item item);

        void _remove_shelf(detail::shelf::shelf_index index);
        void _remove_item(detail::shelf::item_index index);

        void _init();
    };

    bool operator==(const shelf_pack_allocator::allocation_id& lhs,
                    const shelf_pack_allocator::allocation_id& rhs) noexcept;

    bool operator!=(const shelf_pack_allocator::allocation_id& lhs,
                    const shelf_pack_allocator::allocation_id& rhs) noexcept;

    inline math::vec2<uint32_t> shelf_pack_allocator::extent() const noexcept
    {
        return _extent;
    }

} // namespace tempest

#endif // tempest_core_shelf_pack_hpp