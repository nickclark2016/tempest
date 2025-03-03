#include <tempest/thread.hpp>

#include <exception>

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
            std::terminate();
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
            std::terminate();
        }

        _handle = exchange(other._handle, {});

        return *this;
    }

    bool thread::joinable() const noexcept
    {
#if defined(_WIN32)
        return _handle.id != 0;
#elif defined(__unix__) || defined(__APPLE__) // pthreads
        // Check if a pthread is joinable
        return _handle != pthread_t{};
#else
#error "Unsupported platform"
#endif
    }

    thread::id thread::get_id() const noexcept
    {
#if defined(_WIN32)
        return id{_handle.id};
#elif defined(__unix__) || defined(__APPLE__) // pthreads
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
            std::terminate();
        }

#if defined(_WIN32)
        if (_handle.id == _Thrd_id())
        {
            std::terminate();
        }

        if (WaitForSingleObjectEx(_handle.handle, INFINITE, FALSE) == WAIT_FAILED)
        {
            std::terminate();
        }

        auto result = CloseHandle(_handle.handle);
        if (!result)
        {
            std::terminate();
        }
#elif defined(__unix__) || defined(__APPLE__) // pthreads
        if (_handle == pthread_self())
        {
            std::terminate();
        }

        if (pthread_join(_handle, nullptr) != 0)
        {
            std::terminate();
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
            std::terminate();
        }

#if defined(_WIN32)
        // Release the OS handle
        if (!CloseHandle(_handle.handle))
        {
            std::terminate();
        }
#elif defined(__unix__) || defined(__APPLE__) // pthreads
        pthread_detach(_handle);
#else
#error "Unsupported platform"
#endif

        _handle = {};
    }

    typename thread::native_handle_type thread::native_handle() noexcept
    {
#if defined(_WIN32)
        return _handle.handle;
#elif defined(__unix__) || defined(__APPLE__) // pthreads
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
} // namespace tempest
