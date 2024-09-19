#ifndef tempest_ecs_registry_hpp
#define tempest_ecs_registry_hpp

#include "sparse.hpp"
#include "traits.hpp"

#include <tempest/algorithm.hpp>
#include <tempest/meta.hpp>
#include <tempest/vector.hpp>

#include <array>
#include <cassert>
#include <climits>
#include <compare>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>

namespace tempest::ecs
{
    template <typename T>
    class basic_registry;

    namespace detail
    {
        /**
         * @brief A bidirectional iterator for basic entity stores.
         *
         * @tparam T The type of the value the iterator points to.
         * @tparam EPC The number of entities per chunk.
         * @tparam EPB The number of entities per block.
         * @tparam BPC The number of blocks per chunk.
         *
         * This struct defines a basic iterator for entity stores in an ECS (Entity Component System). It provides
         * the functionality to iterate over entities in a chunked storage system, where entities are stored in blocks,
         * and blocks are grouped into chunks.
         *
         * The iterator supports bidirectional iteration, meaning it can increment and decrement.
         *
         * @see tempest::ecs::basic_entity_store
         */
        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        struct basic_entity_store_iterator
        {
            // Iterator traits
            using value_type = T::value_type;
            using pointer = value_type*;
            using const_pointer = const value_type*;
            using reference = value_type&;
            using const_reference = const value_type&;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::bidirectional_iterator_tag;

            // Named constants
            static constexpr std::size_t entities_per_chunk = EPC;
            static constexpr std::size_t entities_per_block = EPB;
            static constexpr std::size_t blocks_per_chunk = BPC;

            /**
             * @brief Default constructor.
             */
            constexpr basic_entity_store_iterator() noexcept = default;

            /**
             * @brief Construct a new basic entity store iterator object.
             *
             * @param chunks A pointer to the chunks in the entity store.
             * @param index The index of the entity the iterator points to.
             * @param end The end index of the entity store.  This is equivalent to the total number of entities in the
             * store.
             */
            constexpr basic_entity_store_iterator(T* chunks, std::size_t index, std::size_t end) noexcept;

            /**
             * @brief Dereference operator.
             */
            [[nodiscard]] constexpr value_type operator*() const noexcept;

            /**
             * @brief Pre-increment operator.
             */
            constexpr basic_entity_store_iterator& operator++() noexcept;

            /**
             * @brief Post-increment operator.
             */
            constexpr basic_entity_store_iterator operator++(int) noexcept;

            /**
             * @brief Pre-decrement operator.
             */
            constexpr basic_entity_store_iterator& operator--() noexcept;

            /**
             * @brief Post-decrement operator.
             */
            constexpr basic_entity_store_iterator operator--(int) noexcept;

            /**
             * Pointer to an array of chunks.
             */
            T* chunks{nullptr};

            /**
             * The index of the entity the iterator points to.
             */
            std::size_t index{0};

            /**
             * The end index of the entity store.
             */
            std::size_t end{0};
        };

        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        inline constexpr basic_entity_store_iterator<T, EPC, EPB, BPC>::basic_entity_store_iterator(
            T* chunks, std::size_t index, std::size_t end) noexcept
            : chunks{chunks}, index{index}, end{end}
        {
        }

        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        inline constexpr basic_entity_store_iterator<T, EPC, EPB, BPC>::value_type basic_entity_store_iterator<
            T, EPC, EPB, BPC>::operator*() const noexcept
        {
            auto chunk_index = index / entities_per_chunk;
            auto chunk_offset = index % entities_per_chunk;

            auto block_index = chunk_offset / entities_per_block;
            auto block_offset = chunk_offset % entities_per_block;

            return chunks[chunk_index].blocks[block_index].entities[block_offset];
        }

        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        inline constexpr basic_entity_store_iterator<T, EPC, EPB, BPC>& basic_entity_store_iterator<
            T, EPC, EPB, BPC>::operator++() noexcept
        {
            ++index;

            while (index < end)
            {
                // TODO: Figure out faster iteration
                auto chunk_index = index / entities_per_chunk;
                auto chunk_offset = index % entities_per_chunk;

                auto block_index = chunk_offset / entities_per_block;
                auto block_offset = chunk_offset % entities_per_block;

                if (tempest::is_bit_set(chunks[chunk_index].blocks[block_index].occupancy, block_offset))
                {
                    break;
                }

                ++index;
            }

            return *this;
        }

        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        inline constexpr basic_entity_store_iterator<T, EPC, EPB, BPC> basic_entity_store_iterator<
            T, EPC, EPB, BPC>::operator++(int) noexcept
        {
            auto self = *this;
            ++(*this);
            return self;
        }

        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        inline constexpr basic_entity_store_iterator<T, EPC, EPB, BPC>& basic_entity_store_iterator<
            T, EPC, EPB, BPC>::operator--() noexcept
        {
            while (index > 0)
            {
                --index;

                auto chunk_index = index / entities_per_chunk;
                auto chunk_offset = index % entities_per_chunk;

                auto block_index = chunk_offset / entities_per_block;
                auto block_offset = chunk_offset % entities_per_block;

                if (tempest::is_bit_set(chunks[chunk_index].blocks[block_index].occupancy, block_offset))
                {
                    break;
                }
            }

            return *this;
        }

        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        inline constexpr basic_entity_store_iterator<T, EPC, EPB, BPC> basic_entity_store_iterator<
            T, EPC, EPB, BPC>::operator--(int) noexcept
        {
            auto self = *this;
            --(*this);
            return self;
        }

