#ifndef tempest_core_memory_hpp
#define tempest_core_memory_hpp

#include <tempest/range.hpp>

#include <algorithm>
#include <cstddef>
#include <new>
#include <optional>
#include <source_location>
#include <thread>
#include <vector>

namespace tempest::core
{
    template <typename T, typename... Args>
    [[nodiscard]] inline constexpr T* construct_at(T* ptr, Args&&... args)
    {
        return ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
    }

    template <typename T>
    inline constexpr void destroy_at(T* ptr)
    {
        if constexpr (std::is_array_v<T>)
        {
            for (auto& elem : *ptr)
            {
                destroy_at(std::addressof(elem));
            }
        }
        else
        {
            ptr->~T();
        }
    }

    template <typename ForwardIt>
    inline constexpr void destroy(ForwardIt first, ForwardIt last)
    {
        for (; first != last; ++first)
        {
            destroy_at(std::addressof(*first));
        }
    }

    template <typename ForwardIt>
    inline constexpr void destroy_n(ForwardIt first, std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            destroy_at(std::addressof(*first));
            ++first;
        }
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

    template <typename T>
    class best_fit_scheme
    {
      public:
        explicit best_fit_scheme(const T initial_range);
        [[nodiscard]] std::optional<range<T>> allocate(const T& len);
        void release(range<T>&& rng);
        void release_all();
        void extend(const T& new_length);
        [[nodiscard]] T min_extent() const noexcept;
        [[nodiscard]] T max_extent() const noexcept;

      private:
        range<T> _full;
        std::vector<range<T>> _free;
    };

    class abstract_allocator
    {
      public:
        virtual ~abstract_allocator() = default;
        virtual void* allocate(std::size_t size, std::size_t alignment,
                               std::source_location loc = std::source_location::current()) = 0;
        virtual void deallocate(void* ptr) = 0;
    };

    class stack_allocator final : public abstract_allocator
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

