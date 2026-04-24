#ifndef tempest_core_thread_hpp
#define tempest_core_thread_hpp

#include <tempest/bit.hpp>
#include <tempest/compare.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/tuple.hpp>
#include <tempest/type_traits.hpp>

#if defined(TEMPEST_WIN_THREADS)

#define NOMINMAX
#include <Windows.h>
#include <process.h>

#elif defined(TEMPEST_POSIX_THREADS) // pthreads

#include <pthread.h>

#else

#error "Unsupported platform"

#endif

namespace tempest
{
    namespace detail
    {
#if defined(TEMPEST_WIN_THREADS)
        using thread_handle = uint32_t;
        using native_handle_type = void*;

        struct thread_id
        {
            void* handle;
            thread_handle id;
        };
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
        using thread_handle = unsigned long;
        using native_handle_type = unsigned long;
#endif
    } // namespace detail

    class thread
    {
      public:
        using native_handle_type = detail::native_handle_type;

        class id;

        thread() noexcept;
        thread(thread&& other) noexcept;

        template <typename Fn, typename... Args>
        explicit thread(Fn&& fn, Args&&... args);

        thread(const thread&) = delete;

        ~thread() noexcept;

        thread& operator=(thread&& other) noexcept;

        bool joinable() const noexcept;
        id get_id() const noexcept;

        void join();
        void detach();
        void swap(thread& other) noexcept;

        native_handle_type native_handle() noexcept;

        static unsigned int hardware_concurrency() noexcept;

      private:
#if defined(TEMPEST_WIN_THREADS)
        template <typename Tup, size_t... Indices>
        static unsigned int __stdcall _invoke_proc(void* raw_vals) noexcept
        {
            const tempest::unique_ptr<Tup> fn_args{static_cast<Tup*>(raw_vals)};
            Tup& tup = *fn_args.get(); // intenionally avoiding ADL
            tempest::invoke(tempest::move(tempest::get<Indices>(tup))...);
            return 0;
        }

        template <typename Tup, size_t... Indices>
        [[nodiscard]] static constexpr auto _get_invoke_proc(tempest::index_sequence<Indices...>) noexcept
        {
            return &_invoke_proc<Tup, Indices...>;
        }

        template <typename Fn, typename... Args>
        detail::thread_id _start(Fn&& fn, Args&&... args)
        {
            using tuple_type = tempest::tuple<decay_t<Fn>, decay_t<Args>...>;
            auto decayed_copy =
                tempest::make_unique<tuple_type>(tempest::forward<Fn>(fn), tempest::forward<Args>(args)...);
            constexpr auto invoke_proc_ptr =
                _get_invoke_proc<tuple_type>(tempest::make_index_sequence<1 + sizeof...(Args)>{});

            uint32_t thread_id;
            auto handle = _beginthreadex(nullptr, 0, invoke_proc_ptr, decayed_copy.get(), 0, &thread_id);

            detail::thread_id result = {
                .handle = reinterpret_cast<void*>(handle),
                .id = thread_id,
            };

            if (handle)
            {
                // Release the unique pointer to be managed by the spawned thread
                (void)decayed_copy.release();
            }

            return result;
        }

        detail::thread_id _handle{};
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
        template <typename Tup, size_t... Indices>
        static void* _invoke_proc(void* raw_vals) noexcept
        {
            const tempest::unique_ptr<Tup> fn_args{static_cast<Tup*>(raw_vals)};
            Tup& tup = *fn_args.get(); // intenionally avoiding ADL
            tempest::invoke(tempest::move(tempest::get<Indices>(tup))...);
            return nullptr;
        }

        template <typename Tup, size_t... Indices>
        [[nodiscard]] static constexpr auto _get_invoke_proc(tempest::index_sequence<Indices...>) noexcept
        {
            return &_invoke_proc<Tup, Indices...>;
        }

        template <typename Fn, typename... Args>
        detail::thread_handle _start_pthread(Fn&& fn, Args&&... args)
        {
            using tuple_type = tempest::tuple<decay_t<Fn>, decay_t<Args>...>;
            auto decayed_copy =
                tempest::make_unique<tuple_type>(tempest::forward<Fn>(fn), tempest::forward<Args>(args)...);
            constexpr auto invoke_proc_ptr =
                _get_invoke_proc<tuple_type>(tempest::make_index_sequence<1 + sizeof...(Args)>{});

            pthread_t handle;
            if (pthread_create(&handle, nullptr, invoke_proc_ptr, decayed_copy.get()) != 0)
            {
                return pthread_t{};
            }

            // Release the unique pointer to be managed by the spawned thread
            (void)decayed_copy.release();

            return handle;
        }

        pthread_t _handle{};
#else
#error "Unsupported platform"
#endif
    };

#if defined(TEMPEST_WIN_THREADS)

    template <typename Fn, typename... Args>
    thread::thread(Fn&& fn, Args&&... args)
    {
        _handle = _start(tempest::forward<Fn>(fn), tempest::forward<Args>(args)...);
    }

#elif defined(TEMPEST_POSIX_THREADS) // pthreads

    template <typename Fn, typename... Args>
    thread::thread(Fn&& fn, Args&&... args)
    {
        _handle = _start_pthread(tempest::forward<Fn>(fn), tempest::forward<Args>(args)...);
    }
#else
#error "Unsupported platform"
#endif

    namespace this_thread
    {
        thread::id get_id() noexcept;
        void yield() noexcept;
    }; // namespace this_thread

    class thread::id
    {
      public:
        id() noexcept = default;

        explicit id(detail::thread_handle handle) noexcept : _handle{handle}
        {
        }

        constexpr strong_ordering operator<=>(const id& other) const noexcept
        {
#if defined(TEMPEST_WIN_THREADS)
            return three_way_comparer<uintptr_t>::compare(_handle, other._handle);
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
            // Cast to uintptr_t to allow comparison
            return three_way_comparer<uintptr_t>::compare(tempest::bit_cast<uintptr_t>(_handle),
                                                          tempest::bit_cast<uintptr_t>(other._handle));
#else
#error "Unsupported platform"
#endif
        }

      private:
        detail::thread_handle _handle{};

        friend thread::id this_thread::get_id() noexcept;
    };
} // namespace tempest

#endif // tempest_core_thread_hpp