        /**
         * @brief Equality operator.
         *
         * @tparam T The type of the value the iterator points to.
         * @tparam EPC The number of entities per chunk.
         * @tparam EPB The number of entities per block.
         * @tparam BPC The number of blocks per chunk.
         *
         * Checks for equality of two iterators by the index they point to. If the iterators were generated from
         * different basic_entity_stores, this function is undefined.
         *
         * @param lhs The left hand side iterator.
         * @param rhs The right hand side iterator.
         * @return true if the iterators point to the same index, false otherwise.
         */
        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        [[nodiscard]] inline constexpr bool operator==(
            const basic_entity_store_iterator<T, EPC, EPB, BPC>& lhs,
            const basic_entity_store_iterator<T, EPC, EPB, BPC>& rhs) noexcept
        {
            return lhs.index == rhs.index;
        }

        /**
         * @brief Three-way comparison operator.
         *
         * @tparam T The type of the value the iterator points to.
         * @tparam EPC The number of entities per chunk.
         * @tparam EPB The number of entities per block.
         * @tparam BPC The number of blocks per chunk.
         *
         * Compares two iterators by the index they point to. If the iterators were generated from different
         * basic_entity_stores, this function is undefined.
         *
         * @param lhs The left hand side iterator.
         * @param rhs The right hand side iterator.
         * @return std::strong_ordering representing the comparison result of the indices.
         */
        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        [[nodiscard]] inline constexpr auto operator<=>(
            const basic_entity_store_iterator<T, EPC, EPB, BPC>& lhs,
            const basic_entity_store_iterator<T, EPC, EPB, BPC>& rhs) noexcept
        {
            return lhs.index <=> rhs.index;
        }
    } // namespace detail

    template <typename T, std::size_t N, std::integral O>
    class basic_entity_store
    {
      public:
        static constexpr std::size_t entities_per_chunk = N;
        static constexpr std::size_t entities_per_block = sizeof(O) * CHAR_BIT;
        static constexpr std::size_t blocks_per_chunk = entities_per_chunk / entities_per_block;

        struct block
        {
            O occupancy{0};
            std::array<T, entities_per_block> entities{};
        };

        struct chunk
        {
            using value_type = T;

            std::array<block, blocks_per_chunk> blocks;
        };

        using traits_type = entity_traits<T>;
        using entity_type = typename traits_type::value_type;
        using version_type = typename traits_type::version_type;

        using size_type = std::size_t;

        using iterator =
            detail::basic_entity_store_iterator<chunk, entities_per_chunk, entities_per_block, blocks_per_chunk>;
        using const_iterator =
            detail::basic_entity_store_iterator<const chunk, entities_per_chunk, entities_per_block, blocks_per_chunk>;

        constexpr basic_entity_store() = default;
        constexpr explicit basic_entity_store(std::size_t initial_capacity);

        [[nodiscard]] constexpr size_type size() const noexcept;
        [[nodiscard]] constexpr size_type capacity() const noexcept;
        [[nodiscard]] constexpr bool empty() const noexcept;

        [[nodiscard]] constexpr iterator begin() noexcept;
        [[nodiscard]] constexpr const_iterator begin() const noexcept;
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept;

        [[nodiscard]] constexpr iterator end() noexcept;
        [[nodiscard]] constexpr const_iterator end() const noexcept;
        [[nodiscard]] constexpr const_iterator cend() const noexcept;

        [[nodiscard]] constexpr T acquire();
        constexpr void release(T e) noexcept;
        [[nodiscard]] constexpr bool is_valid(T e) const noexcept;
        constexpr void clear() noexcept;
        void reserve(std::size_t new_capacity);

      private:
        vector<chunk> _chunks;
        T _head = null;

