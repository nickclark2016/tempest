#include <tempest/shelf_pack.hpp>

namespace tempest
{
    namespace
    {
        uint32_t align_allocation(uint32_t alignment, uint32_t size)
        {
            const auto remainder = size % alignment;
            return remainder > 0 ? size + alignment - remainder : size;
        }

        uint32_t shelf_height(uint32_t height, uint32_t total_height)
        {
            const auto alignment = [](uint32_t h) {
                if (h < 32)
                    return 8;
                if (h < 128)
                    return 16;
                if (h < 512)
                    return 32;
                return 64;
            }(height);

            const auto adjusted = align_allocation(alignment, height);
            return adjusted > total_height ? total_height : adjusted;
        }

        bool is_some(detail::shelf::item_index index)
        {
            return index != detail::item::none;
        }

        bool is_none(detail::shelf::item_index index)
        {
            return index == detail::item::none;
        }

        // TODO: Allow configuration?
        constexpr uint32_t SHELF_SPLIT_THRESHOLD = 32;
        constexpr uint32_t ITEM_SPLIT_THRESHOLD = 32;
    } // namespace

    shelf_pack_allocator::shelf_pack_allocator(math::vec2<uint32_t> extent,
                                               const shelf_pack_allocator::allocator_options& options)
        : _extent{extent}, _alignment{options.alignment}, _shelf_width{_extent.x}
    {
        _shelf_width = _extent.x / options.column_count;
        _shelf_width -= _shelf_width % _alignment.x;

        _init();
    }

    bool shelf_pack_allocator::empty() const noexcept
    {
        auto index = _first_shelf;

        while (is_some(index))
        {
            const auto& s = _shelves[index];
            if (!s.is_empty)
            {
                return false;
            }

            index = s.next;
        }

        return true;
    }

    uint32_t shelf_pack_allocator::used_memory() const noexcept
    {
        return _allocated_memory;
    }

    uint32_t shelf_pack_allocator::free_memory() const noexcept
    {
        return _extent.x * _extent.y - _allocated_memory;
    }

