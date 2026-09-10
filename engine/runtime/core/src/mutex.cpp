#include <tempest/mutex.hpp>

#include <tempest/exception.hpp>

namespace tempest
{
#ifdef TEMPEST_WIN_THREADS

    void mutex::lock()
    {
        AcquireSRWLockExclusive(&_handle);
    }

    auto mutex::try_lock() -> bool
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

    

    void shared_mutex::lock()
    {
        AcquireSRWLockExclusive(&_handle);
    }

    auto shared_mutex::try_lock() -> bool
    {
        return TryAcquireSRWLockExclusive(&_handle) == TRUE;
    }

    void shared_mutex::unlock()
    {
#pragma warning(push)
#pragma warning(disable : 26110)
        ReleaseSRWLockExclusive(&_handle);
#pragma warning(pop)
    }

    void shared_mutex::lock_shared()
    {
        AcquireSRWLockShared(&_handle);
    }

    auto shared_mutex::try_lock_shared() -> bool
    {
        return TryAcquireSRWLockShared(&_handle) == TRUE;
    }

    void shared_mutex::unlock_shared()
    {
        ReleaseSRWLockShared(&_handle);
    }

#elif defined(TEMPEST_POSIX_THREADS)
    mutex::~mutex()
    {
        if (pthread_mutex_destroy(&_handle) != 0)
        {
            tempest::terminate();
        }
    }

    void mutex::lock()
    {
        if (pthread_mutex_lock(&_handle) != 0)
        {
            tempest::terminate();
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
            tempest::terminate();
        }
    }

    shared_mutex::~shared_mutex()
    {
        if (pthread_rwlock_destroy(&_handle) != 0)
        {
            tempest::terminate();
        }
    }

    void shared_mutex::lock()
    {
        if (pthread_rwlock_wrlock(&_handle) != 0)
        {
            tempest::terminate();
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
            tempest::terminate();
        }
    }

    void shared_mutex::lock_shared()
    {
        if (pthread_rwlock_rdlock(&_handle) != 0)
        {
            tempest::terminate();
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
            tempest::terminate();
        }
    }
#else
#error "Unsupported platform"
#endif
} // namespace tempest