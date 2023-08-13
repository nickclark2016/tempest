#ifndef tempest_ecs_archetype_hpp
#define tempest_ecs_archetype_hpp

#include <concepts>
#include <cstddef>
#include <memory>
#include <tuple>

namespace tempest::ecs
{
    template <std::size_t N, std::default_initializable... Ts> struct archetype_storage
    {
        struct chunk
        {
            std::tuple<Ts[N]...> values;
        };

        void extend(std::size_t len);
        std::tuple<Ts...>& value(std::size_t idx);
        const std::tuple<Ts...>& value(std::size_t idx) const;

        std::unique_ptr<chunk[]> chunks;
        std::size_t capacity{0};
        std::size_t length{0};
    };

    template <std::size_t N, std::default_initializable... Ts> struct entity
    {
        std::size_t id;
    };

    template <std::size_t N, std::default_initializable... Ts> class archetype
    {
      public:
        inline static constexpr auto storage_size = N;
        using storage_type = archetype_storage<N, Ts...>;

        [[nodiscard]] entity<N, Ts...> allocate();
        [[nodiscard]] std::size_t entity_count() const noexcept;
        [[nodiscard]] std::size_t entity_capacity() const noexcept;

        [[nodiscard]] typename archetype_storage<N, Ts...>::chunk& get_chunk(std::size_t idx) noexcept;
        [[nodiscard]] const typename archetype_storage<N, Ts...>::chunk& get_chunk(std::size_t idx) const noexcept;

      private:
        archetype_storage<N, Ts...> _storage;
    };

    namespace detail
    {
        template <std::size_t N, typename T, typename... Ts>
        std::remove_extent_t<T>& get_elem(std::tuple<Ts[N]...>& tup, std::size_t i)
        {
            return std::get<T[N]>(tup)[i];
        }

        template <std::size_t N, typename T, typename... Ts>
        const std::remove_extent_t<T>& get_elem(const std::tuple<Ts[N]...>& tup, std::size_t i)
        {
            return std::get<T[N]>(tup)[i];
        }
    } // namespace detail

    template <typename Fn, std::size_t N, std::default_initializable... Ts>
        requires std::invocable<Fn, std::tuple<Ts&...>>
    void for_each_mut(archetype<N, Ts...>& arch, Fn&& fn)
    {
        std::size_t cap = arch.entity_count();
        std::size_t aligned_cap = cap / N;
        std::size_t left_over = cap % N;

        for (std::size_t i = 0; i < aligned_cap; ++i)
        {
            typename archetype_storage<N, Ts...>::chunk& chunk = arch.get_chunk(i);

            for (std::size_t elem = 0; elem < N; ++elem)
            {
                auto refs = std::tie(detail::get_elem<N, Ts>(chunk.values, elem)...);
                fn(refs);
            }
        }

        if (left_over)
        {
            typename archetype_storage<N, Ts...>::chunk& chunk = arch.get_chunk(aligned_cap);

            for (std::size_t elem = 0; elem < left_over; ++elem)
            {
                auto refs = std::tie(detail::get_elem<N, Ts>(chunk.values, elem)...);
                fn(refs);
            }
        }
    }

    template <typename Fn, std::size_t N, std::default_initializable... Ts>
        requires std::invocable<Fn, std::tuple<const Ts&...>>
    void for_each(const archetype<N, Ts...>& arch, Fn&& fn)
    {
        std::size_t cap = arch.entity_count();
        std::size_t aligned_cap = cap / N;
        std::size_t left_over = cap % N;

        for (std::size_t i = 0; i < aligned_cap; ++i)
        {
            const typename archetype_storage<N, Ts...>::chunk& chunk = arch.get_chunk(i);

            for (std::size_t elem = 0; elem < N; ++elem)
            {
                auto refs = std::tie(detail::get_elem<N, Ts>(chunk.values, elem)...);
                fn(refs);
            }
        }

        if (left_over)
        {
            const typename archetype_storage<N, Ts...>::chunk& chunk = arch.get_chunk(aligned_cap);

            for (std::size_t elem = 0; elem < left_over; ++elem)
            {
                auto refs = std::tie(detail::get_elem<N, Ts>(chunk.values, elem)...);
                fn(refs);
            }
        }
    }

