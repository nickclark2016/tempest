#ifndef tempest_ecs_traits_hpp
#define tempest_ecs_traits_hpp

#include <bit>
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace tempest::ecs
{
    namespace detail
    {
        template <typename>
        struct entity_handle_traits;

        template <typename T>
            requires std::is_enum_v<T>
        struct entity_handle_traits<T> : entity_handle_traits<std::underlying_type_t<T>>
        {
            using value_type = T;
        };

        template <typename T>
            requires std::is_class_v<T>
        struct entity_handle_traits<T> : entity_handle_traits<typename T::entity_type>
        {
            using value_type = T;
        };

        template <>
        struct entity_handle_traits<std::uint64_t>
        {
            using value_type = std::uint64_t;
            using entity_type = std::uint64_t;
            using version_type = std::uint32_t;

            static constexpr entity_type entity_mask = 0xFFFFFFFF;
            static constexpr entity_type version_mask = 0xFFFFFFFF;
        };
    } // namespace detail

    template <typename T>
        requires std::is_trivial_v<T>
    class basic_entity_traits
    {
        static constexpr auto length = std::popcount(T::entity_mask);

      public:
        using value_type = typename T::value_type;
        using entity_type = typename T::entity_type;
        using version_type = typename T::version_type;

        static constexpr entity_type entity_mask = T::entity_mask;
        static constexpr entity_type version_mask = T::version_mask;

        [[nodiscard]] static constexpr entity_type as_integral(const value_type v) noexcept
        {
            return static_cast<entity_type>(v);
        }

        [[nodiscard]] static constexpr entity_type as_entity(const value_type v) noexcept
        {
            return static_cast<entity_type>(as_integral(v) & entity_mask);
        }

        [[nodiscard]] static constexpr version_type as_version(const value_type v) noexcept
        {
            return static_cast<version_type>(static_cast<version_type>(as_integral(v) >> length) & version_mask);
        }

        [[nodiscard]] static constexpr value_type next_version(const value_type v) noexcept
        {
            const auto version = as_version(v) + 1;
            return construct(as_integral(v), static_cast<version_type>(version + (version == version_mask)));
        }

        [[nodiscard]] static constexpr value_type construct(const entity_type e, const version_type v) noexcept
        {
            auto e_id = e & entity_mask;
            auto v_id = v & version_mask;
            return value_type{e_id | (v_id << length)};
        }

        [[nodiscard]] static constexpr value_type combine_entities(const entity_type lhs,
                                                                   const entity_type rhs) noexcept
        {
            return value_type{(lhs & entity_mask) | (rhs & (version_mask << length))};
        }
    };

    template <typename T>
    struct entity_traits : basic_entity_traits<detail::entity_handle_traits<T>>
    {
        using base_type = basic_entity_traits<detail::entity_handle_traits<T>>;

        static constexpr std::size_t page_size = 1024;
    };

    struct null_t
    {
        template <typename E>
        [[nodiscard]] constexpr operator E() const noexcept
        {
            using traits_type = entity_traits<E>;
            constexpr auto value = traits_type::construct(traits_type::entity_mask, traits_type::version_mask);
            return value;
        }

        [[nodiscard]] constexpr bool operator==([[maybe_unused]] const null_t) const noexcept
        {
            return true;
        }

        [[nodiscard]] constexpr bool operator!=([[maybe_unused]] const null_t) const noexcept
        {
            return false;
        }

        template <typename E>
        [[nodiscard]] constexpr bool operator==(const E e) const noexcept
        {
            using traits_type = entity_traits<E>;
            return traits_type::as_entity(e) == traits_type::as_entity(*this);
        }

        template <typename E>
        [[nodiscard]] constexpr bool operator!=(const E e) const noexcept
        {
            return !(e == *this);
        }
    };

    template <typename E>
    [[nodiscard]] constexpr bool operator==(const E lhs, const null_t rhs) noexcept
    {
        return rhs.operator==(lhs);
    }

    template <typename E>
    [[nodiscard]] constexpr bool operator!=(const E lhs, const null_t rhs) noexcept
    {
        return !(lhs == rhs);
    }

    struct tombstone_t
    {
        template <typename E>
        [[nodiscard]] constexpr operator E() const noexcept
        {
            using traits_type = entity_traits<E>;
            constexpr auto value = traits_type::construct(traits_type::entity_mask, traits_type::version_mask);
            return value;
        }

        [[nodiscard]] constexpr bool operator==([[maybe_unused]] const null_t) const noexcept
        {
            return true;
        }

        [[nodiscard]] constexpr bool operator!=([[maybe_unused]] const null_t) const noexcept
        {
            return false;
        }

        template <typename E>
        [[nodiscard]] constexpr bool operator==(const E e) const noexcept
        {
            using traits_type = entity_traits<E>;
            return traits_type::as_entity(e) == traits_type::as_entity(*this);
        }

        template <typename E>
        [[nodiscard]] constexpr bool operator!=(const E e) const noexcept
        {
            return !(e == *this);
        }
    };

    template <typename E>
    [[nodiscard]] constexpr bool operator==(const E lhs, const tombstone_t rhs) noexcept
    {
        return rhs.operator==(lhs);
    }

    template <typename E>
    [[nodiscard]] constexpr bool operator!=(const E lhs, const tombstone_t rhs) noexcept
    {
        return !(lhs == rhs);
    }

    inline constexpr null_t null{};
    inline constexpr tombstone_t tombstone{};

    enum class entity : std::uint64_t
    {
    };
} // namespace tempest::ecs

#endif // tempest_ecs_traits_hpp