    class heap_allocator final : public abstract_allocator
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
#ifdef __cpp_lib_hardware_interference_size
        alignas(N* std::hardware_constructive_interference_size) T data;
#else
        alignas(N * 64) T data;
#endif
    };

    template <typename T>
    inline best_fit_scheme<T>::best_fit_scheme(const T initial_range)
        : _full{range<T>{
              .start{0},
              .end{initial_range},
          }}
    {
        _free.push_back(_full);
    }

    template <typename T>
    inline std::optional<range<T>> best_fit_scheme<T>::allocate(const T& len)
    {
        struct fit
        {
            std::size_t index;
            range<T> range;
        };

        std::optional<fit> optimal_block = std::nullopt;
        T best_fit = len - len;

        for (std::size_t i = 0; i < _free.size(); ++i)
        {
            range<T> rng = _free[i];

            T range_size = rng.end - rng.start;
            best_fit += range_size;

            if (range_size < len)
            {
                continue;
            }
            else if (range_size == len)
            {
                optimal_block = fit{
                    .index{i},
                    .range{rng},
                };
                break;
            }
            else
            {
                optimal_block = ([&]() -> std::optional<fit> {
                    if (optimal_block)
                    {
                        if (range_size < (optimal_block->range.end - optimal_block->range.start))
                        {
                            return fit{
                                .index{i},
                                .range{rng},
                            };
                        }
                        else
                        {
                            return optimal_block;
                        }
                    }
                    else
                    {
                        return fit{
                            .index{i},
                            .range{rng},
                        };
                    }
                })();
            }
        }

        if (optimal_block)
        {
            auto& [index, rng] = *optimal_block;
            if (rng.end - rng.start == len)
            {
                _free.erase(_free.begin() + index);
            }
            else
            {
                _free[index].start += len;
            }
            return range<T>{
                .start{rng.start},
                .end{rng.start + len},
            };
        }

        return std::nullopt;
    }

    template <typename T>
    inline void best_fit_scheme<T>::release(range<T>&& rng)
    {
        auto free_iter = std::find_if(_free.begin(), _free.end(), [&rng](range<T>& r) { return r.start > rng.start; });
        const std::size_t idx = std::distance(_free.begin(), free_iter);

        if (idx > 0 && rng.start == _free[idx - 1].end)
        {
            // coalesce left
            _free[idx - 1].end = ([&]() {
                if (idx < _free.size() && rng.end == _free[idx].start)
                {
                    auto end = _free[idx].end;
                    _free.erase(_free.begin() + idx);
                    return end;
                }
                else
                {
                    return rng.end;
                }
            })();
            return;
        }
        else if (idx < _free.size() && rng.end == _free[idx].start)
        {
            _free[idx].start = ([&]() {
                if (idx > 0 && rng.start == _free[idx - 1].end)
                {
                    auto start = _free[idx].start;
                    _free.erase(_free.begin() + idx);
                    return start;
                }
                else
                {
                    return rng.start;
                }
            })();

            return;
        }

        _free.insert(_free.begin() + idx, rng);
    }

    template <typename T>
    inline void best_fit_scheme<T>::release_all()
    {
        _free.clear();
        _free.push_back(_full);
    }

    template <typename T>
    inline void best_fit_scheme<T>::extend(const T& new_length)
    {
        if (!_free.empty())
        {
            _free.back().end = new_length;
        }
        else
        {
            _free.push_back(range<T>{
                .start{_full.end},
                .end{new_length},
            });
        }
    }

    template <typename T>
    inline T best_fit_scheme<T>::min_extent() const noexcept
    {
        return _full.start;
    }

    template <typename T>
    inline T best_fit_scheme<T>::max_extent() const noexcept
    {
        return _full.end;
    }

    template <typename T>
    class allocator
    {
      public:
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        using propagate_on_container_copy_assignment = std::true_type;

        constexpr allocator() noexcept = default;
        constexpr allocator(const allocator&) noexcept = default;
        constexpr allocator(allocator&&) noexcept = default;

        template <typename U>
        constexpr allocator(const allocator<U>&) noexcept;

        constexpr ~allocator() = default;

        allocator& operator=(const allocator&) noexcept = default;
        allocator& operator=(allocator&&) noexcept = default;

        [[nodiscard]] constexpr T* allocate(std::size_t n);
        void deallocate(T* ptr, std::size_t n);
    };

    template <typename T>
    template <typename U>
    constexpr allocator<T>::allocator(const allocator<U>&) noexcept
    {
    }

    template <typename T>
    constexpr T* allocator<T>::allocate(std::size_t n)
    {
        void* data = ::operator new[](sizeof(T) * n, std::align_val_t(alignof(T)), std::nothrow);
        return static_cast<T*>(data);
    }

    template <typename T>
    void allocator<T>::deallocate(T* ptr, std::size_t n)
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
            using type = std::false_type;
        };

        template <::tempest::core::propagate_on_container_copy_assignment Alloc>
        struct propagate_on_container_copy_assignment<Alloc>
        {
            using type = typename Alloc::propagate_on_container_copy_assignment;
        };

        template <typename Alloc>
        struct propagate_on_container_move_assignment
        {
            using type = std::false_type;
        };

        template <::tempest::core::propagate_on_container_move_assignment Alloc>
        struct propagate_on_container_move_assignment<Alloc>
        {
            using type = typename Alloc::propagate_on_container_move_assignment;
        };

        template <typename Alloc>
        struct propagate_on_container_swap
        {
            using type = std::false_type;
        };

        template <::tempest::core::propagate_on_container_swap Alloc>
        struct propagate_on_container_swap<Alloc>
        {
            using type = typename Alloc::propagate_on_container_swap;
        };

        template <typename Alloc>
        struct is_always_equal
        {
            using type = typename std::is_empty<Alloc>::type;
        };

        template <::tempest::core::is_always_equal Alloc>
        struct is_always_equal<Alloc>
        {
            using type = typename Alloc::is_always_equal;
        };

        template <typename Alloc>
        struct select_on_container_copy_construction
        {
            using type = std::false_type;
        };

        template <::tempest::core::select_on_container_copy_construction Alloc>
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
        (void)construct_at(p, std::forward<Args>(args)...);
    }

    template <typename Alloc>
    template <typename T>
    inline constexpr void allocator_traits<Alloc>::destroy(allocator_type& alloc, T* p)
    {
        destroy_at(p);
    }

    template <typename Alloc>
    inline constexpr typename allocator_traits<Alloc>::size_type allocator_traits<Alloc>::max_size(
        const allocator_type& alloc) noexcept
    {
        return std::numeric_limits<size_type>::max() / sizeof(value_type);
    }

    template <typename Alloc>
    inline constexpr Alloc allocator_traits<Alloc>::select_on_container_copy_construction(const Alloc& rhs)
    {
        if constexpr (::tempest::core::select_on_container_copy_construction<Alloc>)
        {
            return rhs.select_on_container_copy_construction();
        }
        else
        {
            return rhs;
        }
    }
} // namespace tempest::core

#endif // tempest_core_memory_hpp