    expected<shelf_pack_allocator::allocation, shelf_pack_allocator::error_code> shelf_pack_allocator::allocate(
        math::vec2<uint32_t> extent)
    {
        // If the extent of the allocation is larger than the allocator's extent, return an error.
        if (extent.x > _extent.x || extent.y > _extent.y)
        {
            return unexpected{error_code::ALLOCATION_TOO_LARGE};
        }

        // If the extent of the allocation is zero, return an error.
        if (extent.x == 0 || extent.y == 0)
        {
            return unexpected{error_code::ZERO_SIZED_ALLOCATION};
        }

        // Align the extent of the allocation.
        auto width = align_allocation(_alignment.x, extent.x);
        auto height = align_allocation(_alignment.y, extent.y);

        // If the width of the allocation is larger than the width of the shelf, return an error.
        if (width > _extent.x)
        {
            return unexpected{error_code::ALLOCATION_TOO_LARGE};
        }

        height = shelf_height(height, _extent.y);

        // Find a shelf that can accommodate the allocation.
        auto selected_shelf_height = static_cast<uint16_t>(~0);
        auto selected_shelf = detail::shelf::none;
        auto selected_item = detail::item::none;

        auto shelf_index = _first_shelf;

        // Find the first shelf that can accommodate the allocation.
        while (is_some(shelf_index))
        {
            const auto& shelf = _shelves[shelf_index];

            // If the shelf height is less than the height of the allocation, skip the shelf.
            // If the shelf height is greater than or equal to the selected shelf height, skip the shelf.
            // If the shelf is not empty and the shelf height is greater than 1.5 times the height of the allocation,
            // skip the shelf.
            if (shelf.height < height || shelf.height >= selected_shelf_height ||
                (!shelf.is_empty && shelf.height > height + height / 2))
            {
                shelf_index = shelf.next;
                continue;
            }

            auto item_index = shelf.first_unallocated_index;
            // Find the first item that can accommodate the allocation.
            while (is_some(item_index))
            {
                const auto& item = _items[item_index];
                if (!item.allocated && item.width >= width)
                {
                    break;
                }

                item_index = item.next_unallocated;
            }

            // If the item index is valid, update the selected shelf and item.
            if (is_some(item_index))
            {
                selected_shelf = shelf_index;
                selected_item = item_index;
                selected_shelf_height = shelf.height;

                // Check for a perfect fit
                if (shelf.height == height)
                {
                    break;
                }
            }

            shelf_index = shelf.next;
        }

        if (is_none(selected_shelf))
        {
            return unexpected{error_code::OUT_OF_MEMORY};
        }

        const auto shelf = _shelves[selected_shelf];
        _shelves[selected_shelf].is_empty = false;

        // If the shelf is empty and the height of the shelf is greater than the height of the allocation plus a
        // threshold, split the shelf. Provide threshold to prevent splitting a shelf into too small of allocations.
        if (shelf.is_empty && shelf.height > height + SHELF_SPLIT_THRESHOLD)
        {
            detail::shelf shelf_to_insert = {
                .position = {shelf.position.x, static_cast<uint16_t>(shelf.position.y + height)},
                .height = static_cast<uint16_t>(shelf.height - height),
                .previous = selected_shelf,
                .next = shelf.next,
                .first_item = detail::item::none,
                .first_unallocated_index = detail::item::none,
                .is_empty = true,
            };

            auto new_shelf_index = _add_shelf(tempest::move(shelf_to_insert));

            detail::item item_to_insert = {
                .x = shelf.position.x,
                .width = static_cast<uint16_t>(_shelf_width),
                .previous = detail::item::none,
                .next = detail::item::none,
                .previous_unallocated = detail::item::none,
                .next_unallocated = detail::item::none,
                .shelf_id = new_shelf_index,
                .allocated = false,
                .generation = 1,
            };

            auto new_item_index = _add_item(tempest::move(item_to_insert));

            _shelves[new_shelf_index].first_item = new_item_index;
            _shelves[new_shelf_index].first_unallocated_index = new_item_index;

            const auto next = _shelves[selected_shelf].next;
            _shelves[selected_shelf].height = static_cast<std::uint16_t>(height);
            _shelves[selected_shelf].next = new_shelf_index;

            if (is_some(next))
            {
                _shelves[next].previous = new_shelf_index;
            }
        }
        else
        {
            height = shelf.height;
        }

        auto element = _items[selected_item];

        // If the item width is greater than the width of the allocation plus a threshold, split the item.
        if (element.width > width + ITEM_SPLIT_THRESHOLD)
        {
            detail::item item_to_insert = {
                .x = static_cast<uint16_t>(element.x + width),
                .width = static_cast<uint16_t>(element.width - width),
                .previous = selected_item,
                .next = element.next,
                .previous_unallocated = detail::item::none,
                .next_unallocated = detail::item::none,
                .shelf_id = selected_shelf,
                .allocated = false,
                .generation = 1,
            };

            auto new_item_index = _add_item(tempest::move(item_to_insert));

            _items[selected_item].width = static_cast<std::uint16_t>(width);
            _items[selected_item].next = new_item_index;

            if (is_some(element.next))
            {
                _items[element.next].previous = new_item_index;
            }

            auto& s_shelf = _shelves[selected_shelf];
            if (s_shelf.first_unallocated_index == selected_item)
            {
                s_shelf.first_unallocated_index = new_item_index;
            }

            if (is_some(element.previous_unallocated))
            {
                _items[element.previous_unallocated].next_unallocated = new_item_index;
            }

            if (is_some(element.next_unallocated))
            {
                _items[element.next_unallocated].previous_unallocated = new_item_index;
            }
        }
        else
        {
            auto& s_shelf = _shelves[selected_shelf];
            if (s_shelf.first_unallocated_index == selected_item)
            {
                s_shelf.first_unallocated_index = element.next_unallocated;
            }

            if (is_some(element.previous_unallocated))
            {
                _items[element.previous_unallocated].next_unallocated = element.next_unallocated;
            }

            if (is_some(element.next_unallocated))
            {
                _items[element.next_unallocated].previous_unallocated = element.previous_unallocated;
            }

            width = element.width;
        }

        _items[selected_item].allocated = true;

        const auto generation = _items[selected_item].generation;

        auto x = element.x;
        auto y = shelf.position.y;

        auto area = width * height;
        _allocated_memory += area;

        return allocation{
            .position = {x, y},
            .extent = {width, height},
            .id = {static_cast<uint16_t>(selected_item), generation},
        };
    }

