#ifndef tempest_core_queue_hpp
#define tempest_core_queue_hpp

#include <tempest/deque.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    /// @brief Container adapter that gives the programmer the functionality of a queue -
    /// specifically, a FIFO (first-in, first-out) data structure.
    /// @tparam T The type of the stored elements.
    /// @tparam Container The type of the underlying container to use to store the elements.
    template <typename T, typename Container = deque<T>>
    class queue
    {
      public:
        using container_type = Container;
        using value_type = typename Container::value_type;
        using size_type = typename Container::size_type;
        using reference = typename Container::reference;
        using const_reference = typename Container::const_reference;

        /// @brief Default constructor. Value-initializes the underlying container.
        queue() = default;

        /// @brief Copy-constructs the underlying container with the contents of cont.
        /// @param cont Container to copy.
        explicit queue(const Container& cont)
            requires is_copy_constructible_v<Container>
            : c(cont)
        {
        }

        /// @brief Move-constructs the underlying container with the contents of cont.
        /// @param cont Container to move.
        explicit queue(Container&& cont) : c(tempest::move(cont))
        {
        }

        /// @brief Copy constructor.
        queue(const queue& other) = default;

        /// @brief Move constructor.
        queue(queue&& other) noexcept = default;

        /// @brief Copy assignment operator.
        queue& operator=(const queue& other) = default;

        /// @brief Move assignment operator.
        queue& operator=(queue&& other) noexcept = default;

        ~queue() = default;

        /// @brief Returns reference to the first element in the queue.
        [[nodiscard]] reference front()
        {
            return c.front();
        }

        /// @brief Returns const reference to the first element in the queue.
        [[nodiscard]] const_reference front() const
        {
            return c.front();
        }

        /// @brief Returns reference to the last element in the queue.
        [[nodiscard]] reference back()
        {
            return c.back();
        }

        /// @brief Returns const reference to the last element in the queue.
        [[nodiscard]] const_reference back() const
        {
            return c.back();
        }

        /// @brief Checks if the underlying container has no elements.
        [[nodiscard]] bool empty() const noexcept
        {
            return c.empty();
        }

        /// @brief Returns the number of elements in the underlying container.
        [[nodiscard]] size_type size() const noexcept
        {
            return c.size();
        }

        /// @brief Pushes the given element value to the end of the queue.
        /// @param value The value of the element to push.
        void push(const T& value)
            requires is_copy_constructible_v<value_type>
        {
            c.push_back(value);
        }

        /// @brief Moves the given element value to the end of the queue.
        /// @param value The rvalue of the element to push.
        void push(T&& value)
        {
            c.push_back(tempest::move(value));
        }

        /// @brief Pushes a new element to the end of the queue. The element is constructed in-place.
        /// @tparam ...Args Types of arguments to forward to the constructor of the element.
        /// @param ...args Arguments to forward to the constructor of the element.
        /// @return Reference to the emplaced element.
        template <typename... Args>
        decltype(auto) emplace(Args&&... args)
        {
            return c.emplace_back(tempest::forward<Args>(args)...);
        }

        /// @brief Removes the first element of the queue.
        void pop()
        {
            c.pop_front();
        }

        /// @brief Exchanges the contents of the container adaptor with those of other.
        /// @param other Container adaptor to exchange the contents with.
        void swap(queue& other) noexcept(is_nothrow_swappable_v<Container>)
        {
            using tempest::swap;
            swap(c, other.c);
        }

        template <typename T1, typename Container1>
        friend constexpr auto operator==(const queue<T1, Container1>& lhs, const queue<T1, Container1>& rhs) -> bool;

        template <typename T1, typename Container1>
        friend constexpr auto operator<=>(const queue<T1, Container1>& lhs, const queue<T1, Container1>& rhs)
            -> decltype(lhs.c <=> rhs.c);

      protected:
        Container c;
    };

    template <typename Container>
    queue(Container) -> queue<typename Container::value_type, Container>;

    /// @brief Specializes the tempest::swap algorithm for tempest::queue.
    /// @tparam T Type of the elements.
    /// @tparam Container Type of the underlying container.
    /// @param lhs First queue to swap.
    /// @param rhs Second queue to swap.
    template <typename T, typename Container>
    void swap(queue<T, Container>& lhs, queue<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    /// @brief Checks if the contents of lhs and rhs are equal.
    template <typename T, typename Container>
    constexpr auto operator==(const queue<T, Container>& lhs, const queue<T, Container>& rhs) -> bool
    {
        return lhs.c == rhs.c;
    }

    /// @brief Compares the contents of lhs and rhs lexicographically.
    template <typename T, typename Container>
    constexpr auto operator<=>(const queue<T, Container>& lhs, const queue<T, Container>& rhs)
        -> decltype(lhs.c <=> rhs.c)
    {
        return lhs.c <=> rhs.c;
    }
} // namespace tempest

#endif // tempest_core_queue_hpp
