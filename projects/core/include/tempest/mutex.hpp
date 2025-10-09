#ifndef tempest_core_mutex_hpp
#define tempest_core_mutex_hpp

#include <tempest/concepts.hpp>
#include <tempest/exception.hpp>
#include <tempest/tuple.hpp>
#include <tempest/utility.hpp>

#if defined(TEMPEST_WIN_THREADS)
#include <windows.h>
#elif defined(TEMPEST_POSIX_THREADS)
#include <pthread.h>
#else
#error "Unsupported platform"
#endif

namespace tempest
{
    class mutex
    {
      public:
#if defined(TEMPEST_WIN_THREADS)
        using native_handle_type = SRWLOCK;
#elif defined(TEMPEST_POSIX_THREADS)
        using native_handle_type = pthread_mutex_t;
#else
#error "Unsupported platform"
#endif

        constexpr mutex() noexcept;
        mutex(const mutex&) = delete;
        ~mutex();

        mutex& operator=(const mutex&) = delete;

        void lock();
        bool try_lock();
        void unlock();

        native_handle_type native_handle() const noexcept;

      private:
        native_handle_type _handle{};
    };

#if defined(TEMPEST_WIN_THREADS)

    inline constexpr mutex::mutex() noexcept
    {
        if (is_constant_evaluated())
        {
            _handle = SRWLOCK{};
        }
        else
        {
            InitializeSRWLock(&_handle);
        }
    }

#elif defined(TEMPEST_POSIX_THREADS)

    inline constexpr mutex::mutex() noexcept
    {
        if (is_constant_evaluated())
        {
            _handle = {};
        }
        else
        {
            pthread_mutex_init(&_handle, nullptr);
        }
    }

#else
#error "Unsupported platform"
#endif

    inline typename mutex::native_handle_type mutex::native_handle() const noexcept
    {
        return _handle;
    }

    class shared_mutex
    {
      public:
#if defined(TEMPEST_WIN_THREADS)
        using native_handle_type = SRWLOCK;
#elif defined(TEMPEST_POSIX_THREADS)
        using native_handle_type = pthread_rwlock_t;
#else
#error "Unsupported platform"
#endif

        constexpr shared_mutex() noexcept;
        shared_mutex(const shared_mutex&) = delete;
        ~shared_mutex();

        shared_mutex& operator=(const shared_mutex&) = delete;

        void lock();
        bool try_lock();
        void unlock();

        void lock_shared();
        bool try_lock_shared();
        void unlock_shared();

        native_handle_type native_handle() const noexcept;

      private:
        native_handle_type _handle{};
    };

#if defined(TEMPEST_WIN_THREADS)

    inline constexpr shared_mutex::shared_mutex() noexcept
    {
        if (is_constant_evaluated())
        {
            _handle = SRWLOCK{};
        }
        else
        {
            InitializeSRWLock(&_handle);
        }
    }

#elif defined(TEMPEST_POSIX_THREADS)

    inline constexpr shared_mutex::shared_mutex() noexcept
    {
        if (is_constant_evaluated())
        {
            _handle = {};
        }
        else
        {
            pthread_rwlock_init(&_handle, nullptr);
        }
    }

#else
#error "Unsupported platform"
#endif

    inline typename shared_mutex::native_handle_type shared_mutex::native_handle() const noexcept
    {
        return _handle;
    }

    template <typename T>
    concept lockable = requires(T t) {
        t.lock();
        { t.try_lock() } -> same_as<bool>;
        t.unlock();
    };

    template <typename T>
    concept shared_lockable = lockable<T> && requires(T t) {
        t.lock_shared();
        { t.try_lock_shared() } -> same_as<bool>;
        t.unlock_shared();
    };

    struct adopt_lock_t
    {
        explicit constexpr adopt_lock_t() = default;
    };

    inline constexpr adopt_lock_t adopt_lock{};

    struct defer_lock_t
    {
        explicit constexpr defer_lock_t() = default;
    };

    inline constexpr defer_lock_t defer_lock{};

    template <lockable Mutex>
    class lock_guard
    {
      public:
        using mutex_type = Mutex;

        explicit lock_guard(mutex_type& m);
        lock_guard(mutex_type& m, adopt_lock_t);
        lock_guard(const lock_guard&) = delete;

        ~lock_guard();

        lock_guard& operator=(const lock_guard&) = delete;

      private:
        mutex_type& _mutex;
    };

    template <lockable Mutex>
    lock_guard(Mutex&) -> lock_guard<Mutex>;

