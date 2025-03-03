#include <tempest/thread.hpp>

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
        return _handle.id != 0;
    }

    thread::id thread::get_id() const noexcept
    {
        return id{_handle.id};
    }

    void thread::join()
    {
        if (!joinable())
        {
            std::terminate();
        }

#ifdef _WIN32
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
#endif

        _handle = {};
    }

    void thread::detach()
    {
        if (!joinable())
        {
            std::terminate();
        }

#ifdef _WIN32
        // Release the OS handle
        if (!CloseHandle(_handle.handle))
        {
            std::terminate();
        }
#endif

        _handle = {};
    }

    typename thread::native_handle_type thread::native_handle() noexcept
    {
        return _handle.handle;
    }

    void thread::swap(thread& other) noexcept
    {
        using tempest::swap;
        swap(_handle, other._handle);
    }
} // namespace tempest
