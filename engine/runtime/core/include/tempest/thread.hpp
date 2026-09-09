#ifndef tempest_core_thread_hpp
#define tempest_core_thread_hpp

#include <tempest/api.hpp>
#include <tempest/bit.hpp>
#include <tempest/chrono.hpp>
#include <tempest/compare.hpp>
#include <tempest/hash.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/tuple.hpp>
#include <tempest/type_traits.hpp>

#ifdef TEMPEST_WIN_THREADS

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <process.h>

#ifdef small
#undef small
#endif

#elif defined(TEMPEST_POSIX_THREADS) // pthreads

#include <pthread.h>

#else

#error "Unsupported platform"

#endif

namespace tempest
{
    namespace detail
    {
#ifdef TEMPEST_WIN_THREADS
        using thread_handle = uint32_t;
        using native_handle_type = void*;

        struct TEMPEST_API thread_id
        {
            void* handle;
            thread_handle id;
        };
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
        using thread_handle = unsigned long;
        using native_handle_type = unsigned long;
#endif
    } // namespace detail

    class TEMPEST_API thread
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

        auto operator=(thread&& other) noexcept -> thread&;

        [[nodiscard]] auto joinable() const noexcept -> bool;
        [[nodiscard]] auto get_id() const noexcept -> id;

        void join();
        void detach();
        void swap(thread& other) noexcept;

        auto native_handle() noexcept -> native_handle_type;

        static auto hardware_concurrency() noexcept -> unsigned int;

      private:
#ifdef TEMPEST_WIN_THREADS
        template <typename Tup, size_t... Indices>
        static unsigned int __stdcall _invoke_proc(void* raw_vals) noexcept
        {
            const tempest::unique_ptr<Tup> fn_args{static_cast<Tup*>(raw_vals)};
            Tup& tup = *fn_args.get(); // intenionally avoiding ADL
            tempest::invoke(tempest::move(tempest::get<Indices>(tup))...);
            return 0;
        }

        template <typename Tup, size_t... Indices>
        [[nodiscard]] static constexpr auto _get_invoke_proc(tempest::index_sequence<Indices...> /*unused*/) noexcept
        {
            return &_invoke_proc<Tup, Indices...>;
        }

        template <typename Fn, typename... Args>
        auto _start(Fn&& fn, Args&&... args) -> detail::thread_id
        {
            using tuple_type = tempest::tuple<decay_t<Fn>, decay_t<Args>...>;
            auto decayed_copy =
                tempest::make_unique<tuple_type>(tempest::forward<Fn>(fn), tempest::forward<Args>(args)...);
            constexpr auto invoke_proc_ptr =
                _get_invoke_proc<tuple_type>(tempest::make_index_sequence<1 + sizeof...(Args)>{});

            uint32_t thread_id = 0;
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

#ifdef TEMPEST_WIN_THREADS

    template <typename Fn, typename... Args>
    thread::thread(Fn&& fn, Args&&... args) : _handle(_start(tempest::forward<Fn>(fn), tempest::forward<Args>(args)...))
    {
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
        TEMPEST_API auto get_id() noexcept -> thread::id;
        TEMPEST_API void yield() noexcept;
        TEMPEST_API void sleep_for_nanoseconds(uint64_t ns) noexcept;

        template <typename Rep, typename Period>
        void sleep_for(const chrono::duration<Rep, Period>& rel_time)
        {
            auto ns = chrono::duration_cast<chrono::nanoseconds>(rel_time).count();
            if (ns > 0)
            {
                sleep_for_nanoseconds(static_cast<uint64_t>(ns));
            }
        }
    }; // namespace this_thread

    class TEMPEST_API thread::id
    {
      public:
        id() noexcept = default;

        explicit id(detail::thread_handle handle) noexcept : _handle{handle}
        {
        }

        constexpr auto operator<=>(const id& other) const noexcept -> strong_ordering
        {
#ifdef TEMPEST_WIN_THREADS
            return three_way_comparer<uintptr_t>::compare(_handle, other._handle);
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
            // Cast to uintptr_t to allow comparison
            return three_way_comparer<uintptr_t>::compare(tempest::bit_cast<uintptr_t>(_handle),
                                                          tempest::bit_cast<uintptr_t>(other._handle));
#else
#error "Unsupported platform"
#endif
        }

        [[nodiscard]] constexpr auto to_uint64() const noexcept -> uint64_t
        {
#ifdef TEMPEST_WIN_THREADS
            return static_cast<uint64_t>(_handle);
#elif defined(TEMPEST_POSIX_THREADS)
            return static_cast<uint64_t>(tempest::bit_cast<uintptr_t>(_handle));
#else
#error "Unsupported platform"
#endif
        }

      private:
        detail::thread_handle _handle{};

        friend auto this_thread::get_id() noexcept -> thread::id;
    };

    template <>
    struct hash<thread::id>
    {
        auto operator()(thread::id id) const noexcept -> size_t
        {
            return hash<uint64_t>()(id.to_uint64());
        }
    };
} // namespace tempest

#endif // tempest_core_thread_hpp
