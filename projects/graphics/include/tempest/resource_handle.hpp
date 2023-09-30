#ifndef tempest_graphics_resource_handle_hpp
#define tempest_graphics_resource_handle_hpp

#include <cstddef>
#include <numeric>

namespace tempest::graphics
{
    class resource_allocator;

    struct raw_handle
    {
        std::size_t id{std::numeric_limits<std::size_t>::max()};
    };

    template <typename T>
    struct resource_handle : public raw_handle
    {
        T payload;

        inline constexpr bool operator==(const resource_handle& rhs) const noexcept
        {
            return id == rhs.id;
        }

        inline constexpr bool operator!=(const resource_handle& rhs) const noexcept
        {
            return id != rhs.id;
        }
    };

    template <typename T>
    class unique_resource
    {
      public:
        inline unique_resource() : _allocator{nullptr}, _payload{}
        {
        }

        inline explicit unique_resource(resource_allocator& alloc) : _allocator{std::addressof(alloc)}, _payload{}
        {
        }

        unique_resource(resource_allocator& alloc, T value)
            : _allocator{std::addressof(alloc)}, _payload{std::move(value)}
        {
        }

        unique_resource(const unique_resource&) = delete;

        inline unique_resource(unique_resource&& rhs) noexcept
            : _allocator{std::move(rhs._allocator)}, _payload{rhs.release()}
        {
        }

        ~unique_resource() noexcept;

        unique_resource& operator=(const unique_resource&) = delete;

        unique_resource& operator=(unique_resource&&) noexcept;

        explicit operator bool() const noexcept;
        T* operator->() noexcept;
        const T* operator->() const noexcept;
        T& operator*() noexcept;
        const T& operator*() const noexcept;

        T& get() noexcept;
        const T& get() const noexcept;

        void reset(T value = T()) noexcept;
        T release() noexcept;

        void swap(unique_resource<T>& rhs) noexcept;

      private:
        resource_allocator* _allocator;
        T _payload;
    };

    template <typename T>
    inline unique_resource<T>& unique_resource<T>::operator=(unique_resource&& rhs) noexcept
    {
        auto tmp = rhs._allocator;
        reset(rhs.release());
        _allocator = tmp;

        return *this;
    }

    template <typename T>
    inline unique_resource<T>::operator bool() const noexcept
    {
        return static_cast<bool>(_payload);
    }

    template <typename T>
    inline T* unique_resource<T>::operator->() noexcept
    {
        return &_payload;
    }

    template <typename T>
    inline const T* unique_resource<T>::operator->() const noexcept
    {
        return &_payload;
    }

    template <typename T>
    inline T& unique_resource<T>::operator*() noexcept
    {
        return _payload;
    }

    template <typename T>
    inline const T& unique_resource<T>::operator*() const noexcept
    {
        return _payload;
    }

    template <typename T>
    inline T& unique_resource<T>::get() noexcept
    {
        return _payload;
    }

    template <typename T>
    inline const T& unique_resource<T>::get() const noexcept
    {
        return _payload;
    }

    template <typename T>
    inline void unique_resource<T>::reset(T value) noexcept
    {
    }

    template <typename T>
    inline T unique_resource<T>::release() noexcept
    {
        _allocator = nullptr;
        return _payload;
    }

    template <typename T>
    inline void unique_resource<T>::swap(unique_resource<T>& rhs) noexcept
    {
        std::swap(_allocator, rhs._allocator);
        std::swap(_payload, rhs._payload);
    }
} // namespace tempest::graphics

#endif // tempeest_graphics_resource_handle_hpp