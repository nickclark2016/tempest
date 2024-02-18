#ifndef tempest_ecs_registry_hpp
#define tempest_ecs_registry_hpp

#include "sparse.hpp"
#include "traits.hpp"

#include <tempest/algorithm.hpp>
#include <tempest/meta.hpp>

#include <array>
#include <cassert>
#include <climits>
#include <compare>
#include <concepts>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tempest::ecs
{
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
            [[nodiscard]] constexpr reference operator*() noexcept;

            /**
             * @brief Dereference operator.
             */
            [[nodiscard]] constexpr const_reference operator*() const noexcept;

            /**
             * @brief Dereference operator.
             */
            [[nodiscard]] constexpr pointer operator->() const noexcept;

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
        inline constexpr basic_entity_store_iterator<T, EPC, EPB, BPC>::reference basic_entity_store_iterator<
            T, EPC, EPB, BPC>::operator*() noexcept
        {
            auto chunk_index = index / entities_per_chunk;
            auto chunk_offset = index % entities_per_chunk;

            auto block_index = chunk_offset / entities_per_block;
            auto block_offset = chunk_offset % entities_per_block;

            return chunks[chunk_index].blocks[block_index].entities[block_offset];
        }

        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        inline constexpr basic_entity_store_iterator<T, EPC, EPB, BPC>::const_reference basic_entity_store_iterator<
            T, EPC, EPB, BPC>::operator*() const noexcept
        {
            auto chunk_index = index / entities_per_chunk;
            auto chunk_offset = index % entities_per_chunk;

            auto block_index = chunk_offset / entities_per_block;
            auto block_offset = chunk_offset % entities_per_block;

            return chunks[chunk_index].blocks[block_index].entities[block_offset];
        }

        template <typename T, std::size_t EPC, std::size_t EPB, std::size_t BPC>
        inline constexpr basic_entity_store_iterator<T, EPC, EPB, BPC>::pointer basic_entity_store_iterator<
            T, EPC, EPB, BPC>::operator->() const noexcept
        {
            auto chunk_index = index / entities_per_chunk;
            auto chunk_offset = index % entities_per_chunk;

            auto block_index = chunk_offset / entities_per_block;
            auto block_offset = chunk_offset % entities_per_block;

            return chunks[chunk_index].blocks[block_index].entities.data() + block_offset;
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

                if (core::is_bit_set(chunks[chunk_index].blocks[block_index].occupancy, block_offset))
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

                if (core::is_bit_set(chunks[chunk_index].blocks[block_index].occupancy, block_offset))
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
            std::array<T, entities_per_block> entities;
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
        std::vector<chunk> _chunks;
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
                    if (core::is_bit_set(_chunks[chunk_idx].blocks[block_idx].occupancy, block_offset))
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
                    if (core::is_bit_set(_chunks[chunk_idx].blocks[block_idx].occupancy, block_offset))
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

        blk.occupancy = core::set_bit(blk.occupancy, block_offset);

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

        blk.occupancy = core::clear_bit(blk.occupancy, block_offset);

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
            return core::is_bit_set(blk.occupancy, block_offset) &&
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

        _chunks.reserve(new_chunks);

        for (std::size_t i = current_chunk_count; i < new_chunks; ++i)
        {
            _chunks.emplace_back();
        }

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

    using entity_store = basic_entity_store<entity, 4096, std::uint64_t>;

    template <typename E>
    class basic_registry
    {
      public:
        using entity_type = E;
        using traits_type = entity_traits<E>;
        using size_type = std::size_t;

        E acquire_entity();
        void release_entity(E e);

        bool is_valid(E e) const noexcept;
        std::size_t entity_count() const noexcept;

        template <typename T>
        void assign(E e, const T& value);

        template <typename T>
        [[nodiscard]] bool has(E e) const noexcept;

        template <typename T>
        [[nodiscard]] T& get(E e);

        template <typename T>
        [[nodiscard]] const T& get(E e) const;

        template <typename T>
        [[nodiscard]] T* try_get(E e) noexcept;

        template <typename T>
        [[nodiscard]] const T* try_get(E e) const noexcept;

        template <typename T>
        void remove(E e);

      private:
        basic_entity_store<E, 4096, std::uint64_t> _entities;
        std::unordered_map<std::size_t, std::unique_ptr<basic_sparse_map_interface<E>>> _component_stores;
    };

    template <typename E>
    inline E basic_registry<E>::acquire_entity()
    {
        return _entities.acquire();
    }

    template <typename E>
    inline void basic_registry<E>::release_entity(E e)
    {
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
    template <typename T>
    inline void basic_registry<E>::assign(E e, const T& value)
    {
        static core::type_info id = core::type_id<T>();
        auto& store = _component_stores[id.index()];
        if (!store)
        {
            store = std::make_unique<sparse_map<T>>();
        }

        static_cast<sparse_map<T>*>(store.get())->insert(e, value);
    }

    template <typename E>
    template <typename T>
    inline bool basic_registry<E>::has(E e) const noexcept
    {
        static core::type_info id = core::type_id<T>();
        auto store = _component_stores.find(id.index());
        if (store == _component_stores.cend())
        {
            return false;
        }

        return static_cast<sparse_map<T>*>(store->second.get())->contains(e);
    }

    template <typename E>
    template <typename T>
    inline T& basic_registry<E>::get(E e)
    {
        static core::type_info id = core::type_id<T>();
        auto store_it = _component_stores.find(id.index());
        assert(store_it != _component_stores.cend());

        auto& [_, store] = *store_it;

        assert(static_cast<sparse_map<T>*>(store.get())->contains(e));

        return static_cast<sparse_map<T>&>(*store.get())[e];
    }

    template <typename E>
    template <typename T>
    inline const T& basic_registry<E>::get(E e) const
    {
        static core::type_info id = core::type_id<T>();
        auto store_it = _component_stores.find(id.index());
        assert(store_it != _component_stores.cend());

        const auto& [_, store] = *store_it;

        assert(static_cast<const sparse_map<T>*>(store.get())->contains(e));

        return static_cast<const sparse_map<T>&>(*store.get())[e];
    }

    template <typename E>
    template <typename T>
    inline T* basic_registry<E>::try_get(E e) noexcept
    {
        static core::type_info id = core::type_id<T>();
        auto store_it = _component_stores.find(id.index());
        if (store_it == _component_stores.cend())
        {
            return nullptr;
        }

        auto& [_, store] = *store_it;

        if (!static_cast<sparse_map<T>*>(store.get())->contains(e))
        {
            return nullptr;
        }

        return &static_cast<sparse_map<T>&>(*store.get())[e];
    }

    template <typename E>
    template <typename T>
    inline const T* basic_registry<E>::try_get(E e) const noexcept
    {
        static core::type_info id = core::type_id<T>();
        auto store_it = _component_stores.find(id.index());
        if (store_it == _component_stores.cend())
        {
            return nullptr;
        }

        const auto& [_, store] = *store_it;

        if (!static_cast<const sparse_map<T>*>(store.get())->contains(e))
        {
            return nullptr;
        }

        return &static_cast<const sparse_map<T>&>(*store.get())[e];
    }

    template <typename E>
    template <typename T>
    inline void basic_registry<E>::remove(E e)
    {
        static core::type_info id = core::type_id<T>();
        auto store_it = _component_stores.find(id.index());
        assert(store_it != _component_stores.cend());

        auto& [_, store] = *store_it;

        assert(static_cast<sparse_map<T>*>(store.get())->contains(e));

        static_cast<sparse_map<T>*>(store.get())->erase(e);
    }

    using registry = basic_registry<entity>;
} // namespace tempest::ecs

#endif // tempest_ecs_registry_hpp
