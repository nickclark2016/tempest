#ifndef tempest_core_memory_hpp
#define tempest_core_memory_hpp

#include <tempest/range.hpp>

#include <cstddef>
#include <optional>
#include <source_location>
#include <thread>
#include <vector>

namespace tempest::core
{
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

    template <typename T> class best_fit_scheme
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

    template <typename T, std::size_t N> struct aligned_storage
    {
        alignas(alignof(T)) unsigned char data[sizeof(T[N])];
    };

    template <typename T, std::size_t N = 2> struct cacheline_aligned_storage
    {
        alignas(N* std::hardware_constructive_interference_size) T data;
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

    template <typename T> inline std::optional<range<T>> best_fit_scheme<T>::allocate(const T& len)
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

    template <typename T> inline void best_fit_scheme<T>::release(range<T>&& rng)
    {
        auto free_iter = std::find_if(_free.begin(), _free.end(), [](range<T>& r) { return r.start > rng.start; });
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

    template <typename T> inline void best_fit_scheme<T>::release_all()
    {
        _free.clear();
        _free.push_back(_full);
    }

    template <typename T> inline void best_fit_scheme<T>::extend(const T& new_length)
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

    template <typename T> inline T best_fit_scheme<T>::min_extent() const noexcept
    {
        return _full.start;
    }

    template <typename T> inline T best_fit_scheme<T>::max_extent() const noexcept
    {
        return _full.end;
    }
} // namespace tempest::core

#endif // tempest_core_memory_hpp
