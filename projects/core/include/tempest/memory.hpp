#ifndef tempest_core_memory_hpp
#define tempest_core_memory_hpp

#include <cstddef>
#include <source_location>
#include <thread>

namespace tempest::core
{
    class allocator
    {
      public:
        virtual ~allocator() = default;
        virtual void* allocate(std::size_t size, std::size_t alignment,
                               std::source_location loc = std::source_location::current()) = 0;
        virtual void deallocate(void* ptr) = 0;
    };

    class stack_allocator final : public allocator
    {
      public:
        explicit stack_allocator(std::size_t bytes);
        stack_allocator(const stack_allocator&) = delete;
        stack_allocator(stack_allocator&& other) noexcept;

        ~stack_allocator() override;

        stack_allocator& operator=(const stack_allocator&) = delete;
        stack_allocator& operator=(stack_allocator&& rhs) noexcept;

        [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment,
                                     std::source_location loc = std::source_location::current()) override;
        void deallocate(void* ptr) override;

        std::size_t get_marker() const noexcept;
        void free_marker(std::size_t marker);

        void release();

      private:
        std::byte* _buffer{0};
        std::size_t _capacity{0};
        std::size_t _allocated_bytes{0};
    };

    class linear_allocator final : public allocator
    {
    };

    class heap_allocator final : public allocator
    {
      public:
        explicit heap_allocator(std::size_t bytes);
        heap_allocator(const heap_allocator&) = delete;
        heap_allocator(heap_allocator&& other) noexcept;
        ~heap_allocator() override;

        heap_allocator& operator=(const heap_allocator&) = delete;
        heap_allocator& operator=(heap_allocator&& rhs) noexcept;

        [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment,
                                     std::source_location loc = std::source_location::current()) override;
        void deallocate(void* ptr) override;
      private:
        void* _tlsf_handle{nullptr};
        std::byte* _memory{nullptr};
        std::size_t _allocated_size{0};
        std::size_t _max_size{0};

        void _release();
    };

    template <typename T, std::size_t N>
    struct aligned_storage
    {
        alignas(alignof(T)) unsigned char data[sizeof(T[N])];
    };

    template <typename T, std::size_t N = 2>
    struct cacheline_aligned_storage
    {
        alignas(N* std::hardware_constructive_interference_size) T data;
    };
} // namespace tempest::core

#endif // tempest_core_memory_hpp
