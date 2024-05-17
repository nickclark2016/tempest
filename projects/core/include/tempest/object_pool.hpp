#ifndef tempest_core_object_pool_hpp
#define tempest_core_object_pool_hpp

#include <tempest/memory.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace tempest::core
{
    class object_pool
    {
      public:
        object_pool(abstract_allocator* _alloc, std::uint32_t pool_size, std::uint32_t resource_size);
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
        abstract_allocator* _alloc{nullptr}; // non-owning

        std::uint32_t _free_index_head{0};
        std::uint32_t _pool_size{0};
        std::uint32_t _resource_size{0};
        std::uint32_t _used_index_count{0};
    };

    class generational_object_pool
    {
      public:
        struct key
        {
            std::uint32_t index;
            std::uint32_t generation;

            constexpr auto operator<=>(const key&) const noexcept = default;
            constexpr operator bool() const noexcept
            {
                return index != ~0u && generation != ~0u;
            }
        };

        inline static constexpr key invalid_key = {.index = ~0u, .generation = ~0u};

        generational_object_pool(abstract_allocator* _alloc, std::uint32_t pool_size, std::uint32_t resource_size);
        generational_object_pool(const object_pool&) = delete;
        generational_object_pool(object_pool&&) noexcept = delete;
        ~generational_object_pool();

        generational_object_pool& operator=(const generational_object_pool&) = delete;
        generational_object_pool& operator=(generational_object_pool&&) noexcept = delete;

        [[nodiscard]] key acquire_resource();
        void release_resource(key index);
        void release_all_resources();

        [[nodiscard]] void* access(key index);
        [[nodiscard]] const void* access(key index) const;

        [[nodiscard]] std::size_t size() const noexcept;

      private:
        abstract_allocator* _alloc;
        std::byte* _memory{nullptr};
        std::uint32_t* _erased{nullptr};
        key* _keys{nullptr};
        std::byte* _payload{nullptr};

        std::uint32_t _pool_size;
        std::uint32_t _resource_size;
        std::uint32_t _free_index_head;
        std::uint32_t _used_index_count;
    };
} // namespace tempest::core

#endif // tempest_core_object_pool_hpp
