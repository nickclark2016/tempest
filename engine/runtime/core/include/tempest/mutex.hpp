#ifndef tempest_core_mutex_hpp
#define tempest_core_mutex_hpp

#include <tempest/api.hpp>
#include <tempest/concepts.hpp>
#include <tempest/exception.hpp>
#include <tempest/tuple.hpp>
#include <tempest/utility.hpp>

#ifdef TEMPEST_WIN_THREADS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(TEMPEST_POSIX_THREADS)
#include <pthread.h>
#else
#error "Unsupported platform"
#endif

namespace tempest
{
    class TEMPEST_API mutex
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
        mutex(mutex&&) = delete;

#if defined(TEMPEST_WIN_THREADS)
        ~mutex() = default;
#elif defined(TEMPEST_POSIX_THREADS)
        ~mutex();
#endif

        auto operator=(const mutex&) -> mutex& = delete;
        auto operator=(mutex&&) -> mutex& = delete;

        void lock();
        auto try_lock() -> bool;
        void unlock();

        [[nodiscard]] auto native_handle() const noexcept -> native_handle_type;

      private:
        native_handle_type _handle{};
    };

#ifdef TEMPEST_WIN_THREADS

    constexpr mutex::mutex() noexcept
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

    inline auto mutex::native_handle() const noexcept -> typename mutex::native_handle_type
    {
        return _handle;
    }

    class TEMPEST_API shared_mutex
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
#if defined(TEMPEST_WIN_THREADS)
        ~shared_mutex() = default;
#elif defined(TEMPEST_POSIX_THREADS)
        ~shared_mutex();
#endif

        auto operator=(const shared_mutex&) -> shared_mutex& = delete;

        void lock();
        auto try_lock() -> bool;
        void unlock();

        void lock_shared();
        auto try_lock_shared() -> bool;
        void unlock_shared();

        [[nodiscard]] auto native_handle() const noexcept -> native_handle_type;

      private:
        native_handle_type _handle{};
    };

#ifdef TEMPEST_WIN_THREADS

    constexpr shared_mutex::shared_mutex() noexcept
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

    inline auto shared_mutex::native_handle() const noexcept -> typename shared_mutex::native_handle_type
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

    struct TEMPEST_API adopt_lock_t
    {
        explicit constexpr adopt_lock_t() = default;
    };

    inline constexpr adopt_lock_t adopt_lock{};

    struct TEMPEST_API defer_lock_t
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
        lock_guard(mutex_type& m, adopt_lock_t /*unused*/);
        lock_guard(const lock_guard&) = delete;

        ~lock_guard();

        auto operator=(const lock_guard&) -> lock_guard& = delete;

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
    inline lock_guard<Mutex>::lock_guard(mutex_type& m, adopt_lock_t /*unused*/) : _mutex{m}
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
        unique_lock(const unique_lock&) = delete;
        unique_lock(unique_lock&& other) noexcept;
        explicit unique_lock(mutex_type& m);
        unique_lock(mutex_type& m, adopt_lock_t /*unused*/);
        unique_lock(mutex_type& m, defer_lock_t /*unused*/);

        ~unique_lock();

        auto operator=(const unique_lock&) -> unique_lock& = delete;
        auto operator=(unique_lock&& rhs) noexcept -> unique_lock&;

        void lock();
        auto try_lock() -> bool;
        void unlock();

        void swap(unique_lock& other) noexcept;
        auto release() noexcept -> mutex_type*;
        [[nodiscard]] auto owns_lock() const noexcept -> bool;
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
    inline unique_lock<Mutex>::unique_lock(mutex_type& m) : _mutex{&m}, _owns_lock(true)
    {
        _mutex->lock();
    }

    template <lockable Mutex>
    inline unique_lock<Mutex>::unique_lock(mutex_type& m, adopt_lock_t /*unused*/) : _mutex{&m}, _owns_lock{true}
    {
    }

    template <lockable Mutex>
    inline unique_lock<Mutex>::unique_lock(mutex_type& m, defer_lock_t /*unused*/) : _mutex{&m}
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
    inline auto unique_lock<Mutex>::operator=(unique_lock&& rhs) noexcept -> unique_lock<Mutex>&
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
        if ((_mutex == nullptr) || owns_lock())
        {
            terminate();
        }

        _mutex->lock();
        _owns_lock = true;
    }

    template <lockable Mutex>
    inline auto unique_lock<Mutex>::try_lock() -> bool
    {
        if ((_mutex == nullptr) || owns_lock())
        {
            terminate();
        }
        _owns_lock = _mutex->try_lock();
        return _owns_lock;
    }

    template <lockable Mutex>
    inline void unique_lock<Mutex>::unlock()
    {
        if ((_mutex == nullptr) || !owns_lock())
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
    inline auto unique_lock<Mutex>::release() noexcept -> typename unique_lock<Mutex>::mutex_type*
    {
        _owns_lock = false;
        return exchange(_mutex, nullptr);
    }

    template <lockable Mutex>
    inline auto unique_lock<Mutex>::owns_lock() const noexcept -> bool
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
    // TODO: Implement scoped_lock
} // namespace tempest

#endif // tempest_core_mutex_hpp