#ifndef tempest_core_threading_work_steal_queue_hpp
#define tempest_core_threading_work_steal_queue_hpp

#include "../algorithm.hpp"
#include "../memory.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace tempest::core::threading
{
    enum class task_priority : unsigned int
    {
        HIGH = 0,
        MEDIUM = 1,
        LOW = 2,
        COUNT = 3
    };

    // Based on https://www.di.ens.fr/~zappa/readings/ppopp13.pdf
    template <typename T, task_priority MaxPriority = task_priority::COUNT> class work_steal_queue
    {
        static_assert(static_cast<unsigned int>(MaxPriority) > static_cast<unsigned int>(task_priority::HIGH),
                      "Cannot only have high priority tasks");
        static_assert(std::is_pointer_v<T>, "T must be a pointer type.");

      public:
        explicit work_steal_queue(std::uint64_t capacity = 256);
        work_steal_queue(const work_steal_queue&) = delete;
        work_steal_queue(work_steal_queue&&) noexcept = delete;
        ~work_steal_queue();

        work_steal_queue& operator=(const work_steal_queue&) = delete;
        work_steal_queue& operator=(work_steal_queue&&) noexcept = delete;

        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] bool empty(task_priority pri) const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;
        [[nodiscard]] std::size_t size(task_priority pri) const noexcept;
        [[nodiscard]] std::size_t capacity() const noexcept;
        [[nodiscard]] std::size_t capacity(task_priority pri) const noexcept;

        void push(T elem, task_priority pri);
        T pop();
        T pop(task_priority pri);
        T steal();
        T steal(task_priority pri);

      private:
        class data_array;

        cacheline_aligned_storage<std::atomic<std::int64_t>> _top[static_cast<unsigned int>(MaxPriority)];
        cacheline_aligned_storage<std::atomic<std::int64_t>> _bottom[static_cast<unsigned int>(MaxPriority)];
        std::atomic<data_array*> _underlying_data[static_cast<unsigned int>(MaxPriority)];
        std::vector<data_array*> _garbage_data[static_cast<unsigned int>(
            MaxPriority)]; // this could grow infinitely, but as long as tasks are consumed at roughly the same
                           // frequency as they are pushed, it shouldn't grow significantly

        class data_array
        {
          public:
            using size_type = std::make_signed_t<std::size_t>;

            explicit data_array(std::int64_t capacity);

            data_array(const data_array&) = delete;
            data_array(data_array&& other) noexcept = delete;
            ~data_array();

            data_array& operator=(const data_array&) = delete;
            data_array& operator=(data_array&& rhs) noexcept = delete;

            size_type capacity() const noexcept;
            void write(size_type index, T value) noexcept;
            T read(size_type index) noexcept;

            data_array* copy_and_resize(size_type bottom, size_type top);

          private:
            size_type _capacity;
            size_type _mask; // requires capacity to be a power of 2
            std::atomic<T>* _data{nullptr};

            using allocator = std::allocator<std::atomic<T>>;
            allocator _alloc{};

            void _release();
        };

        data_array* _resize_underlying_array(data_array* src, task_priority pri, std::int64_t bottom, std::int64_t top);
    };

    template <typename T, task_priority MaxPriority>
    inline work_steal_queue<T, MaxPriority>::data_array::data_array(std::int64_t capacity)
        : _capacity{capacity}, _mask{_capacity - 1}
    {
        using alloc_traits = std::allocator_traits<allocator>;

        _data = alloc_traits::allocate(_alloc, _capacity);

        for (std::int64_t i = 0; i < capacity; ++i)
        {
            std::construct_at(_data + i);
        }
    }

    template <typename T, task_priority MaxPriority> inline work_steal_queue<T, MaxPriority>::data_array::~data_array()
    {
        using alloc_traits = std::allocator_traits<allocator>;

        std::destroy_n(_data, _capacity);
        alloc_traits::deallocate(_alloc, _data, _capacity);
    }

    template <typename T, task_priority MaxPriority>
    inline work_steal_queue<T, MaxPriority>::data_array::size_type work_steal_queue<
        T, MaxPriority>::data_array::capacity() const noexcept
    {
        return _capacity;
    }

    template <typename T, task_priority MaxPriority>
    inline void work_steal_queue<T, MaxPriority>::data_array::write(size_type index, T value) noexcept
    {
        std::atomic<T>& ref = _data[index & _mask];
        ref.store(value, std::memory_order_relaxed);
    }

    template <typename T, task_priority MaxPriority>
    inline T work_steal_queue<T, MaxPriority>::data_array::read(size_type index) noexcept
    {
        std::atomic<T>& ref = _data[index & _mask];
        return ref.load(std::memory_order_relaxed);
    }

    template <typename T, task_priority MaxPriority>
    inline work_steal_queue<T, MaxPriority>::data_array* work_steal_queue<T, MaxPriority>::data_array::copy_and_resize(
        size_type bottom, size_type top)
    {
        data_array* copy = new data_array{2 * _capacity};
        for (size_type i = top; i < bottom; ++i)
        {
            copy->write(i, read(i));
        }
        return copy;
    }

    template <typename T, task_priority MaxPriority>
    inline work_steal_queue<T, MaxPriority>::work_steal_queue(std::uint64_t capacity)
    {
        core::unroll_loop<0, static_cast<unsigned int>(MaxPriority), 1>([&](auto pri) {
            _top[pri].data.store(0, std::memory_order_relaxed);
            _bottom[pri].data.store(0, std::memory_order_relaxed);
            _underlying_data[pri].store(new data_array{static_cast<data_array::size_type>(capacity)},
                                        std::memory_order_relaxed);
            _garbage_data[pri].reserve(1 << 5); // reserve 32 elements for garbage
        });
    }

    template <typename T, task_priority MaxPriority> inline work_steal_queue<T, MaxPriority>::~work_steal_queue()
    {
        core::unroll_loop<0, static_cast<unsigned int>(MaxPriority), 1>([&](auto pri) {
            for (auto garbage : _garbage_data[pri])
            {
                delete garbage;
            }

            delete _underlying_data[pri].load();
        });
    }

    template <typename T, task_priority MaxPriority>
    inline bool work_steal_queue<T, MaxPriority>::empty() const noexcept
    {
        // check each priority for if it's empty
        for (auto i = 0; i < static_cast<unsigned int>(MaxPriority); ++i)
        {
            if (!empty(static_cast<task_priority>(i)))
            {
                return false;
            }
        }
        return true;
    }

    template <typename T, task_priority MaxPriority>
    inline bool work_steal_queue<T, MaxPriority>::empty(task_priority pri) const noexcept
    {
        auto p = static_cast<unsigned int>(pri);
        auto bottom = _bottom[p].data.load(std::memory_order_relaxed);
        auto top = _top[p].data.load(std::memory_order_relaxed);

        return bottom <= top;
    }

    template <typename T, task_priority MaxPriority>
    inline std::size_t work_steal_queue<T, MaxPriority>::size() const noexcept
    {
        std::size_t sz = 0;

        core::unroll_loop<0, static_cast<unsigned int>(MaxPriority), 1>(
            [&](auto pri) { sz += size(static_cast<task_priority>(pri)); });

        return sz;
    }

    template <typename T, task_priority MaxPriority>
    inline std::size_t work_steal_queue<T, MaxPriority>::size(task_priority pri) const noexcept
    {
        auto p = static_cast<unsigned int>(pri);
        auto bottom = _bottom[p].data.load(std::memory_order_relaxed);
        auto top = _top[p].data.load(std::memory_order_relaxed);

        return static_cast<std::size_t>(std::max(static_cast<std::size_t>(0), static_cast<std::size_t>(bottom - top)));
    }

    template <typename T, task_priority MaxPriority>
    inline std::size_t work_steal_queue<T, MaxPriority>::capacity() const noexcept
    {
        std::size_t sz = 0;

        core::unroll_loop<0, static_cast<unsigned int>(MaxPriority), 1>(
            [&](auto pri) { sz += capacity(static_cast<task_priority>(pri)); });

        return sz;
    }

    template <typename T, task_priority MaxPriority>
    inline std::size_t work_steal_queue<T, MaxPriority>::capacity(task_priority pri) const noexcept
    {
        auto p = static_cast<unsigned int>(pri);
        return _underlying_data[p].load(std::memory_order_relaxed)->capacity();
    }

    template <typename T, task_priority MaxPriority>
    inline void work_steal_queue<T, MaxPriority>::push(T elem, task_priority pri)
    {
        auto p = static_cast<unsigned int>(pri);

        auto bottom = _bottom[p].data.load(std::memory_order_relaxed);
        auto top = _top[p].data.load(std::memory_order_acquire);
        data_array* data = _underlying_data[p].load(std::memory_order_relaxed);

        // resize if needed
        if (data->capacity() <= (bottom - top))
        {
            data = _resize_underlying_array(data, pri, bottom, top);
        }

        data->write(bottom, elem);
        std::atomic_thread_fence(std::memory_order_release); // release after getting top
        _bottom[p].data.store(bottom + 1, std::memory_order_relaxed);
    }

    template <typename T, task_priority MaxPriority> inline T work_steal_queue<T, MaxPriority>::pop()
    {
        auto max_pri = static_cast<unsigned int>(MaxPriority);
        for (unsigned int i = 0; i < max_pri; ++i)
        {
            auto t = pop(static_cast<task_priority>(i));
            if (t != nullptr)
            {
                return t;
            }
        }
        return nullptr;
    }

    template <typename T, task_priority MaxPriority> inline T work_steal_queue<T, MaxPriority>::pop(task_priority pri)
    {
        auto p = static_cast<unsigned int>(pri);

        auto bottom = _bottom[p].data.load(std::memory_order_relaxed) - 1;
        auto data = _underlying_data[p].load(std::memory_order_relaxed);
        _bottom[p].data.store(bottom, std::memory_order_relaxed); // subtract one from the bottom
        std::atomic_thread_fence(std::memory_order_seq_cst);      // no reorder
        auto top = _top[p].data.load(std::memory_order_relaxed);

        T elem{nullptr};

        if (top <= bottom)
        {
            elem = data->read(bottom);

            // last item in queue
            if (top == bottom)
            {
                // check to make sure the item is still there if its the last in queue
                if (!_top[p].data.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst,
                                                          std::memory_order_relaxed))
                {
                    // if the top element was stolen already, can't steal what's already gone
                    elem = nullptr;
                }

                _bottom[p].data.store(
                    bottom + 1,
                    std::memory_order_relaxed); // top cannot be bottom, push the bottom to the original spot
            }
        }
        else
        {
            _bottom[p].data.store(
                bottom + 1, std::memory_order_relaxed); // can't get an element, push the bottom to the original spot
        }

        return elem;
    }

    template <typename T, task_priority MaxPriority>
    inline T tempest::core::threading::work_steal_queue<T, MaxPriority>::steal()
    {
        auto max_pri = static_cast<unsigned int>(MaxPriority);
        for (unsigned int i = 0; i < max_pri; ++i)
        {
            auto t = steal(static_cast<task_priority>(i));
            if (t != nullptr)
            {
                return t;
            }
        }
        return nullptr;
    }

    template <typename T, task_priority MaxPriority>
    inline T tempest::core::threading::work_steal_queue<T, MaxPriority>::steal(task_priority pri)
    {
        auto p = static_cast<unsigned int>(pri);

        auto top = _top[p].data.load(std::memory_order_acquire); // don't reorder reads/writes before getting the top
        std::atomic_thread_fence(std::memory_order_seq_cst);     // prevent read and write reordering
        auto bottom =
            _bottom[p].data.load(std::memory_order_acquire); // don't reorder reads/writes before getting the bottom

        T elem{nullptr};

        if (top < bottom)
        {
            data_array* data = _underlying_data[p].load(
                std::memory_order_consume); // do not reorder reads or writes before this operation
            elem = data->read(top);

            // if the owner popped this before steal could complete, steal was not successful and queue is empty
            if (!_top[p].data.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst,
                                                      std::memory_order_relaxed))
            {
                elem = nullptr;
            }
        }

        return elem;
    }

    template <typename T, task_priority MaxPriority>
    inline work_steal_queue<T, MaxPriority>::data_array* work_steal_queue<T, MaxPriority>::_resize_underlying_array(
        data_array* src, task_priority pri, std::int64_t bottom, std::int64_t top)
    {
        auto p = static_cast<unsigned int>(pri);

        data_array* copy = src->copy_and_resize(bottom, top);
        _garbage_data[p].push_back(src);
        std::swap(src, copy);
        _underlying_data[p].store(src, std::memory_order_release);

        return src;
    }
} // namespace tempest::core::threading

#endif // tempest_core_threading_work_steal_queue_hpp