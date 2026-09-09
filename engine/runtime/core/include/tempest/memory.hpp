#ifndef tempest_core_memory_hpp
#define tempest_core_memory_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/iterator.hpp>
#include <tempest/limits.hpp>
#include <tempest/source_location.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

#include <new>

namespace tempest
{
    template <typename T>
    constexpr auto addressof(T& arg) noexcept -> T*
    {
        return __builtin_addressof(arg);
    }

    template <typename T>
    auto addressof(const T&&) -> const T* = delete;

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

        static auto pointer_to(element_type& p) noexcept -> pointer
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
    [[nodiscard]] constexpr auto to_address(T* p) noexcept
    {
        return p;
    }

    template <typename T>
    [[nodiscard]] constexpr auto to_address(const T& p) noexcept
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
    [[nodiscard]] constexpr auto construct_at(T* ptr, Args&&... args) -> T*
    {
        return ::new (static_cast<void*>(ptr)) T(tempest::forward<Args>(args)...);
    }

    template <typename T>
    constexpr void destroy_at(T* ptr)
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
    constexpr void destroy(ForwardIt first, ForwardIt last)
    {
        for (; first != last; ++first)
        {
            destroy_at(addressof(*first));
        }
    }

    template <forward_iterator ForwardIt>
    constexpr void destroy_n(ForwardIt first, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            destroy_at(addressof(*first));
            ++first;
        }
    }

    template <forward_iterator ForwardIt, typename T>
    constexpr void uninitialized_fill(ForwardIt first, ForwardIt last, const T& value)
    {
        for (; first != last; ++first)
        {
            (void)construct_at(addressof(*first), value);
        }
    }

    template <forward_iterator ForwardIt, integral Count, typename T>
    constexpr void uninitialized_fill_n(ForwardIt first, Count n, const T& value)
    {
        for (Count i = 0; i < n; ++i)
        {
            (void)construct_at(addressof(*first), value);
            ++first;
        }
    }

    template <input_iterator InputIt, forward_iterator FwdIt>
    constexpr auto uninitialized_copy(InputIt first, InputIt last, FwdIt d_first) -> FwdIt
    {
        for (; first != last; ++first, ++d_first)
        {
            (void)construct_at(addressof(*d_first), *first);
        }
        return d_first;
    }

    template <input_iterator InputIt, integral Size, forward_iterator FwdIt>
    constexpr auto uninitialized_copy_n(InputIt first, Size n, FwdIt d_first) -> FwdIt
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
    constexpr auto uninitialized_move(InputIt first, InputIt last, FwdIt d_first) -> FwdIt
    {
        for (; first != last; ++first, ++d_first)
        {
            (void)construct_at(addressof(*d_first), tempest::move(*first));
        }
        return d_first;
    }

    template <input_iterator InputIt, integral Size, forward_iterator FwdIt>
    constexpr auto uninitialized_move_n(InputIt first, Size n, FwdIt d_first) -> FwdIt
    {
        for (Size i = 0; i < n; ++i)
        {
            (void)construct_at(addressof(*d_first), tempest::move(*first));
            ++first;
            ++d_first;
        }
        return d_first;
    }

    class TEMPEST_API no_copy
    {
      public:
        no_copy(const no_copy&) = delete;
        virtual ~no_copy() = default;

        auto operator=(const no_copy&) -> no_copy& = delete;

      private:
    };

    class TEMPEST_API no_move
    {
      public:
        no_move(no_move&&) noexcept = delete;
        virtual ~no_move() = default;

        auto operator=(no_move&&) noexcept -> no_move& = delete;
    };

    class no_copy_move : public no_copy, no_move
    {
    };

    class TEMPEST_API abstract_allocator
    {
      public:
        virtual ~abstract_allocator() = default;
        virtual auto allocate(size_t size, size_t alignment, source_location loc = source_location::current())
            -> void* = 0;
        virtual void deallocate(void* ptr) = 0;
    };

    class TEMPEST_API stack_allocator final : public abstract_allocator
    {
      public:
        explicit stack_allocator(size_t bytes);
        stack_allocator(const stack_allocator&) = delete;
        stack_allocator(stack_allocator&& other) noexcept;

        ~stack_allocator() override;

        auto operator=(const stack_allocator&) -> stack_allocator& = delete;
        auto operator=(stack_allocator&& rhs) noexcept -> stack_allocator&;

        [[nodiscard]] auto allocate(size_t size, size_t alignment, source_location loc = source_location::current())
            -> void* override;
        void deallocate(void* ptr) override;

        [[nodiscard]] auto get_marker() const noexcept -> size_t;
        void free_marker(size_t marker);

        void release();
        void reset();

        template <typename T>
        auto allocate_typed(size_t count, source_location loc = source_location::current()) -> T*
        {
            void* ptr = allocate(sizeof(T) * count, alignof(T), loc);
            return static_cast<T*>(ptr);
        }

      private:
        byte* _buffer{nullptr};
        size_t _capacity{0};
        size_t _allocated_bytes{0};
    };

    class TEMPEST_API heap_allocator final : public abstract_allocator
    {
      public:
        explicit heap_allocator(size_t bytes);
        heap_allocator(const heap_allocator&) = delete;
        heap_allocator(heap_allocator&& other) noexcept;
        ~heap_allocator() override;

        auto operator=(const heap_allocator&) -> heap_allocator& = delete;
        auto operator=(heap_allocator&& rhs) noexcept -> heap_allocator&;

        [[nodiscard]] auto allocate(size_t size, size_t alignment, source_location loc = source_location::current())
            -> void* override;
        void deallocate(void* ptr) override;

      private:
        void* _tlsf_handle{nullptr};
        byte* _memory{nullptr};
        size_t _allocated_size{0};
        size_t _max_size{0};

        void _release();
    };

    class TEMPEST_API system_allocator final : public abstract_allocator
    {
      public:
        system_allocator() = default;
        ~system_allocator() override = default;

        system_allocator(const system_allocator&) = default;
        system_allocator(system_allocator&&) noexcept = default;
        auto operator=(const system_allocator&) -> system_allocator& = default;
        auto operator=(system_allocator&&) noexcept -> system_allocator& = default;

        [[nodiscard]] auto allocate(size_t size, size_t alignment, source_location loc = source_location::current())
            -> void* override;
        void deallocate(void* ptr) override;
    };

    template <typename T, size_t N>
    struct aligned_storage
    {
        alignas(alignof(T)) unsigned char data[sizeof(T[N])];
    };

    template <typename T, size_t N = 2>
    struct cacheline_aligned_storage
    {
        alignas(N * 64) T data;
    };

    TEMPEST_API auto aligned_alloc(size_t n, size_t alignment) -> void*;
    TEMPEST_API void aligned_free(void* ptr);

    template <typename T>
    class allocator
    {
      public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        using propagate_on_container_copy_assignment = true_type;

        constexpr allocator() noexcept = default;
        constexpr allocator(const allocator&) noexcept = default;
        constexpr allocator(allocator&&) noexcept = default;

        template <typename U>
        constexpr allocator(const allocator<U>& /*unused*/) noexcept
        {
        }

        constexpr ~allocator() = default;

        auto operator=(const allocator&) noexcept -> allocator& = default;
        auto operator=(allocator&&) noexcept -> allocator& = default;

        [[nodiscard]] auto allocate(size_t n) -> T*
        {
            void* data = tempest::aligned_alloc(sizeof(T) * n, alignof(T));
            return static_cast<T*>(data);
        }

        void deallocate(T* ptr, [[maybe_unused]] size_t n)
        {
            tempest::aligned_free(ptr);
        }
    };

    template <typename T, typename U>
    [[nodiscard]] constexpr auto operator==(const allocator<T>& /*unused*/, const allocator<U>& /*unused*/) noexcept
        -> bool
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
            using type = Alloc::propagate_on_container_copy_assignment;
        };

        template <typename Alloc>
        struct propagate_on_container_move_assignment
        {
            using type = false_type;
        };

        template <::tempest::propagate_on_container_move_assignment Alloc>
        struct propagate_on_container_move_assignment<Alloc>
        {
            using type = Alloc::propagate_on_container_move_assignment;
        };

        template <typename Alloc>
        struct propagate_on_container_swap
        {
            using type = false_type;
        };

        template <::tempest::propagate_on_container_swap Alloc>
        struct propagate_on_container_swap<Alloc>
        {
            using type = Alloc::propagate_on_container_swap;
        };

        template <typename Alloc>
        struct is_always_equal
        {
            using type = is_empty<Alloc>::type;
        };

        template <::tempest::is_always_equal Alloc>
        struct is_always_equal<Alloc>
        {
            using type = Alloc::is_always_equal;
        };

        template <typename Alloc>
        struct select_on_container_copy_construction
        {
            using type = false_type;
        };

        template <::tempest::select_on_container_copy_construction Alloc>
        struct select_on_container_copy_construction<Alloc>
        {
            using type = Alloc::select_on_container_copy_construction;
        };
    } // namespace detail

    template <typename Alloc>
    struct allocator_traits
    {
        using allocator_type = Alloc;
        using value_type = Alloc::value_type;
        using size_type = Alloc::size_type;
        using difference_type = Alloc::difference_type;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using void_pointer = void*;
        using const_void_pointer = const void*;

        using propagate_on_container_copy_assignment = detail::propagate_on_container_copy_assignment<Alloc>::type;
        using propagate_on_container_move_assignment = detail::propagate_on_container_move_assignment<Alloc>::type;
        using propagate_on_container_swap = detail::propagate_on_container_swap<Alloc>::type;
        using is_always_equal = detail::is_always_equal<Alloc>::type;

        template <typename T>
        using rebind_alloc = allocator<T>;

        template <typename T>
        using rebind_traits = allocator_traits<rebind_alloc<T>>;

        [[nodiscard]] static constexpr auto allocate(allocator_type& alloc, size_type n) -> pointer;
        static constexpr void deallocate(allocator_type& alloc, pointer p, size_type n);

        template <typename T, typename... Args>
        static constexpr void construct(allocator_type& alloc, T* p, Args&&... args);

        template <typename T>
        static constexpr void destroy(allocator_type& alloc, T* p);

        static constexpr auto max_size(const allocator_type& alloc) noexcept -> size_type;

        static constexpr auto select_on_container_copy_construction(const Alloc& rhs) -> Alloc;
    };

    template <typename Alloc>
    constexpr auto allocator_traits<Alloc>::allocate(allocator_type& alloc, size_type n)
        -> allocator_traits<Alloc>::pointer
    {
        return alloc.allocate(n);
    }

    template <typename Alloc>
    constexpr void allocator_traits<Alloc>::deallocate(allocator_type& alloc, pointer p, size_type n)
    {
        alloc.deallocate(p, n);
    }

    template <typename Alloc>
    template <typename T, typename... Args>
    constexpr void allocator_traits<Alloc>::construct([[maybe_unused]] allocator_type& alloc, T* p, Args&&... args)
    {
        (void)::tempest::construct_at(p, tempest::forward<Args>(args)...);
    }

    template <typename Alloc>
    template <typename T>
    constexpr void allocator_traits<Alloc>::destroy([[maybe_unused]] allocator_type& alloc, T* p)
    {
        ::tempest::destroy_at(p);
    }

    template <typename Alloc>
    constexpr auto allocator_traits<Alloc>::max_size([[maybe_unused]] const allocator_type& alloc) noexcept
        -> allocator_traits<Alloc>::size_type
    {
        return numeric_limits<size_type>::max() / sizeof(value_type);
    }

    template <typename Alloc>
    constexpr auto allocator_traits<Alloc>::select_on_container_copy_construction(const Alloc& rhs) -> Alloc
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

    template <typename T>
    struct default_delete
    {
        constexpr default_delete() noexcept = default;

        template <typename U>
            requires convertible_to<U*, T*>
        constexpr default_delete(const default_delete<U>& /*unused*/) noexcept
        {
        }

        void operator()(T* ptr) const noexcept
        {
            delete ptr;
        }
    };

    template <typename T>
    struct default_delete<T[]>
    {
        constexpr default_delete() noexcept = default;

        template <typename U>
            requires convertible_to<U (*)[], T (*)[]>
        constexpr default_delete(const default_delete<U[]>& /*unused*/) noexcept
        {
        }

        void operator()(T* ptr) const noexcept
        {
            delete[] ptr;
        }
    };

    template <typename T, typename Deleter = default_delete<T>>
    class unique_ptr
    {
      public:
        using pointer = T*;
        using element_type = T;

        constexpr unique_ptr() noexcept = default;

        constexpr unique_ptr(nullptr_t) noexcept
        {
        }

        explicit constexpr unique_ptr(pointer p) noexcept : _ptr{p}
        {
        }

        constexpr unique_ptr(pointer p, const Deleter& d) noexcept : _ptr{p}, _deleter{d}
        {
        }

        constexpr unique_ptr(unique_ptr&& other) noexcept
            : _ptr{other.release()}, _deleter{tempest::move(other._deleter)}
        {
        }

        template <typename U, typename E>
            requires convertible_to<U*, T*> && convertible_to<E, Deleter> && (!is_array_v<U>)
        constexpr unique_ptr(unique_ptr<U, E>&& other) noexcept
            : _ptr{other.release()}, _deleter{tempest::move(other._deleter)}
        {
        }

        unique_ptr(const unique_ptr&) = delete;

        ~unique_ptr()
        {
            if (_ptr)
            {
                _deleter(_ptr);
            }
        }

        constexpr auto operator=(unique_ptr&& other) noexcept -> unique_ptr&
        {
            if (this == addressof(other)) [[unlikely]]
            {
                return *this;
            }

            reset(other.release());
            _deleter = tempest::move(other._deleter);
            return *this;
        }

        template <typename U, typename E>
        constexpr auto operator=(unique_ptr<U, E>&& other) noexcept -> unique_ptr&
        {
            reset(other.release());
            _deleter = tempest::move(other._deleter);
            return *this;
        }

        constexpr auto operator=(nullptr_t) noexcept -> unique_ptr&
        {
            reset();
            return *this;
        }

        auto operator=(const unique_ptr&) -> unique_ptr& = delete;

        [[nodiscard]] constexpr auto release() noexcept -> pointer
        {
            return tempest::exchange(_ptr, nullptr);
        }

        constexpr void reset(pointer p = pointer()) noexcept
        {
            if (_ptr)
            {
                _deleter(_ptr);
            }
            _ptr = p;
        }

        constexpr void reset(nullptr_t) noexcept
        {
            reset();
        }

        void swap(unique_ptr& other) noexcept
        {
            using tempest::swap;
            swap(_ptr, other._ptr);
            swap(_deleter, other._deleter);
        }

        constexpr auto get() const noexcept -> pointer
        {
            return _ptr;
        }

        constexpr auto get_deleter() noexcept -> Deleter&
        {
            return _deleter;
        }

        constexpr auto get_deleter() const noexcept -> const Deleter&
        {
            return _deleter;
        }

        explicit constexpr operator bool() const noexcept
        {
            return _ptr != nullptr;
        }

        constexpr auto operator*() const noexcept(noexcept(*declval<pointer>())) -> add_lvalue_reference_t<T>
        {
            return *_ptr;
        }

        constexpr auto operator->() const noexcept -> pointer
        {
            return _ptr;
        }

      private:
        pointer _ptr{nullptr};
        Deleter _deleter{};

        // Add friend declaration for unique_ptr<U, E> to access private members.
        template <typename U, typename E>
        friend class unique_ptr;
    };

    template <typename T, typename Deleter>
    inline void swap(unique_ptr<T, Deleter>& lhs, unique_ptr<T, Deleter>& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    template <typename T, typename... Args>
    constexpr auto make_unique(Args&&... args) -> unique_ptr<T>
    {
        return unique_ptr<T>(new T(tempest::forward<Args>(args)...));
    }

    template <typename T, typename Deleter>
    inline auto operator==(const unique_ptr<T, Deleter>& lhs, const unique_ptr<T, Deleter>& rhs) noexcept -> bool
    {
        return lhs.get() == rhs.get();
    }

    template <typename T, typename Deleter>
    inline auto operator==(const unique_ptr<T, Deleter>& lhs, nullptr_t) noexcept -> bool
    {
        return !lhs;
    }

    template <typename T, typename Deleter>
    inline auto operator==(nullptr_t, const unique_ptr<T, Deleter>& rhs) noexcept -> bool
    {
        return !rhs;
    }

    template <typename T, typename Deleter>
    inline auto operator!=(const unique_ptr<T, Deleter>& lhs, const unique_ptr<T, Deleter>& rhs) noexcept -> bool
    {
        return !(lhs == rhs);
    }

    template <typename T, typename Deleter>
    inline auto operator!=(const unique_ptr<T, Deleter>& lhs, nullptr_t) noexcept -> bool
    {
        return static_cast<bool>(lhs);
    }

    template <typename T, typename Deleter>
    inline auto operator!=(nullptr_t, const unique_ptr<T, Deleter>& rhs) noexcept -> bool
    {
        return static_cast<bool>(rhs);
    }

    
    
} // namespace tempest

#endif // tempest_core_memory_hpp