    template <lockable Mutex>
    inline lock_guard<Mutex>::lock_guard(mutex_type& m) : _mutex{m}
    {
        _mutex.lock();
    }

    template <lockable Mutex>
    inline lock_guard<Mutex>::lock_guard(mutex_type& m, adopt_lock_t) : _mutex{m}
    {
    }

    template <lockable Mutex>
    inline lock_guard<Mutex>::~lock_guard()
    {
        _mutex.unlock();
    }

    template <lockable Mutex>
    class unique_lock
    {
      public:
        using mutex_type = Mutex;

        unique_lock() noexcept;
        unique_lock(unique_lock&& other) noexcept;
        explicit unique_lock(mutex_type& m);
        unique_lock(mutex_type& m, adopt_lock_t);
        unique_lock(mutex_type& m, defer_lock_t);

        ~unique_lock();

        unique_lock& operator=(unique_lock&& rhs) noexcept;

        void lock();
        bool try_lock();
        void unlock();

        void swap(unique_lock& other) noexcept;
        mutex_type* release() noexcept;
        bool owns_lock() const noexcept;
        explicit operator bool() const noexcept;

      private:
        mutex_type* _mutex;
        bool _owns_lock = false;
    };

    template <lockable Mutex>
    unique_lock(Mutex&) -> unique_lock<Mutex>;

    template <lockable Mutex>
    inline unique_lock<Mutex>::unique_lock() noexcept : _mutex{nullptr}
    {
    }

    template <lockable Mutex>
    inline unique_lock<Mutex>::unique_lock(unique_lock&& other) noexcept
        : _mutex{exchange(other._mutex, nullptr)}, _owns_lock{exchange(other._owns_lock, false)}
    {
    }

    template <lockable Mutex>
    inline unique_lock<Mutex>::unique_lock(mutex_type& m) : _mutex{&m}
    {
        _mutex->lock();
        _owns_lock = true;
    }

    template <lockable Mutex>
    inline unique_lock<Mutex>::unique_lock(mutex_type& m, adopt_lock_t) : _mutex{&m}, _owns_lock{true}
    {
    }

    template <lockable Mutex>
    inline unique_lock<Mutex>::unique_lock(mutex_type& m, defer_lock_t) : _mutex{&m}
    {
    }

    template <lockable Mutex>
    inline unique_lock<Mutex>::~unique_lock()
    {
        if (_owns_lock)
        {
            _mutex->unlock();
        }

        _mutex = nullptr;
        _owns_lock = false;
    }

    template <lockable Mutex>
    inline unique_lock<Mutex>& unique_lock<Mutex>::operator=(unique_lock&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        if (_owns_lock)
        {
            _mutex->unlock();
        }

        _mutex = exchange(rhs._mutex, nullptr);
        _owns_lock = exchange(rhs._owns_lock, false);

        return *this;
    }

    template <lockable Mutex>
    inline void unique_lock<Mutex>::lock()
    {
        if (!_mutex || owns_lock())
        {
            terminate();
        }

        _mutex->lock();
        _owns_lock = true;
    }

    template <lockable Mutex>
    inline bool unique_lock<Mutex>::try_lock()
    {
        if (!_mutex || owns_lock())
        {
            terminate();
        }
        _owns_lock = _mutex->try_lock();
        return _owns_lock;
    }

    template <lockable Mutex>
    inline void unique_lock<Mutex>::unlock()
    {
        if (!_mutex || !owns_lock())
        {
            terminate();
        }
        _mutex->unlock();
        _owns_lock = false;
    }

    template <lockable Mutex>
    inline void unique_lock<Mutex>::swap(unique_lock& other) noexcept
    {
        using tempest::swap;
        swap(_mutex, other._mutex);
        swap(_owns_lock, other._owns_lock);
    }

    template <lockable Mutex>
    inline typename unique_lock<Mutex>::mutex_type* unique_lock<Mutex>::release() noexcept
    {
        _owns_lock = false;
        return exchange(_mutex, nullptr);
    }

    template <lockable Mutex>
    inline bool unique_lock<Mutex>::owns_lock() const noexcept
    {
        return _owns_lock;
    }

    template <lockable Mutex>
    inline unique_lock<Mutex>::operator bool() const noexcept
    {
        return owns_lock();
    }

    template <lockable Mutex>
    inline void swap(unique_lock<Mutex>& lhs, unique_lock<Mutex>& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    // TODO: Implement lock, try_lock, and unlock free functions
    // TODO: Implement scoped_guard
} // namespace tempest

#endif // tempest_core_mutex_hpp