        std::size_t _count{0};
    };

    template <typename T, std::size_t N, std::integral O>
    inline constexpr basic_entity_store<T, N, O>::basic_entity_store(std::size_t initial_capacity)
    {
        reserve(initial_capacity);
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr basic_entity_store<T, N, O>::size_type basic_entity_store<T, N, O>::size() const noexcept
    {
        return _count;
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr basic_entity_store<T, N, O>::size_type basic_entity_store<T, N, O>::capacity() const noexcept
    {
        return _chunks.size() * entities_per_chunk;
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr bool basic_entity_store<T, N, O>::empty() const noexcept
    {
        return size() == 0;
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr basic_entity_store<T, N, O>::iterator basic_entity_store<T, N, O>::begin() noexcept
    {
        for (std::size_t chunk_idx = 0; chunk_idx < _chunks.size(); ++chunk_idx)
        {
            for (std::size_t block_idx = 0; block_idx < blocks_per_chunk; ++block_idx)
            {
                for (std::size_t block_offset = 0; block_offset < entities_per_block; ++block_offset)
                {
                    if (tempest::is_bit_set(_chunks[chunk_idx].blocks[block_idx].occupancy, block_offset))
                    {
                        return iterator(_chunks.data(),
                                        chunk_idx * entities_per_chunk + block_idx * entities_per_block + block_offset,
                                        capacity());
                    }
                }
            }
        }
        return end();
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr basic_entity_store<T, N, O>::const_iterator basic_entity_store<T, N, O>::begin() const noexcept
    {
        for (std::size_t chunk_idx = 0; chunk_idx < _chunks.size(); ++chunk_idx)
        {
            for (std::size_t block_idx = 0; block_idx < blocks_per_chunk; ++block_idx)
            {
                for (std::size_t block_offset = 0; block_offset < entities_per_block; ++block_offset)
                {
                    if (tempest::is_bit_set(_chunks[chunk_idx].blocks[block_idx].occupancy, block_offset))
                    {
                        return const_iterator(
                            _chunks.data(),
                            chunk_idx * entities_per_chunk + block_idx * entities_per_block + block_offset, capacity());
                    }
                }
            }
        }
        return end();
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr basic_entity_store<T, N, O>::const_iterator basic_entity_store<T, N, O>::cbegin() const noexcept
    {
        return begin();
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr basic_entity_store<T, N, O>::iterator basic_entity_store<T, N, O>::end() noexcept
    {
        return iterator(_chunks.data(), capacity(), capacity());
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr basic_entity_store<T, N, O>::const_iterator basic_entity_store<T, N, O>::end() const noexcept
    {
        return const_iterator(_chunks.data(), capacity(), capacity());
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr basic_entity_store<T, N, O>::const_iterator basic_entity_store<T, N, O>::cend() const noexcept
    {
        return const_iterator(_chunks.data(), capacity(), capacity());
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr T basic_entity_store<T, N, O>::acquire()
    {
        if (size() == capacity())
        {
            reserve(size() + 1);
        }

        assert(_head != null);
        T ent = _head;

        auto index = traits_type::as_entity(ent);

        auto chunk_index = index / entities_per_chunk;
        auto chunk_offset = index % entities_per_chunk;

        auto block_index = chunk_offset / entities_per_block;
        auto block_offset = chunk_offset % entities_per_block;

        block& blk = _chunks[chunk_index].blocks[block_index];
        T next = blk.entities[block_offset];
        assert(next != null);
        _head = next;

        blk.occupancy = tempest::set_bit(blk.occupancy, block_offset);

        auto result = traits_type::construct(traits_type::as_entity(ent), traits_type::as_version(next));
        blk.entities[block_offset] = result;

        ++_count;

        return result;
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr void basic_entity_store<T, N, O>::release(T e) noexcept
    {
        auto index = traits_type::as_entity(e);

        auto chunk_index = index / entities_per_chunk;
        auto chunk_offset = index % entities_per_chunk;

        auto block_index = chunk_offset / entities_per_block;
        auto block_offset = chunk_offset % entities_per_block;

        block& blk = _chunks[chunk_index].blocks[block_index];

        blk.occupancy = tempest::clear_bit(blk.occupancy, block_offset);

        auto head_index = traits_type::as_entity(_head);
        T& to_erase = blk.entities[block_offset];
        to_erase = traits_type::construct(head_index,
                                          traits_type::as_version(to_erase) + 1); // point entity to next, bump version

        _head = e;
        --_count;
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr bool basic_entity_store<T, N, O>::is_valid(T e) const noexcept
    {
        auto index = traits_type::as_entity(e);

        auto chunk_index = index / entities_per_chunk;
        auto chunk_offset = index % entities_per_chunk;

        auto block_index = chunk_offset / entities_per_block;
        auto block_offset = chunk_offset % entities_per_block;

        if (chunk_index < _chunks.size())
        {
            const block& blk = _chunks[chunk_index].blocks[block_index];
            return tempest::is_bit_set(blk.occupancy, block_offset) &&
                   traits_type::as_version(blk.entities[block_offset]) == traits_type::as_version(e);
        }

        return false;
    }

    template <typename T, std::size_t N, std::integral O>
    inline constexpr void basic_entity_store<T, N, O>::clear() noexcept
    {
        std::size_t index = 0;
        for (auto& chunk : _chunks)
        {
            for (auto& block : chunk.blocks)
            {
                block.occupancy = 0;
                for (auto& entity : block.entities)
                {
                    entity = traits_type::construct(index + 1, traits_type::as_version(entity) +
                                                                   1); // point entity to next, bump version
                    ++index;
                }
            }
        }

        _count = 0;

        if (!_chunks.empty())
        {
            _head = traits_type::construct(0, traits_type::as_version(_chunks[0].blocks[0].entities[0]));
        }
        else
        {
            _head = null;
        }
    }

    template <typename T, std::size_t N, std::integral O>
    inline void basic_entity_store<T, N, O>::reserve(std::size_t new_capacity)
    {
        if (new_capacity <= capacity())
        {
            return;
        }

        std::size_t new_chunks = (new_capacity + entities_per_chunk - 1) / entities_per_chunk;

        std::size_t current_cap = capacity();
        std::size_t current_chunk_count = _chunks.size();

        _chunks.resize(new_chunks);

        // build next chain
        auto idx = current_cap;
        for (std::size_t chunk_idx = current_chunk_count; chunk_idx < new_chunks; ++chunk_idx)
        {
            for (std::size_t block_idx = 0; block_idx < blocks_per_chunk; ++block_idx)
            {
                for (std::size_t block_offset = 0; block_offset < entities_per_block; ++block_offset)
                {
                    _chunks[chunk_idx].blocks[block_idx].entities[block_offset] =
                        traits_type::construct(idx + 1, 0); // new blocks have all entities as version 0
                    ++idx;
                }
            }
        }

        // attach to current chain
        auto last_chunk = _chunks.size() - 1;
        auto last_block = blocks_per_chunk - 1;
        auto last_offset = entities_per_block - 1;
        _chunks[last_chunk].blocks[last_block].entities[last_offset] = traits_type::construct(current_cap, 0);
        _head = traits_type::construct(current_cap, 0);
    }

    namespace detail
    {
        template <typename E, typename... Ts>
        struct basic_component_view_iterator
        {
            basic_registry<E>* source;
            basic_registry<E>::store_type::iterator current;
            basic_registry<E>::store_type::iterator end;

            using reference = std::tuple<E, Ts&...>;
            using const_reference = std::tuple<E, const Ts&...>;

            reference operator*() noexcept;
            const_reference operator*() const noexcept;

            basic_component_view_iterator& operator++() noexcept;
            basic_component_view_iterator operator++(int) noexcept;
            basic_component_view_iterator& operator--() noexcept;
            basic_component_view_iterator operator--(int) noexcept;
        };

        template <typename E, typename... Ts>
        struct basic_component_view
        {
            basic_registry<E>* source;

            basic_component_view(basic_registry<E>* source) noexcept;

            using iterator = basic_component_view_iterator<E, Ts...>;
            using const_iterator = basic_component_view_iterator<E, const Ts...>;

            iterator begin() noexcept;
            const_iterator begin() const noexcept;
            const_iterator cbegin() const noexcept;

            iterator end() noexcept;
            const_iterator end() const noexcept;
            const_iterator cend() const noexcept;
        };

        template <typename E, typename... Ts>
        struct basic_component_const_view_iterator
        {
            const basic_registry<E>* source;
            basic_registry<E>::store_type::const_iterator current;
            basic_registry<E>::store_type::const_iterator end;

            using reference = std::tuple<E, Ts&...>;
            using const_reference = std::tuple<E, const Ts&...>;

            reference operator*() noexcept;
            const_reference operator*() const noexcept;

            basic_component_const_view_iterator& operator++() noexcept;
            basic_component_const_view_iterator operator++(int) noexcept;
            basic_component_const_view_iterator& operator--() noexcept;
            basic_component_const_view_iterator operator--(int) noexcept;
        };

        template <typename E, typename... Ts>
        struct basic_component_const_view
        {
            const basic_registry<E>* source;

            basic_component_const_view(const basic_registry<E>* source) noexcept;

            using iterator = basic_component_const_view_iterator<E, Ts...>;
            using const_iterator = basic_component_const_view_iterator<E, const Ts...>;

            iterator begin() noexcept;
            const_iterator begin() const noexcept;
            const_iterator cbegin() const noexcept;

            iterator end() noexcept;
            const_iterator end() const noexcept;
            const_iterator cend() const noexcept;
        };
    } // namespace detail

    using entity_store = basic_entity_store<entity, 4096, std::uint64_t>;

    template <typename E>
    class basic_registry
    {
      public:
        using entity_type = E;
        using traits_type = entity_traits<E>;
        using size_type = std::size_t;
        using store_type = entity_store;

        void reserve(std::size_t new_capacity);

        E acquire_entity();
        void release_entity(E e);

        bool is_valid(E e) const noexcept;
        std::size_t entity_count() const noexcept;

        template <typename T>
        void assign(E e, const T& value);

        template <typename T>
        [[nodiscard]] bool has(E e) const noexcept;

        template <typename T1, typename T2, typename... Ts>
        [[nodiscard]] bool has(E e) const noexcept;

        template <typename T>
        [[nodiscard]] T& get(E e);

        template <typename T>
        [[nodiscard]] const T& get(E e) const;

        template <typename T1, typename T2, typename... Ts>
        [[nodiscard]] std::tuple<T1&, T2&, Ts&...> get(E e);

        template <typename T1, typename T2, typename... Ts>
        [[nodiscard]] std::tuple<const T1&, const T2&, const Ts&...> get(E e) const;

        template <typename T>
        [[nodiscard]] T* try_get(E e) noexcept;

        template <typename T>
        [[nodiscard]] const T* try_get(E e) const noexcept;

        template <typename T1, typename T2, typename... Ts>
        [[nodiscard]] std::tuple<T1*, T2*, Ts*...> try_get(E e) noexcept;

        template <typename T1, typename T2, typename... Ts>
        [[nodiscard]] std::tuple<const T1*, const T2*, const Ts*...> try_get(E e) const noexcept;

        template <typename T>
        void remove(E e);

        [[nodiscard]] E duplicate(E e);

        entity_store& entities() noexcept
        {
            return _entities;
        }

        const entity_store& entities() const noexcept
        {
            return _entities;
        }

        [[nodiscard]] std::optional<std::string_view> name(entity e) const;
        void name(entity e, std::string_view n);

        template <typename... Ts>
        detail::basic_component_view<E, Ts...> view();

        template <typename... Ts>
        detail::basic_component_const_view<E, Ts...> view() const;

      private:
        basic_entity_store<E, 4096, std::uint64_t> _entities;
        vector<std::unique_ptr<basic_sparse_map_interface<E>>> _component_stores;

        std::unordered_map<entity, std::string> _name;
    };

    template <typename E>
    inline void basic_registry<E>::reserve(std::size_t new_capacity)
    {
        for (auto& store : _component_stores)
        {
            store->reserve(new_capacity);
        }
        _entities.reserve(new_capacity);
    }

    template <typename E>
    inline E basic_registry<E>::acquire_entity()
    {
        return _entities.acquire();
    }

    template <typename E>
    inline void basic_registry<E>::release_entity(E e)
    {
        for (auto& store : _component_stores)
        {
            store->erase(e);
        }

        _entities.release(e);
    }

    template <typename E>
    inline bool basic_registry<E>::is_valid(E e) const noexcept
    {
        return _entities.is_valid(e);
    }

    template <typename E>
    inline std::size_t basic_registry<E>::entity_count() const noexcept
    {
        return _entities.size();
    }

    template <typename E>
    inline E basic_registry<E>::duplicate(E e)
    {
        auto dup = acquire_entity();
        for (auto& store : _component_stores)
        {
            store->duplicate(e, dup);
        }
        return dup;
    }

    template <typename E>
    inline std::optional<std::string_view> basic_registry<E>::name(entity e) const
    {
        if (auto it = _name.find(e); it != _name.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    template <typename E>
    inline void basic_registry<E>::name(entity e, std::string_view n)
    {
        _name[e] = n;
    }

    template <typename E>
    template <typename T>
    inline void basic_registry<E>::assign(E e, const T& value)
    {
        using type = std::remove_cv_t<T>;

        static core::type_info id = core::type_id<type>();

        if (_component_stores.size() <= id.index())
        {
            _component_stores.resize(id.index() + 1);
        }

        if (_component_stores[id.index()] == nullptr)
        {
            _component_stores[id.index()] = std::make_unique<sparse_map<type>>();
        }

        auto& store = _component_stores[id.index()];

        static_cast<sparse_map<type>*>(store.get())->insert(e, value);
    }

    template <typename E>
    template <typename T>
    inline bool basic_registry<E>::has(E e) const noexcept
    {
        using type = std::remove_cv_t<T>;

        static core::type_info id = core::type_id<type>();

        if (_component_stores.size() <= id.index())
        {
            return false;
        }

        auto& store = _component_stores[id.index()];
        if (store == nullptr)
        {
            return false;
        }

        return static_cast<const sparse_map<type>*>(store.get())->contains(e);
    }

    template <typename E>
    template <typename T1, typename T2, typename... Ts>
    inline bool basic_registry<E>::has(E e) const noexcept
    {
        return has<T1>(e) && has<T2, Ts...>(e);
    }

    template <typename E>
    template <typename T>
    inline T& basic_registry<E>::get(E e)
    {
        using type = std::remove_cv_t<T>;

        static core::type_info id = core::type_id<type>();
        assert(id.index() < _component_stores.size());

        auto& store = _component_stores[id.index()];
        assert(store != nullptr);

        assert(static_cast<sparse_map<type>*>(store.get())->contains(e));

        return static_cast<sparse_map<type>&>(*store.get())[e];
    }

    template <typename E>
    template <typename T>
    inline const T& basic_registry<E>::get(E e) const
    {
        using type = std::remove_cv_t<T>;

        static core::type_info id = core::type_id<type>();
        assert(id.index() < _component_stores.size());

        const auto& store = _component_stores[id.index()];
        assert(store != nullptr);

        assert(static_cast<const sparse_map<type>*>(store.get())->contains(e));

        return static_cast<const sparse_map<type>&>(*store.get())[e];
    }

    template <typename E>
    template <typename T1, typename T2, typename... Ts>
    inline std::tuple<T1&, T2&, Ts&...> basic_registry<E>::get(E e)
    {
        return std::make_tuple(std::ref(get<T1>(e)), std::ref(get<T2>(e)), std::ref(get<Ts>(e))...);
    }

    template <typename E>
    template <typename T1, typename T2, typename... Ts>
    inline std::tuple<const T1&, const T2&, const Ts&...> basic_registry<E>::get(E e) const
    {
        return std::make_tuple(std::cref(get<T1>(e)), std::cref(get<T2>(e)), std::cref(get<Ts>(e))...);
    }

    template <typename E>
    template <typename T>
    inline T* basic_registry<E>::try_get(E e) noexcept
    {
        using type = std::remove_cv_t<T>;

        static core::type_info id = core::type_id<type>();

        if (id.index() >= _component_stores.size())
        {
            return nullptr;
        }

        auto& store = _component_stores[id.index()];
        if (store == nullptr)
        {
            return nullptr;
        }

        if (!static_cast<sparse_map<type>*>(store.get())->contains(e))
        {
            return nullptr;
        }

        return &static_cast<sparse_map<type>&>(*store.get())[e];
    }

    template <typename E>
    template <typename T>
    inline const T* basic_registry<E>::try_get(E e) const noexcept
    {
        using type = std::remove_cv_t<T>;

        static core::type_info id = core::type_id<type>();

        if (id.index() >= _component_stores.size())
        {
            return nullptr;
        }

        const auto& store = _component_stores[id.index()];
        if (store == nullptr)
        {
            return nullptr;
        }

        if (!static_cast<const sparse_map<type>*>(store.get())->contains(e))
        {
            return nullptr;
        }

        return &static_cast<const sparse_map<type>&>(*store.get())[e];
    }

    template <typename E>
    template <typename T1, typename T2, typename... Ts>
    inline std::tuple<T1*, T2*, Ts*...> basic_registry<E>::try_get(E e) noexcept
    {
        return std::make_tuple(try_get<T1>(e), try_get<T2>(e), try_get<Ts>(e)...);
    }

    template <typename E>
    template <typename T1, typename T2, typename... Ts>
    inline std::tuple<const T1*, const T2*, const Ts*...> basic_registry<E>::try_get(E e) const noexcept
    {
        return std::make_tuple(try_get<T1>(e), try_get<T2>(e), try_get<Ts>(e)...);
    }

    template <typename E>
    template <typename T>
    inline void basic_registry<E>::remove(E e)
    {
        using type = std::remove_cv_t<T>;

        static core::type_info id = core::type_id<type>();
        if (id.index() < _component_stores.size())
        {
            auto& store = _component_stores[id.index()];
            if (store != nullptr)
            {
                static_cast<sparse_map<type>*>(store.get())->erase(e);
            }
        }
    }

    template <typename E>
    template <typename... Ts>
    inline detail::basic_component_view<E, Ts...> basic_registry<E>::view()
    {
        return detail::basic_component_view<E, Ts...>(this);
    }

    template <typename E>
    template <typename... Ts>
    inline detail::basic_component_const_view<E, Ts...> basic_registry<E>::view() const
    {
        return detail::basic_component_const_view<E, Ts...>(this);
    }

    namespace detail
    {
        template <typename E, typename... Ts>
        inline typename basic_component_view_iterator<E, Ts...>::reference basic_component_view_iterator<
            E, Ts...>::operator*() noexcept
        {
            auto e = *current;
            if constexpr (sizeof...(Ts) == 0)
            {
                return std::make_tuple(e);
            }
            else if constexpr (sizeof...(Ts) == 1)
            {
                return std::tuple_cat(std::make_tuple(e), std::tuple<Ts&...>(source->template get<Ts...>(e)));
            }
            else
            {
                return std::tuple_cat(std::make_tuple(e), source->template get<Ts...>(e));
            }
        }

        template <typename E, typename... Ts>
        inline basic_component_view_iterator<E, Ts...>::const_reference basic_component_view_iterator<
            E, Ts...>::operator*() const noexcept
        {
            auto e = *current;
            if constexpr (sizeof...(Ts) == 0)
            {
                return std::make_tuple(e);
            }
            else if constexpr (sizeof...(Ts) == 1)
            {
                return std::tuple_cat(std::make_tuple(e), std::tuple<Ts&...>(source->template get<Ts...>(e)));
            }
            else
            {
                return std::tuple_cat(std::make_tuple(e), source->template get<Ts...>(e));
            }
        }

        template <typename T, typename... Ts>
        inline basic_component_view_iterator<T, Ts...>& basic_component_view_iterator<T, Ts...>::operator++() noexcept
        {
            ++current;

            for (; current != end; ++current)
            {
                auto e = *current;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
            }

            return *this;
        }

        template <typename T, typename... Ts>
        inline basic_component_view_iterator<T, Ts...> basic_component_view_iterator<T, Ts...>::operator++(int) noexcept
        {
            auto self = *this;
            ++(*this);
            return self;
        }

        template <typename T, typename... Ts>
        inline basic_component_view_iterator<T, Ts...>& basic_component_view_iterator<T, Ts...>::operator--() noexcept
        {
            for (; current != source->entities().begin(); --current)
            {
                auto e = *current;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
            }

            return *this;
        }

        template <typename T, typename... Ts>
        inline basic_component_view_iterator<T, Ts...> basic_component_view_iterator<T, Ts...>::operator--(int) noexcept
        {
            auto self = *this;
            --(*this);
            return self;
        }

        template <typename T, typename... Ts>
        inline bool operator==(const basic_component_view_iterator<T, Ts...>& lhs,
                               const basic_component_view_iterator<T, Ts...>& rhs) noexcept
        {
            return lhs.current == rhs.current;
        }

        template <typename T, typename... Ts>
        inline bool operator!=(const basic_component_view_iterator<T, Ts...>& lhs,
                               const basic_component_view_iterator<T, Ts...>& rhs) noexcept
        {
            return !(lhs == rhs);
        }

        template <typename E, typename... Ts>
        inline basic_component_view<E, Ts...>::basic_component_view(basic_registry<E>* source) noexcept : source{source}
        {
        }

        template <typename E, typename... Ts>
        inline typename basic_component_view<E, Ts...>::iterator basic_component_view<E, Ts...>::begin() noexcept
        {
            auto begin = source->entities().begin();
            while (begin != source->entities().end())
            {
                auto e = *begin;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
                ++begin;
            }

            return {source, begin, source->entities().end()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_view<E, Ts...>::const_iterator basic_component_view<E, Ts...>::begin()
            const noexcept
        {
            // Get first entity that has all components
            auto begin = source->entities().begin();
            while (begin != source->entities().end())
            {
                auto e = *begin;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
                ++begin;
            }

            return {source, begin, source->entities().end()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_view<E, Ts...>::const_iterator basic_component_view<E, Ts...>::cbegin()
            const noexcept
        {
            auto begin = source->entities().begin();
            while (begin != source->entities().end())
            {
                auto e = *begin;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
                ++begin;
            }

            return {source, begin, source->entities().end()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_view<E, Ts...>::iterator basic_component_view<E, Ts...>::end() noexcept
        {
            return {source, source->entities().end(), source->entities().end()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_view<E, Ts...>::const_iterator basic_component_view<E, Ts...>::end()
            const noexcept
        {
            return {source, source->entities().end(), source->entities().end()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_view<E, Ts...>::const_iterator basic_component_view<E, Ts...>::cend()
            const noexcept
        {
            return {source, source->entities().end(), source->entities().end()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_const_view_iterator<E, Ts...>::reference basic_component_const_view_iterator<
            E, Ts...>::operator*() noexcept
        {
            auto e = *current;
            return std::tuple_cat(std::make_tuple(e), source->template get<typename std::remove_reference_t<Ts>...>(e));
        }

        template <typename E, typename... Ts>
        inline basic_component_const_view_iterator<E, Ts...>::const_reference basic_component_const_view_iterator<
            E, Ts...>::operator*() const noexcept
        {
            auto e = *current;
            return std::tuple_cat(std::make_tuple(e), source->template get<typename std::remove_reference_t<Ts>...>(e));
        }

        template <typename E, typename... Ts>
        inline basic_component_const_view_iterator<E, Ts...>& basic_component_const_view_iterator<
            E, Ts...>::operator++() noexcept
        {
            ++current;

            for (; current != end; ++current)
            {
                auto e = *current;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
            }

            return *this;
        }

        template <typename E, typename... Ts>
        inline basic_component_const_view_iterator<E, Ts...> basic_component_const_view_iterator<E, Ts...>::operator++(
            int) noexcept
        {
            auto self = *this;
            ++(*this);
            return self;
        }

        template <typename E, typename... Ts>
        inline basic_component_const_view_iterator<E, Ts...>& basic_component_const_view_iterator<
            E, Ts...>::operator--() noexcept
        {
            for (; current != source->entities().begin(); --current)
            {
                auto e = *current;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
            }

            return *this;
        }

        template <typename E, typename... Ts>
        inline basic_component_const_view_iterator<E, Ts...> basic_component_const_view_iterator<E, Ts...>::operator--(
            int) noexcept
        {
            auto self = *this;
            --(*this);
            return self;
        }

        template <typename E, typename... Ts>
        inline bool operator==(const basic_component_const_view_iterator<E, Ts...>& lhs,
                               const basic_component_const_view_iterator<E, Ts...>& rhs) noexcept
        {
            return lhs.current == rhs.current;
        }

        template <typename E, typename... Ts>
        inline bool operator!=(const basic_component_const_view_iterator<E, Ts...>& lhs,
                               const basic_component_const_view_iterator<E, Ts...>& rhs) noexcept
        {
            return !(lhs == rhs);
        }

        template <typename E, typename... Ts>
        inline basic_component_const_view<E, Ts...>::basic_component_const_view(
            const basic_registry<E>* source) noexcept
            : source{source}
        {
        }

        template <typename E, typename... Ts>
        inline typename basic_component_const_view<E, Ts...>::iterator basic_component_const_view<
            E, Ts...>::begin() noexcept
        {
            auto begin = source->entities().cbegin();
            while (begin != source->entities().cend())
            {
                const auto e = *begin;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
                ++begin;
            }

            return {&*source, begin, source->entities().end()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_const_view<E, Ts...>::const_iterator basic_component_const_view<
            E, Ts...>::begin() const noexcept
        {
            auto begin = source->entities().cbegin();
            while (begin != source->entities().cend())
            {
                const auto e = *begin;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
                ++begin;
            }

            return {&*source, begin, source->entities().end()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_const_view<E, Ts...>::const_iterator basic_component_const_view<
            E, Ts...>::cbegin() const noexcept
        {
            auto begin = source->entities().cbegin();
            while (begin != source->entities().cend())
            {
                const auto e = *begin;
                if ((source->template has<Ts>(e) && ...))
                {
                    break;
                }
                ++begin;
            }

            return {&*source, begin, source->entities().end()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_const_view<E, Ts...>::iterator basic_component_const_view<E,
                                                                                                  Ts...>::end() noexcept
        {
            return {&*source, source->entities().cend(), source->entities().cend()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_const_view<E, Ts...>::const_iterator basic_component_const_view<E, Ts...>::end()
            const noexcept
        {
            return {&*source, source->entities().cend(), source->entities().cend()};
        }

        template <typename E, typename... Ts>
        inline typename basic_component_const_view<E, Ts...>::const_iterator basic_component_const_view<
            E, Ts...>::cend() const noexcept
        {
            return {&*source, source->entities().cend(), source->entities().cend()};
        }
    } // namespace detail

    using registry = basic_registry<entity>;
} // namespace tempest::ecs

#endif // tempest_ecs_registry_hpp
