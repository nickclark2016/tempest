#pragma once

#include <tempest/algorithm.hpp>
#include <tempest/assert.hpp>
#include <tempest/mutex.hpp>
#include <tempest/utility.hpp>

#ifndef VMA_ASSERT
#define VMA_ASSERT(expr) TEMPEST_ASSERT(expr)
#endif

#ifndef VMA_HEAVY_ASSERT
#define VMA_HEAVY_ASSERT(expr) TEMPEST_ASSERT(expr)
#endif

#ifndef VMA_MIN
#define VMA_MIN(v1, v2) tempest::min((v1), (v2))
#endif

#ifndef VMA_MAX
#define VMA_MAX(v1, v2) tempest::max((v1), (v2))
#endif

#ifndef VMA_SWAP
#define VMA_SWAP(v1, v2) tempest::swap((v1), (v2))
#endif

#ifndef VMA_SORT
#define VMA_SORT(beg, end, cmp) tempest::sort((beg), (end), (cmp))
#endif

namespace tempest::vk
{
    class vma_mutex
    {
      public:
        void Lock()
        {
            _mutex.lock();
        }

        void Unlock()
        {
            _mutex.unlock();
        }

        bool TryLock()
        {
            return _mutex.try_lock();
        }

      private:
        tempest::mutex _mutex;
    };

    class vma_rw_mutex
    {
      public:
        void LockRead()
        {
            _mutex.lock_shared();
        }

        void UnlockRead()
        {
            _mutex.unlock_shared();
        }

        bool TryLockRead()
        {
            return _mutex.try_lock_shared();
        }

        void LockWrite()
        {
            _mutex.lock();
        }

        void UnlockWrite()
        {
            _mutex.unlock();
        }

        bool TryLockWrite()
        {
            return _mutex.try_lock();
        }

      private:
        tempest::shared_mutex _mutex;
    };
} // namespace tempest::vk

#ifndef VMA_MUTEX
#define VMA_MUTEX ::tempest::vk::vma_mutex
#endif

#ifndef VMA_RW_MUTEX
#define VMA_RW_MUTEX ::tempest::vk::vma_rw_mutex
#endif
