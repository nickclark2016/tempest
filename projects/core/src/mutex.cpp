#include <tempest/mutex.hpp>

#include <exception>

namespace tempest
{
#if defined(TEMPEST_WIN_THREADS)

    mutex::~mutex() = default;

    void mutex::lock()
    {
        AcquireSRWLockExclusive(&_handle);
    }

    bool mutex::try_lock()
    {
        auto result = TryAcquireSRWLockExclusive(&_handle);
        return result == WAIT_OBJECT_0;
    }

    void mutex::unlock()
    {
#pragma warning(push)
#pragma warning(disable : 26110)
        ReleaseSRWLockExclusive(&_handle);
#pragma warning(pop)
    }

    shared_mutex::~shared_mutex()
    {
    }

    void shared_mutex::lock()
    {
        AcquireSRWLockExclusive(&_handle);
    }

    bool shared_mutex::try_lock()
    {
        return TryAcquireSRWLockExclusive(&_handle) == TRUE;
    }

    void shared_mutex::unlock()
    {
#pragma warning(push)
#pragma warning(disable: 26110)
        ReleaseSRWLockExclusive(&_handle);
#pragma warning(pop)
    }

    void shared_mutex::lock_shared()
    {
        AcquireSRWLockShared(&_handle);
    }

    bool shared_mutex::try_lock_shared()
    {
        return TryAcquireSRWLockShared(&_handle) == TRUE;
    }

    void shared_mutex::unlock_shared()
    {
        ReleaseSRWLockShared(&_handle);
    }

#elif defined(TEMPEST_POSIX_THREADS)
    void mutex::lock()
    {
        if (pthread_mutex_lock(&_handle) != 0)
        {
            std::terminate();
        }
    }

    bool mutex::try_lock()
    {
        return pthread_mutex_trylock(&_handle) == 0;
    }

    void mutex::unlock()
    {
        if (pthread_mutex_unlock(&_handle) != 0)
        {
            std::terminate();
        }
    }

    shared_mutex::~shared_mutex()
    {
        if (pthread_rwlock_destroy(&_handle) != 0)
        {
            std::terminate();
        }
    }

    void shared_mutex::lock()
    {
        if (pthread_rwlock_wrlock(&_handle) != 0)
        {
            std::terminate();
        }
    }

    bool shared_mutex::try_lock()
    {
        return pthread_rwlock_trywrlock(&_handle) == 0;
    }

    void shared_mutex::unlock()
    {
        if (pthread_rwlock_unlock(&_handle) != 0)
        {
            std::terminate();
        }
    }

    void shared_mutex::lock_shared()
    {
        if (pthread_rwlock_rdlock(&_handle) != 0)
        {
            std::terminate();
        }
    }

    bool shared_mutex::try_lock_shared()
    {
        return pthread_rwlock_tryrdlock(&_handle) == 0;
    }

    void shared_mutex::unlock_shared()
    {
        if (pthread_rwlock_unlock(&_handle) != 0)
        {
            std::terminate();
        }
    }    
#else
#error "Unsupported platform"
#endif
} // namespace tempest