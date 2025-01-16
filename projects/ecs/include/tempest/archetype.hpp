#ifndef tempest_ecs_archetype_hpp
#define tempest_ecs_archetype_hpp

#include <tempest/functional.hpp>
#include <tempest/int.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::ecs
{
    struct basic_archetype_type_info
    {
        uint16_t size;
        uint16_t alignment;
    };

    template <typename T>
        requires is_trivial_v<T>
    inline basic_archetype_type_info create_archetype_type_info()
    {
        size_t alignment = alignof(T);
        size_t size = sizeof(T);

        basic_archetype_type_info ti = {
            .size = static_cast<uint16_t>(size),
            .alignment = static_cast<uint16_t>(alignment),
        };

        return ti;
    }

    class basic_archetype_storage
    {
      public:
        basic_archetype_storage(basic_archetype_type_info info, size_t initial_capacity = 0);
        basic_archetype_storage(const basic_archetype_storage&) = delete;
        basic_archetype_storage(basic_archetype_storage&& rhs) noexcept;
        ~basic_archetype_storage();

        basic_archetype_storage& operator=(const basic_archetype_storage&) = delete;
        basic_archetype_storage& operator=(basic_archetype_storage&& rhs) noexcept;

        void reserve(size_t count);
        byte* element_at(size_t index);
        const byte* element_at(size_t index) const;

        size_t capacity() const noexcept;

        void copy(size_t dst, size_t src);

      private:
        basic_archetype_type_info _storage;
        byte* _data;
        size_t _size;
    };

    inline size_t basic_archetype_storage::capacity() const noexcept
    {
        return _size;
    }

    struct basic_archetype_key
    {
        uint32_t index;
        uint32_t generation;
    };

    inline constexpr bool operator==(basic_archetype_key lhs, basic_archetype_key rhs) noexcept
    {
        return lhs.index == rhs.index && lhs.generation == rhs.generation;
    }

    inline constexpr bool operator!=(basic_archetype_key lhs, basic_archetype_key rhs) noexcept
    {
        return !(lhs == rhs);
    }

    class basic_archetype
    {
      public:
        using key_type = basic_archetype_key;

        basic_archetype(span<const basic_archetype_type_info> field_info);

        key_type allocate();
        void reserve(size_t count);
        bool erase(key_type key);

        byte* element_at(size_t el_index, size_t type_info_index);
        const byte* element_at(size_t el_index, size_t type_info_index) const;
        byte* element_at(key_type key, size_t type_info_index);
        const byte* element_at(key_type key, size_t type_info_index) const;

        size_t size() const noexcept;
        size_t capacity() const noexcept;
        bool empty() const noexcept;

      private:
        vector<basic_archetype_key> _trampoline;
        vector<uint32_t> _look_back_table; // points from the index of the value to the trampoline table

        vector<basic_archetype_storage> _storage;
        size_t _element_count;
        size_t _element_capacity;
        size_t _first_free_element;
    };

    inline size_t basic_archetype::size() const noexcept
    {
        return _element_count;
    }

    inline size_t basic_archetype::capacity() const noexcept
    {
        return _element_capacity;
    }

    inline bool basic_archetype::empty() const noexcept
    {
        return _element_count == 0;
    }

    template <size_t N>
    struct basic_archetype_types_hash
    {
    };
} // namespace tempest::ecs

#endif // tempest_ecs_archetype_hpp