    void shelf_pack_allocator::deallocate(shelf_pack_allocator::allocation_id id)
    {
        const auto item_index = id.index();

        detail::item item = _items[item_index];
        assert(item.allocated);
        assert(item.generation == id.generation());

        // Set the item as deallocated.
        _items[item_index].allocated = false;

        // Return the area of the deallocated item to the allocator.
        auto area = item.width * _shelves[item.shelf_id].height;
        _allocated_memory -= area;

        // If there is a next item and the next item is not allocated, coalesce the items.
        if (is_some(item.next) && !_items[item.next].allocated)
        {
            auto next_next = _items[item.next].next;
            auto next_width = _items[item.next].width;

            auto next_unallocated = _items[item.next].next_unallocated;
            auto previous_unallocated = _items[item.next].previous_unallocated;

            if (_shelves[item.shelf_id].first_unallocated_index == item.next)
            {
                _shelves[item.shelf_id].first_unallocated_index = next_unallocated;
            }

            if (is_some(previous_unallocated))
            {
                _items[previous_unallocated].next_unallocated = next_unallocated;
            }

            if (is_some(next_unallocated))
            {
                _items[next_unallocated].previous_unallocated = previous_unallocated;
            }

            _items[item_index].next = next_next;
            _items[item_index].width += next_width;

            item.width += _items[item_index].width;

            if (is_some(next_next))
            {
                _items[next_next].previous = item_index;
            }

            _remove_item(item.next);

            item.next = next_next;
        }

        // If there is a previous item and the previous item is not allocated, coalesce the items.
        if (is_some(item.previous) && !_items[item.previous].allocated)
        {
            _items[item.previous].next = item.next;
            _items[item.previous].width += item.width;

            if (is_some(item.next))
            {
                _items[item.next].previous = item.previous;
            }

            _remove_item(item_index);

            item.previous = _items[item.previous].previous;
        }
        else
        {
            auto first = _shelves[item.shelf_id].first_unallocated_index;
            if (is_some(first))
            {
                _items[first].previous_unallocated = item_index;
            }

            _items[item_index].next_unallocated = first;
            _items[item_index].previous_unallocated = detail::item::none;
            _shelves[item.shelf_id].first_unallocated_index = item_index;
        }

        // If there is no previous or next item, the shelf is empty
        if (is_none(item.previous) && is_none(item.next))
        {
            auto shelf_index = item.shelf_id;
            _shelves[shelf_index].is_empty = true;

            // Merge shelves on the same column
            auto x = _shelves[shelf_index].position.x;

            auto next_shelf_index = _shelves[shelf_index].next;

            // If the next shelf exists, is empty, and is on the same column, merge the shelves.
            if (is_some(next_shelf_index) && _shelves[next_shelf_index].is_empty &&
                _shelves[next_shelf_index].position.x == x)
            {
                auto next_next = _shelves[next_shelf_index].next;
                auto next_height = _shelves[next_shelf_index].height;

                _shelves[shelf_index].next = next_next;
                _shelves[shelf_index].height += next_height;

                if (is_some(next_next))
                {
                    _shelves[next_next].previous = shelf_index;
                }

                _remove_shelf(next_shelf_index);
            }

            auto previous_shelf_index = _shelves[shelf_index].previous;

            // If the previous shelf exists, is empty, and is on the same column, merge the shelves.
            if (is_some(previous_shelf_index) && _shelves[previous_shelf_index].is_empty &&
                _shelves[previous_shelf_index].position.x == x)
            {
                auto next_selected_shelf_index = _shelves[shelf_index].next;
                _shelves[previous_shelf_index].next = next_selected_shelf_index;
                _shelves[previous_shelf_index].height += _shelves[shelf_index].height;

                _shelves[previous_shelf_index].next = next_selected_shelf_index;

                if (is_some(next_selected_shelf_index))
                {
                    _shelves[next_selected_shelf_index].previous = previous_shelf_index;
                }

                _remove_shelf(shelf_index);
            }
        }
    }

    void shelf_pack_allocator::clear()
    {
        _init();
    }