    template <std::default_initializable... Query>
    void for_each_mut_select(auto& arch, std::invocable<Query&...> auto&& fn)
    {
        using Arch = std::remove_reference_t<decltype(arch)>;
        static constexpr auto N = Arch::storage_size;

        std::size_t cap = arch.entity_count();
        std::size_t aligned_cap = cap / N;
        std::size_t left_over = cap % N;

        for (std::size_t i = 0; i < aligned_cap; ++i)
        {
            typename Arch::storage_type::chunk& chunk = arch.get_chunk(i);

            for (std::size_t elem = 0; elem < N; ++elem)
            {
                auto refs = std::tie(detail::get_elem<N, std::remove_reference_t<Query>>(chunk.values, elem)...);
                fn(refs);
            }
        }

        if (left_over)
        {
            typename Arch::storage_type::chunk& chunk = arch.get_chunk(aligned_cap);

            for (std::size_t elem = 0; elem < left_over; ++elem)
            {
                auto refs = std::tie(detail::get_elem<N, std::remove_reference_t<Query>>(chunk.values, elem)...);
                fn(refs);
            }
        }
    }

    template <std::default_initializable... Query>
    void for_each_select(const auto& arch, std::invocable<const Query&...> auto&& fn)
    {
        using Arch = std::remove_reference_t<decltype(arch)>;
        static constexpr auto N = Arch::storage_size;

        std::size_t cap = arch.entity_count();
        std::size_t aligned_cap = cap / N;
        std::size_t left_over = cap % N;

        for (std::size_t i = 0; i < aligned_cap; ++i)
        {
            const typename Arch::storage_type::chunk& chunk = arch.get_chunk(i);

            for (std::size_t elem = 0; elem < N; ++elem)
            {
                auto refs = std::tie(detail::get_elem<N, std::remove_reference_t<Query>>(chunk.values, elem)...);
                fn(refs);
            }
        }

        if (left_over)
        {
            const typename Arch::storage_type::chunk& chunk = arch.get_chunk(aligned_cap);

            for (std::size_t elem = 0; elem < left_over; ++elem)
            {
                auto refs = std::tie(detail::get_elem<N, std::remove_reference_t<Query>>(chunk.values, elem)...);
                fn(refs);
            }
        }
    }

    template <std::size_t N, std::default_initializable... Ts>
    inline void archetype_storage<N, Ts...>::extend(std::size_t len)
    {
        if (len < capacity)
        {
            return;
        }

        auto new_chunk_count = (len + N - 1) / N;
        auto old_chunk_count = (capacity + N - 1) / N;

        auto new_chunks = std::make_unique<chunk[]>(new_chunk_count);
        std::uninitialized_move_n(chunks.get(), old_chunk_count, new_chunks.get());
        chunks = std::move(new_chunks);

        capacity = new_chunk_count * N;
    }

    template <std::size_t N, std::default_initializable... Ts>
    inline std::tuple<Ts...>& archetype_storage<N, Ts...>::value(std::size_t idx)
    {
        return chunks[idx / N].values[idx % N];
    }

    template <std::size_t N, std::default_initializable... Ts>
    inline const std::tuple<Ts...>& archetype_storage<N, Ts...>::value(std::size_t idx) const
    {
        return chunks[idx / N].values[idx % N];
    }

    template <std::size_t N, std::default_initializable... Ts> inline entity<N, Ts...> archetype<N, Ts...>::allocate()
    {
        _storage.extend(_storage.length + 1);
        entity<N, Ts...> e{.id{_storage.length}};
        ++_storage.length;
        return e;
    }

    template <std::size_t N, std::default_initializable... Ts>
    inline std::size_t archetype<N, Ts...>::entity_count() const noexcept
    {
        return _storage.length;
    }

    template <std::size_t N, std::default_initializable... Ts>
    inline std::size_t archetype<N, Ts...>::entity_capacity() const noexcept
    {
        return _storage.capacity;
    }
    template <std::size_t N, std::default_initializable... Ts>
    inline typename archetype_storage<N, Ts...>::chunk& archetype<N, Ts...>::get_chunk(std::size_t idx) noexcept
    {
        return _storage.chunks[idx];
    }

    template <std::size_t N, std::default_initializable... Ts>
    inline const typename archetype_storage<N, Ts...>::chunk& archetype<N, Ts...>::get_chunk(
        std::size_t idx) const noexcept
    {
        return _storage.chunks[idx];
    }
} // namespace tempest::ecs

#endif // tempest_ecs_archetype_hpp