#ifndef tempest_core_object_pool_hpp__
#define tempest_core_object_pool_hpp__

#include "memory.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tempest::core
{
    class object_pool
    {
      public:
        object_pool(allocator* alloc, std::uint32_t pool_size, std::uint32_t resource_size);
        object_pool(const object_pool&) = delete;
        object_pool(object_pool&&) noexcept = delete;
        virtual ~object_pool();

        object_pool& operator=(const object_pool&) = delete;
        object_pool& operator=(object_pool&&) noexcept = delete;

        [[nodiscard]] std::uint32_t acquire_resource();
        void release_resource(std::uint32_t index);
        void release_all_resources();

        [[nodiscard]] void* access(std::uint32_t index);
        [[nodiscard]] const void* access(std::uint32_t index) const;

        [[nodiscard]] std::size_t size() const noexcept;

      private:
        std::byte* _memory{nullptr};
        std::uint32_t* _free_indices{nullptr};
        allocator* _alloc{nullptr}; // non-owning

        std::uint32_t _free_index_head{0};
        std::uint32_t _pool_size{0};
        std::uint32_t _resource_size{0};
        std::uint32_t _used_index_count{0};
    };
} // namespace tempest::core

#endif // tempest_core_object_pool_hpp__