    expected<shelf_pack_allocator::allocation, shelf_pack_allocator::error_code> shelf_pack_allocator::get(shelf_pack_allocator::allocation_id id) const
    {
        const auto item_index = id.index();
        const auto& item = _items[item_index];

        if (!item.allocated || item.generation != id.generation())
        {
            return unexpected{error_code::INVALID_ID};
        }

        const auto& shelf = _shelves[item.shelf_id];

        return allocation{
            .position = {item.x, shelf.position.y},
            .extent = {item.width, shelf.height},
            .id = id,
        };
    }

    detail::shelf::shelf_index shelf_pack_allocator::_add_shelf(detail::shelf shelf)
    {
        // If there are free shelves, reuse the first free shelf.
        if (is_some(_first_unallocated_shelf))
        {
            auto index = _first_unallocated_shelf;
            _first_unallocated_shelf = _shelves[index].next;

            _shelves[index] = shelf;
            return index;
        }

        // Otherwise, add a new shelf.
        auto index = static_cast<detail::shelf::shelf_index>(_shelves.size());
        _shelves.push_back(shelf);
        return index;
    }

    detail::shelf::item_index shelf_pack_allocator::_add_item(detail::item item)
    {
        // If there are free items, reuse the first free item
        if (is_some(_first_unallocated_item))
        {
            auto index = _first_unallocated_item;

            item.generation = _items[index].generation + 1;

            _first_unallocated_item = _items[index].next_unallocated;
            _items[index] = item;

            return index;
        }

        // Otherwise, add a new item.
        auto index = static_cast<detail::shelf::item_index>(_items.size());
        _items.push_back(item);

        return index;
    }

    void shelf_pack_allocator::_remove_shelf(detail::shelf::shelf_index index)
    {
        _remove_item(_shelves[index].first_item);
        _shelves[index].next = _first_unallocated_shelf;
        _first_unallocated_shelf = index;
    }

    void shelf_pack_allocator::_remove_item(detail::shelf::item_index index)
    {
        _items[index].next = _first_unallocated_item;
        _first_unallocated_item = index;
    }

    void shelf_pack_allocator::_init()
    {
        _shelves.clear();
        _items.clear();

        auto column_count = _extent.x / _shelf_width;

        auto prev = detail::shelf::none;
        for (uint32_t i = 0; i < column_count; ++i)
        {
            auto first_item = static_cast<detail::shelf::item_index>(_items.size());
            auto x_position = i * _shelf_width;

            auto current = static_cast<detail::shelf::shelf_index>(i);
            auto next = (i + 1 < column_count) ? static_cast<detail::shelf::shelf_index>(i + 1) : detail::shelf::none;

            // Build shelf
            detail::shelf shelf{
                .position = {static_cast<uint16_t>(x_position), 0},
                .height = static_cast<uint16_t>(_extent.y),
                .previous = prev,
                .next = next,
                .first_item = first_item,
                .first_unallocated_index = first_item,
                .is_empty = true,
            };

            _shelves.push_back(shelf);

            // Build item for shelf
            detail::item item{
                .x = static_cast<uint16_t>(x_position),
                .width = static_cast<uint16_t>(_shelf_width),
                .previous = detail::item::none,
                .next = detail::item::none,
                .previous_unallocated = detail::item::none,
                .next_unallocated = detail::item::none,
                .shelf_id = current,
                .allocated = false,
                .generation = 1,
            };

            _items.push_back(item);

            // Next shelf
            prev = current;
        }

        _allocated_memory = 0;
        _first_shelf = 0;
        _first_unallocated_item = detail::item::none;
        _first_unallocated_shelf = detail::shelf::none;
    }

    shelf_pack_allocator::allocation_id::allocation_id(uint16_t index, uint16_t generation)
        : value{static_cast<uint32_t>(index) | (static_cast<uint32_t>(generation) << 16)}
    {
    }

    uint16_t shelf_pack_allocator::allocation_id::index() const
    {
        return static_cast<uint16_t>(value);
    }

    uint16_t shelf_pack_allocator::allocation_id::generation() const
    {
        return static_cast<uint16_t>(value >> 16);
    }

    bool operator==(const shelf_pack_allocator::allocation_id& lhs,
                    const shelf_pack_allocator::allocation_id& rhs) noexcept
    {
        return lhs.value == rhs.value;
    }

    bool operator!=(const shelf_pack_allocator::allocation_id& lhs,
                    const shelf_pack_allocator::allocation_id& rhs) noexcept
    {
        return lhs.value != rhs.value;
    }
} // namespace tempest