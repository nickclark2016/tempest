#include <tempest/thread.hpp>

#include <exception>

#if defined(TEMPEST_POSIX_THREADS)
#include <sched.h>
#endif

namespace tempest
{
    thread::thread() noexcept : _handle{}
    {
    }

    thread::thread(thread&& other) noexcept : _handle(exchange(other._handle, {}))
    {
    }

    thread::~thread() noexcept
    {
        if (joinable())
        {
            terminate();
        }
    }

    thread& thread::operator=(thread&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        if (joinable())
        {
            terminate();
        }

        _handle = exchange(other._handle, {});

        return *this;
    }

    bool thread::joinable() const noexcept
    {
#if defined(TEMPEST_WIN_THREADS)
        return _handle.id != 0;
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
        // Check if a pthread is joinable
        return _handle != pthread_t{};
#else
#error "Unsupported platform"
#endif
    }

    thread::id thread::get_id() const noexcept
    {
#if defined(TEMPEST_WIN_THREADS)
        return id{_handle.id};
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
        // Get the pthread id
        return id{_handle};
#else
#error "Unsupported platform"
#endif
    }

    void thread::join()
    {
        if (!joinable())
        {
            terminate();
        }

#if defined(TEMPEST_WIN_THREADS)
        if (_handle.id == GetCurrentThreadId())
        {
            terminate();
        }

        if (WaitForSingleObjectEx(_handle.handle, INFINITE, FALSE) == WAIT_FAILED)
        {
            terminate();
        }

        auto result = CloseHandle(_handle.handle);
        if (!result)
        {
            terminate();
        }
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
        if (_handle == pthread_self())
        {
            terminate();
        }

        if (pthread_join(_handle, nullptr) != 0)
        {
            terminate();
        }
#else
#error "Unsupported platform"
#endif

        _handle = {};
    }

    void thread::detach()
    {
        if (!joinable())
        {
            terminate();
        }

#if defined(TEMPEST_WIN_THREADS)
        // Release the OS handle
        if (!CloseHandle(_handle.handle))
        {
            terminate();
        }
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
        pthread_detach(_handle);
#else
#error "Unsupported platform"
#endif

        _handle = {};
    }

    typename thread::native_handle_type thread::native_handle() noexcept
    {
#if defined(TEMPEST_WIN_THREADS)
        return _handle.handle;
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
        return _handle;
#else
#error "Unsupported platform"
#endif
    }

    void thread::swap(thread& other) noexcept
    {
        using tempest::swap;
        swap(_handle, other._handle);
    }

    unsigned int thread::hardware_concurrency() noexcept
    {
#if defined(TEMPEST_WIN_THREADS)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
        // No need to cast to unsigned int, DWORD is unsigned long, which is the same width as an uint on MSVC
#elif defined(TEMPEST_POSIX_THREADS) // pthreads
        return static_cast<unsigned int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
#error "Unsupported platform"
#endif
    }

    namespace this_thread
    {
        thread::id get_id() noexcept
        {
#if defined(TEMPEST_WIN_THREADS)
            return thread::id{static_cast<detail::thread_handle>(GetCurrentThreadId())};
#elif defined(TEMPEST_POSIX_THREADS)
            return thread::id{pthread_self()};
#else
#error "Unsupported platform"
#endif
        }

        void yield() noexcept
        {
#if defined(TEMPEST_WIN_THREADS)
            SwitchToThread();
#elif defined(TEMPEST_POSIX_THREADS)
            sched_yield(); // pthread_yield is deprecated in favor of sched_yield
#else
#error "Unsupported platform"
#endif
        }
    } // namespace this_thread
} // namespace tempest
