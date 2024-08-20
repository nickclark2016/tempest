#ifndef tempest_core_memory_hpp
#define tempest_core_memory_hpp

#include <tempest/int.hpp>
#include <tempest/iterator.hpp>
#include <tempest/memory.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

#include <limits>
#include <new>
#include <source_location>
#include <thread>

namespace tempest
{
    template <typename T>
    constexpr T* addressof(T& arg) noexcept
    {
        return __builtin_addressof(arg);
    }

    template <typename T>
    const T* addressof(const T&&) = delete;

    template <typename T>
    struct pointer_traits;

    template <typename T>
    struct pointer_traits<T*>
    {
        using pointer = T*;
        using element_type = T;
        using difference_type = ptrdiff_t;

        template <typename U>
        using rebind = U*;

        static pointer pointer_to(element_type& p) noexcept
        {
            if constexpr (requires { T::pointer_to(p); })
            {
                return T::pointer_to(p);
            }
            else
            {
                return addressof(p);
            }
        }
    };

    template <typename T>
        requires(!is_function_v<T>)
    [[nodiscard]] inline constexpr auto to_address(T* p) noexcept
    {
        return p;
    }

    template <typename T>
    [[nodiscard]] inline constexpr auto to_address(const T& p) noexcept
    {
        if constexpr (requires { pointer_traits<T>::to_address(p); })
        {
            return pointer_traits<T>::to_address(p);
        }
        else
        {
            return to_address(p.operator->());
        }
    }

    template <typename T, typename... Args>
    [[nodiscard]] inline constexpr T* construct_at(T* ptr, Args&&... args)
    {
        return ::new (static_cast<void*>(ptr)) T(tempest::forward<Args>(args)...);
    }

    template <typename T>
    inline constexpr void destroy_at(T* ptr)
    {
        if constexpr (is_array_v<T>)
        {
            for (auto& elem : *ptr)
            {
                destroy_at(addressof(elem));
            }
        }
        else
        {
            ptr->~T();
        }
    }

    template <forward_iterator ForwardIt>
    inline constexpr void destroy(ForwardIt first, ForwardIt last)
    {
        for (; first != last; ++first)
        {
            destroy_at(addressof(*first));
        }
    }

