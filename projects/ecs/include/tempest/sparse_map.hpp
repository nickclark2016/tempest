#ifndef tempest_ecs_sparse_map_hpp
#define tempest_ecs_sparse_map_hpp

#include "keys.hpp"

#include <tempest/algorithm.hpp>

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace tempest::ecs
{
    template <sparse_key K, typename V, std::size_t SparsePageSize = 1024,
              typename DenseKeyAllocator = std::allocator<K>, typename DenseValueAllocator = std::allocator<V>,
              typename PageAllocator = std::allocator<std::uint32_t>,
              typename PageArrayAllocator = std::allocator<std::uint32_t*>>
    class sparse_map
    {
      public:
        sparse_map() = default;
        sparse_map(const sparse_map& src);
        sparse_map(sparse_map&& src) noexcept;
        ~sparse_map();

        sparse_map& operator=(const sparse_map& rhs);
        sparse_map& operator=(sparse_map&& rhs) noexcept;
      private:
        using key_allocator = DenseKeyAllocator;
        using value_allocator = DenseValueAllocator;
        using page_allocator_type = PageAllocator;
        using page_array_allocator_type = PageArrayAllocator;
        using sparse_page_type = std::uint32_t*;

        using size_type = std::size_t;
        using key_type = K;
        using value_type = V;
        using pointer = V*;
        using const_pointer = const V*;
        using reference = V&;
        using const_reference = const V&;
        using iterator = V*;
        using const_iterator = const V*;

        sparse_page_type* _sparse_pages;
        std::size_t _page_count;

        std::size_t _capacity;
        key_type* _keys_begin;
        key_type* _keys_end;
        value_type* _values_begin;
        value_type* _values_end;
    };
} // namespace tempest::ecs

#endif // tempest_ecs_sparse_map_hpp