    template <forward_iterator ForwardIt>
    inline constexpr void destroy_n(ForwardIt first, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            destroy_at(addressof(*first));
            ++first;
        }
    }

    template <forward_iterator ForwardIt, typename T>
    inline constexpr void uninitialized_fill(ForwardIt first, ForwardIt last, const T& value)
    {
        for (; first != last; ++first)
        {
            (void)construct_at(addressof(*first), value);
        }
    }

    template <forward_iterator ForwardIt, integral Count, typename T>
    inline constexpr void uninitialized_fill_n(ForwardIt first, Count n, const T& value)
    {
        for (Count i = 0; i < n; ++i)
        {
            (void)construct_at(addressof(*first), value);
            ++first;
        }
    }

    template <input_iterator InputIt, forward_iterator FwdIt>
    inline constexpr FwdIt uninitialized_copy(InputIt first, InputIt last, FwdIt d_first)
    {
        for (; first != last; ++first, ++d_first)
        {
            (void)construct_at(addressof(*d_first), *first);
        }
        return d_first;
    }

    template <input_iterator InputIt, integral Size, forward_iterator FwdIt>
    inline constexpr FwdIt uninitialized_copy_n(InputIt first, Size n, FwdIt d_first)
    {
        for (Size i = 0; i < n; ++i)
        {
            (void)construct_at(addressof(*d_first), *first);
            ++first;
            ++d_first;
        }
        return d_first;
    }

    template <input_iterator InputIt, forward_iterator FwdIt>
    inline constexpr FwdIt uninitialized_move(InputIt first, InputIt last, FwdIt d_first)
    {
        for (; first != last; ++first, ++d_first)
        {
            (void)construct_at(addressof(*d_first), tempest::move(*first));
        }
        return d_first;
    }

    template <input_iterator InputIt, integral Size, forward_iterator FwdIt>
    inline constexpr FwdIt uninitialized_move_n(InputIt first, Size n, FwdIt d_first)
    {
        for (Size i = 0; i < n; ++i)
        {
            (void)construct_at(addressof(*d_first), tempest::move(*first));
            ++first;
            ++d_first;
        }
        return d_first;
    }

    class no_copy
    {
      public:
        no_copy(const no_copy&) = delete;
        virtual ~no_copy() = default;

        no_copy& operator=(const no_copy&) = delete;

      private:
    };

    class no_move
    {
      public:
        no_move(no_move&&) noexcept = delete;
        virtual ~no_move() = default;

        no_move& operator=(no_move&&) noexcept = delete;
    };

    class no_copy_move : public no_copy, no_move
    {
    };

    class abstract_allocator
    {
      public:
        virtual ~abstract_allocator() = default;
        virtual void* allocate(size_t size, size_t alignment,
                               std::source_location loc = std::source_location::current()) = 0;
        virtual void deallocate(void* ptr) = 0;
    };

    class stack_allocator final : public abstract_allocator
    {
      public:
        explicit stack_allocator(size_t bytes);
        stack_allocator(const stack_allocator&) = delete;
        stack_allocator(stack_allocator&& other) noexcept;

        ~stack_allocator() override;

        stack_allocator& operator=(const stack_allocator&) = delete;
        stack_allocator& operator=(stack_allocator&& rhs) noexcept;

        [[nodiscard]] void* allocate(size_t size, size_t alignment,
                                     std::source_location loc = std::source_location::current()) override;
        void deallocate(void* ptr) override;

        size_t get_marker() const noexcept;
        void free_marker(size_t marker);

        void release();

      private:
        byte* _buffer{0};
        size_t _capacity{0};
        size_t _allocated_bytes{0};
    };

    class heap_allocator final : public abstract_allocator
    {
      public:
        explicit heap_allocator(size_t bytes);
        heap_allocator(const heap_allocator&) = delete;
        heap_allocator(heap_allocator&& other) noexcept;
        ~heap_allocator() override;

        heap_allocator& operator=(const heap_allocator&) = delete;
        heap_allocator& operator=(heap_allocator&& rhs) noexcept;

        [[nodiscard]] void* allocate(size_t size, size_t alignment,
                                     std::source_location loc = std::source_location::current()) override;
        void deallocate(void* ptr) override;

      private:
        void* _tlsf_handle{nullptr};
        byte* _memory{nullptr};
        size_t _allocated_size{0};
        size_t _max_size{0};

        void _release();
    };

    template <typename T, size_t N>
    struct aligned_storage
    {
        alignas(alignof(T)) unsigned char data[sizeof(T[N])];
    };

    template <typename T, size_t N = 2>
    struct cacheline_aligned_storage
    {
#ifdef __cpp_lib_hardware_interference_size
        alignas(N* std::hardware_constructive_interference_size) T data;
#else
        alignas(N * 64) T data;
#endif
    };

    template <typename T>
    class allocator
    {
      public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;

        using propagate_on_container_copy_assignment = true_type;

        constexpr allocator() noexcept = default;
        constexpr allocator(const allocator&) noexcept = default;
        constexpr allocator(allocator&&) noexcept = default;

        template <typename U>
        constexpr allocator(const allocator<U>&) noexcept;

        constexpr ~allocator() = default;

        allocator& operator=(const allocator&) noexcept = default;
        allocator& operator=(allocator&&) noexcept = default;

        [[nodiscard]] constexpr T* allocate(size_t n);
        void deallocate(T* ptr, size_t n);
    };

    template <typename T>
    template <typename U>
    constexpr allocator<T>::allocator(const allocator<U>&) noexcept
    {
    }

    template <typename T>
    constexpr T* allocator<T>::allocate(size_t n)
    {
        void* data = ::operator new[](sizeof(T) * n, std::align_val_t(alignof(T)), std::nothrow);
        return static_cast<T*>(data);
    }

    template <typename T>
    void allocator<T>::deallocate(T* ptr, size_t n)
    {
        ::operator delete[](ptr, std::align_val_t(alignof(T)), std::nothrow);
    }

    template <typename T, typename U>
    [[nodiscard]] constexpr bool operator==(const allocator<T>&, const allocator<U>&) noexcept
    {
        return true;
    }

    template <typename T>
    concept propagate_on_container_copy_assignment =
        requires(T t) { typename T::propagate_on_container_copy_assignment; };

    template <typename T>
    concept propagate_on_container_move_assignment =
        requires(T t) { typename T::propagate_on_container_move_assignment; };

    template <typename T>
    concept propagate_on_container_swap = requires(T t) { typename T::propagate_on_container_swap; };

    template <typename T>
    concept is_always_equal = requires(T t) { typename T::is_always_equal; };

    template <typename T>
    concept select_on_container_copy_construction =
        requires(T t) { typename T::select_on_container_copy_construction; };

    namespace detail
    {
        template <typename Alloc>
        struct propagate_on_container_copy_assignment
        {
            using type = false_type;
        };

        template <::tempest::propagate_on_container_copy_assignment Alloc>
        struct propagate_on_container_copy_assignment<Alloc>
        {
            using type = typename Alloc::propagate_on_container_copy_assignment;
        };

        template <typename Alloc>
        struct propagate_on_container_move_assignment
        {
            using type = false_type;
        };

        template <::tempest::propagate_on_container_move_assignment Alloc>
        struct propagate_on_container_move_assignment<Alloc>
        {
            using type = typename Alloc::propagate_on_container_move_assignment;
        };

        template <typename Alloc>
        struct propagate_on_container_swap
        {
            using type = false_type;
        };

        template <::tempest::propagate_on_container_swap Alloc>
        struct propagate_on_container_swap<Alloc>
        {
            using type = typename Alloc::propagate_on_container_swap;
        };

        template <typename Alloc>
        struct is_always_equal
        {
            using type = typename is_empty<Alloc>::type;
        };

        template <::tempest::is_always_equal Alloc>
        struct is_always_equal<Alloc>
        {
            using type = typename Alloc::is_always_equal;
        };

        template <typename Alloc>
        struct select_on_container_copy_construction
        {
            using type = false_type;
        };

        template <::tempest::select_on_container_copy_construction Alloc>
        struct select_on_container_copy_construction<Alloc>
        {
            using type = typename Alloc::select_on_container_copy_construction;
        };
    } // namespace detail

    template <typename Alloc>
    struct allocator_traits
    {
        using allocator_type = Alloc;
        using value_type = typename Alloc::value_type;
        using size_type = typename Alloc::size_type;
        using difference_type = typename Alloc::difference_type;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using void_pointer = void*;
        using const_void_pointer = const void*;

        using propagate_on_container_copy_assignment =
            typename detail::propagate_on_container_copy_assignment<Alloc>::type;
        using propagate_on_container_move_assignment =
            typename detail::propagate_on_container_move_assignment<Alloc>::type;
        using propagate_on_container_swap = typename detail::propagate_on_container_swap<Alloc>::type;
        using is_always_equal = typename detail::is_always_equal<Alloc>::type;

        template <typename T>
        using rebind_alloc = allocator<T>;

        template <typename T>
        using rebind_traits = allocator_traits<rebind_alloc<T>>;

        [[nodiscard]] static constexpr pointer allocate(allocator_type& alloc, size_type n);
        static constexpr void deallocate(allocator_type& alloc, pointer p, size_type n);

        template <typename T, typename... Args>
        static constexpr void construct(allocator_type& alloc, T* p, Args&&... args);

        template <typename T>
        static constexpr void destroy(allocator_type& alloc, T* p);

        static constexpr size_type max_size(const allocator_type& alloc) noexcept;

        static constexpr Alloc select_on_container_copy_construction(const Alloc& rhs);
    };

    template <typename Alloc>
    inline constexpr allocator_traits<Alloc>::pointer allocator_traits<Alloc>::allocate(allocator_type& alloc,
                                                                                        size_type n)
    {
        return alloc.allocate(n);
    }

    template <typename Alloc>
    inline constexpr void allocator_traits<Alloc>::deallocate(allocator_type& alloc, pointer p, size_type n)
    {
        alloc.deallocate(p, n);
    }

    template <typename Alloc>
    template <typename T, typename... Args>
    inline constexpr void allocator_traits<Alloc>::construct(allocator_type& alloc, T* p, Args&&... args)
    {
        (void)::tempest::construct_at(p, tempest::forward<Args>(args)...);
    }

    template <typename Alloc>
    template <typename T>
    inline constexpr void allocator_traits<Alloc>::destroy(allocator_type& alloc, T* p)
    {
        ::tempest::destroy_at(p);
    }

    template <typename Alloc>
    inline constexpr typename allocator_traits<Alloc>::size_type allocator_traits<Alloc>::max_size(
        [[maybe_unused]] const allocator_type& alloc) noexcept
    {
        return std::numeric_limits<size_type>::max() / sizeof(value_type);
    }

    template <typename Alloc>
    inline constexpr Alloc allocator_traits<Alloc>::select_on_container_copy_construction(const Alloc& rhs)
    {
        if constexpr (::tempest::select_on_container_copy_construction<Alloc>)
        {
            return rhs.select_on_container_copy_construction();
        }
        else
        {
            return rhs;
        }
    }
} // namespace tempest

#endif // tempest_core_memory_hpp
