#ifndef tempest_ecs_archetype_hpp
#define tempest_ecs_archetype_hpp

#include <tempest/array.hpp>
#include <tempest/bit.hpp>
#include <tempest/functional.hpp>
#include <tempest/int.hpp>
#include <tempest/meta.hpp>
#include <tempest/slot_map.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::ecs
{
    struct basic_archetype_type_info
    {
        uint16_t size;
        uint16_t alignment;
        uint32_t index;
    };

    namespace detail
    {
        size_t get_archetype_type_index(string_view name);

        template <typename T>
        size_t get_archetype_type_index()
        {
            constexpr auto name = core::get_type_name<T>();
            static const size_t index = get_archetype_type_index(name);
            return index;
        }
    } // namespace detail

    template <typename T>
        requires is_trivial_v<T>
    inline basic_archetype_type_info create_archetype_type_info()
    {
        size_t alignment = alignof(T);
        size_t size = sizeof(T);
        size_t index = detail::get_archetype_type_index<T>();

        basic_archetype_type_info ti = {
            .size = static_cast<uint16_t>(size),
            .alignment = static_cast<uint16_t>(alignment),
            .index = static_cast<uint32_t>(index),
        };

        return ti;
    }

    class basic_archetype_storage
    {
      public:
        basic_archetype_storage(basic_archetype_type_info info, size_t initial_capacity = 0);
        basic_archetype_storage(const basic_archetype_storage&) = delete;
        basic_archetype_storage(basic_archetype_storage&& rhs) noexcept;
        ~basic_archetype_storage();

        basic_archetype_storage& operator=(const basic_archetype_storage&) = delete;
        basic_archetype_storage& operator=(basic_archetype_storage&& rhs) noexcept;

        void reserve(size_t count);
        byte* element_at(size_t index);
        const byte* element_at(size_t index) const;

        size_t capacity() const noexcept;

        void copy(size_t dst, size_t src);

        basic_archetype_type_info type_info() const noexcept
        {
            return _storage;
        }

      private:
        basic_archetype_type_info _storage;
        byte* _data;
        size_t _size;
    };

    inline size_t basic_archetype_storage::capacity() const noexcept
    {
        return _size;
    }

    struct basic_archetype_key
    {
        uint32_t index;
        uint32_t generation;
    };

    inline constexpr bool operator==(basic_archetype_key lhs, basic_archetype_key rhs) noexcept
    {
        return lhs.index == rhs.index && lhs.generation == rhs.generation;
    }

    inline constexpr bool operator!=(basic_archetype_key lhs, basic_archetype_key rhs) noexcept
    {
        return !(lhs == rhs);
    }

    class basic_archetype
    {
      public:
        using key_type = basic_archetype_key;

        basic_archetype(span<const basic_archetype_type_info> field_info);

        key_type allocate();
        void reserve(size_t count);
        bool erase(key_type key);

        byte* element_at(size_t el_index, size_t type_info_index);
        const byte* element_at(size_t el_index, size_t type_info_index) const;
        byte* element_at(key_type key, size_t type_info_index);
        const byte* element_at(key_type key, size_t type_info_index) const;

        size_t size() const noexcept;
        size_t capacity() const noexcept;
        bool empty() const noexcept;

        span<const basic_archetype_storage> storages() const noexcept;

      private:
        vector<basic_archetype_key> _trampoline;
        vector<uint32_t> _look_back_table; // points from the index of the value to the trampoline table

        vector<basic_archetype_storage> _storage;
        size_t _element_count;
        size_t _element_capacity;
        size_t _first_free_element;
    };

    inline size_t basic_archetype::size() const noexcept
    {
        return _element_count;
    }

    inline size_t basic_archetype::capacity() const noexcept
    {
        return _element_capacity;
    }

    inline bool basic_archetype::empty() const noexcept
    {
        return _element_count == 0;
    }

    inline span<const basic_archetype_storage> basic_archetype::storages() const noexcept
    {
        return _storage;
    }

    template <size_t N>
    struct basic_archetype_types_hash
    {
        static constexpr size_t Count = bit_ceil(N) / 8;
        array<byte, Count> hash;
    };

    template <size_t N>
    constexpr bool operator==(const basic_archetype_types_hash<N>& lhs,
                              const basic_archetype_types_hash<N>& rhs) noexcept
    {
        return lhs.hash == rhs.hash;
    }

    template <size_t N>
    constexpr bool operator!=(const basic_archetype_types_hash<N>& lhs,
                              const basic_archetype_types_hash<N>& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    struct basic_archetype_entity
    {
        basic_archetype::key_type archetype_key;
        size_t archetype_index;
    };

    namespace detail
    {
        template <size_t N, typename... Ts>
        basic_archetype_types_hash<N> create_archetype_types_hash()
        {
            basic_archetype_types_hash<N> hash = {};
            ((hash.hash[get_archetype_type_index<Ts>() / 8] |=
              static_cast<byte>(1 << (get_archetype_type_index<Ts>() % 8))),
             ...);
            return hash;
        }
    } // namespace detail

    class basic_archetype_registry
    {
      public:
        using entity_type = uint64_t;

        template <typename... Ts>
        entity_type create();

        void destroy(entity_type entity);

        template <typename T>
        remove_cvref_t<T>& assign(entity_type entity, T&& component);

        template <typename T>
        remove_cvref_t<T>& replace(entity_type entity, T&& component);

        template <typename T>
        remove_cvref_t<T>& assign_or_replace(entity_type entity, T&& component);

        template <typename T>
        void remove(entity_type entity);

        template <typename T>
        remove_cvref_t<T>& get(entity_type entity);

        template <typename T>
        const remove_cvref_t<T>& get(entity_type entity) const;

        template <typename... Ts>
        bool has(entity_type entity) const;

        size_t size() const noexcept;

        template <typename Fn>
        void each(Fn&& func);

      private:
        vector<basic_archetype> _archetypes;
        vector<basic_archetype_types_hash<256u>> _hashes;
        slot_map<basic_archetype_entity> _entity_keys;

        size_t _index_of_component_in_archetype(size_t arch_index, size_t component_id) const;
    };

    template <typename... Ts>
    inline basic_archetype_registry::entity_type basic_archetype_registry::create()
    {
        static const auto hash = detail::create_archetype_types_hash<256u, remove_cvref_t<Ts>...>();

        // Check if the archetype already exists
        auto it = tempest::find(_hashes.begin(), _hashes.end(), hash);
        if (it == _hashes.end())
        {
            // Create a new archetype
            array type_infos{create_archetype_type_info<remove_cvref_t<Ts>>()...};
            std::sort(type_infos.begin(), type_infos.end(),
                      [](const auto& lhs, const auto& rhs) { return lhs.index < rhs.index; });
            _archetypes.emplace_back(type_infos);
            _hashes.push_back(hash);
            it = _hashes.end() - 1;
        }

        const size_t archetype_index = tempest::distance(_hashes.begin(), it);
        basic_archetype& arch = _archetypes[archetype_index];

        // Allocate an entity in an archetype
        const auto arch_key = arch.allocate();

        auto ent = basic_archetype_entity{
            .archetype_key = arch_key,
            .archetype_index = archetype_index,
        };

        return _entity_keys.insert(ent);
    }

    template <typename T>
    inline remove_cvref_t<T>& basic_archetype_registry::assign(typename basic_archetype_registry::entity_type entity,
                                                               T&& component)
    {
        auto key = _entity_keys.at(entity);
        auto archetype_index = key.archetype_index;

        // Get the archetype
        auto existing_arch = &_archetypes[key.archetype_index];

        // Get the archetype of the entity + component
        using component_type = remove_cvref_t<T>;
        static const auto type_index = detail::get_archetype_type_index<component_type>();

        // Create a hash for the combined archetype
        auto hash = _hashes[archetype_index];
        hash.hash[type_index / 8] |= static_cast<byte>(1 << (type_index % 8));

        // Check if the archetype already exists
        auto it = tempest::find(_hashes.begin(), _hashes.end(), hash);

        if (it == _hashes.end())
        {
            // Create a new archetype
            // Get the existing archetype's info
            auto existing_storage_view = existing_arch->storages();

            vector<basic_archetype_type_info> new_types;

            for (const auto& storage : existing_storage_view)
            {
                new_types.push_back(storage.type_info());
            }

            new_types.push_back(create_archetype_type_info<component_type>());
            std::sort(new_types.begin(), new_types.end(),
                      [](const auto& lhs, const auto& rhs) { return lhs.index < rhs.index; });


            auto capacity = existing_arch->capacity();
            auto& new_archetype = _archetypes.emplace_back(new_types);
            new_archetype.reserve(capacity);

            _hashes.push_back(hash);

            it = _hashes.end() - 1;
        }

        auto new_archetype_index = tempest::distance(_hashes.begin(), it);
        basic_archetype& new_arch = _archetypes[new_archetype_index];

        auto arch_key = new_arch.allocate();

        // Copy the entity's data to the new archetype, skipping the newly added component
        auto& existing_hash = _hashes[archetype_index];
        const auto& new_hash = _hashes[new_archetype_index];

        existing_arch = &_archetypes[key.archetype_index];

        component_type* result_ptr = nullptr;

        size_t components_written = 0;

        for (size_t i = 0; i < 256u; ++i)
        {
            // Test the bit at i
            const bool existing_bit =
                (existing_hash.hash[i / 8] & static_cast<byte>(1 << (i % 8))) != static_cast<byte>(0);
            const bool new_bit = (new_hash.hash[i / 8] & static_cast<byte>(1 << (i % 8))) != static_cast<byte>(0);

            auto existing_component_index = _index_of_component_in_archetype(archetype_index, i);
            auto new_component_index = _index_of_component_in_archetype(new_archetype_index, i);

            if (existing_bit && new_bit)
            {
                // Copy the component
                auto existing_data = existing_arch->element_at(key.archetype_key, existing_component_index);
                auto new_data = new_arch.element_at(arch_key, new_component_index);
                copy_n(existing_data, existing_arch->storages()[existing_component_index].type_info().size, new_data);

                ++components_written;
            }
            else if (new_bit)
            {
                // Initialize the component
                auto new_data = new_arch.element_at(arch_key, new_component_index);
                result_ptr = construct_at(reinterpret_cast<remove_cvref_t<T>*>(new_data), component);

                ++components_written;
            }

            // Early exit if we've copied all the components
            if (components_written == new_arch.storages().size())
            {
                break;
            }
        }

        // Erase the old entity
        existing_arch->erase(key.archetype_key);

        // Update the entity key
        key.archetype_key = arch_key;
        key.archetype_index = new_archetype_index;
        _entity_keys.at(entity) = key;

        return *result_ptr;
    }

    template <typename T>
    inline remove_cvref_t<T>& basic_archetype_registry::replace(typename basic_archetype_registry::entity_type entity,
                                                                T&& value)
    {
        using component_type = remove_cvref_t<T>;
        static const basic_archetype_type_info type_info = create_archetype_type_info<component_type>();

        auto key = _entity_keys.at(entity);
        auto archetype_index = key.archetype_index;
        const auto type_index = _index_of_component_in_archetype(archetype_index, type_info.index);

        auto& arch = _archetypes[archetype_index];
        auto data = arch.element_at(key.archetype_key, type_index);

        auto* res = construct_at(reinterpret_cast<component_type*>(data), value);

        return *res;
    }

    template <typename T>
    inline remove_cvref_t<T>& basic_archetype_registry::assign_or_replace(
        typename basic_archetype_registry::entity_type entity, T&& value)
    {
        if (has<T>(entity))
        {
            return replace(entity, value);
        }
        else
        {
            return assign(entity, value);
        }
    }

    template <typename T>
    inline void basic_archetype_registry::remove(typename basic_archetype_registry::entity_type entity)
    {
        using component_type = remove_cvref_t<T>;
        static const auto type_info_index = detail::get_archetype_type_index<component_type>();
        static const basic_archetype_type_info type_info = create_archetype_type_info<component_type>();

        auto key = _entity_keys.at(entity);
        auto archetype_index = key.archetype_index;

        auto type_index = _index_of_component_in_archetype(archetype_index, type_info.index);

        auto* arch = &_archetypes[archetype_index];
        auto data = arch->element_at(key.archetype_key, type_index);
        (void)destroy_at(reinterpret_cast<component_type*>(data));

        // Create a hash without the component
        auto hash = _hashes[archetype_index];
        hash.hash[detail::get_archetype_type_index<component_type>() / 8] &=
            static_cast<byte>(~(1 << (detail::get_archetype_type_index<component_type>() % 8)));

        // Find the index of the component in the archetype
        size_t index_to_remove = 0;
        for (size_t i = 0; i < type_info.index / 8; ++i)
        {
            auto hash_byte = hash.hash[i];
            index_to_remove += popcount(static_cast<uint8_t>(hash_byte));
        }

        for (size_t i = 0; i < type_info.index % 8; ++i)
        {
            if (static_cast<unsigned int>(hash.hash[type_info_index / 8]) & (1 << i))
            {
                ++index_to_remove;
            }
        }

        // Check if the archetype already exists
        auto it = tempest::find(_hashes.begin(), _hashes.end(), hash);

        if (it == _hashes.end())
        {

            // Create a new archetype
            auto existing_storage_view = arch->storages();
            vector<basic_archetype_type_info> new_types;

            for (auto& storage : existing_storage_view)
            {
                if (storage.type_info().index != type_info_index)
                {
                    new_types.push_back(storage.type_info());
                }
            }

            std::sort(new_types.begin(), new_types.end(),
                      [](const auto& lhs, const auto& rhs) { return lhs.index < rhs.index; });

            auto capacity = arch->capacity();
            auto& new_arch = _archetypes.emplace_back(new_types);
            new_arch.reserve(capacity);
            _hashes.push_back(hash);

            it = _hashes.end() - 1;
        }

        auto new_archetype_index = tempest::distance(_hashes.begin(), it);
        auto& new_arch = _archetypes[new_archetype_index];
        auto new_key = new_arch.allocate();

        arch = &_archetypes[key.archetype_index];

        // Copy the entity's data to the new archetype, skipping the removed component
        const auto& existing_hash = _hashes[archetype_index];
        const auto& new_hash = _hashes[new_archetype_index];

        for (size_t i = 0; i < 256u; ++i)
        {
            // Test the bit at i
            const bool existing_bit =
                (existing_hash.hash[i / 8] & static_cast<byte>(1 << (i % 8))) != static_cast<byte>(0);
            const bool new_bit = (new_hash.hash[i / 8] & static_cast<byte>(1 << (i % 8))) != static_cast<byte>(0);

            auto existing_component_index = _index_of_component_in_archetype(archetype_index, i);
            auto new_component_index = _index_of_component_in_archetype(new_archetype_index, i);

            if (existing_bit && new_bit)
            {
                // Copy the component
                auto existing_data = arch->element_at(key.archetype_key, existing_component_index);
                auto new_data = new_arch.element_at(new_key, new_component_index);
                copy_n(existing_data, arch->storages()[existing_component_index].type_info().size, new_data);
            }

            // Early exit if we've copied all the components
            if (new_component_index == new_arch.storages().size())
            {
                break;
            }
        }

        // Erase the old entity
        arch->erase(key.archetype_key);

        // Update the entity key
        auto entity_key = basic_archetype_entity{
            .archetype_key = new_key,
            .archetype_index = static_cast<size_t>(new_archetype_index),
        };

        _entity_keys.at(entity) = entity_key;
    }

    template <typename T>
    inline remove_cvref_t<T>& basic_archetype_registry::get(typename basic_archetype_registry::entity_type entity)
    {
        using component_type = remove_cvref_t<T>;
        static const auto type_info = create_archetype_type_info<component_type>();

        auto key = _entity_keys.at(entity);
        auto archetype_index = key.archetype_index;
        const auto type_index = _index_of_component_in_archetype(archetype_index, type_info.index);
        auto& arch = _archetypes[archetype_index];
        auto data = arch.element_at(key.archetype_key, type_index);
        return *reinterpret_cast<T*>(data);
    }

    template <typename T>
    inline const remove_cvref_t<T>& basic_archetype_registry::get(
        typename basic_archetype_registry::entity_type entity) const
    {
        using component_type = remove_cvref_t<T>;
        static const auto type_info = create_archetype_type_info<component_type>();

        auto key = _entity_keys.at(entity);
        auto archetype_index = key.archetype_index;
        const auto type_index = _index_of_component_in_archetype(archetype_index, type_info.index);
        auto& arch = _archetypes[archetype_index];
        auto data = arch.element_at(key.archetype_key, type_index);
        return *reinterpret_cast<const T*>(data);
    }

    template <typename... Ts>
    inline bool basic_archetype_registry::has(typename basic_archetype_registry::entity_type entity) const
    {
        static const auto hash = detail::create_archetype_types_hash<256u, remove_cvref_t<Ts>...>();
        const auto& key = _entity_keys.at(entity);
        auto archetype_index = key.archetype_index;
        const auto& arch_hash = _hashes[archetype_index];

        // Test to ensure each bit set in hash is set in arch_hash
        // TODO: Vectorize this
        for (size_t i = 0; i < 256u / 8u; ++i)
        {
            if ((hash.hash[i] & arch_hash.hash[i]) != hash.hash[i])
            {
                return false;
            }
        }

        return true;
    }

    inline size_t basic_archetype_registry::size() const noexcept
    {
        return _entity_keys.size();
    }

    inline size_t basic_archetype_registry::_index_of_component_in_archetype(size_t arch_index,
                                                                             size_t component_id) const
    {
        size_t result = 0;

        for (size_t i = 0; i < _archetypes[arch_index].storages().size(); ++i)
        {
            if (_archetypes[arch_index].storages()[i].type_info().index == component_id)
            {
                return result;
            }
            ++result;
        }
        return result;
    }

    namespace detail
    {
        template <typename... Ts>
        struct hash_mask_type_list_traits;

        template <typename... Ts>
        struct hash_mask_type_list_traits<core::type_list<Ts...>>
        {
            static basic_archetype_types_hash<256u> create()
            {
                return create_archetype_types_hash<256u, remove_cvref_t<Ts>...>();
            }
        };

        template <size_t N, typename... Ts>
        struct for_each_fn_applier;

        template <size_t N, typename... Ts>
        struct for_each_fn_applier<N, core::type_list<Ts...>>
        {
            template <typename Fn, size_t... Is>
            static void apply(Fn&& fn, array<byte*, N>&& args, index_sequence<Is...>)
            {
                tempest::forward<Fn>(fn)(*reinterpret_cast<remove_cvref_t<Ts>*>(args[Is])...);
            }

            template <typename Fn>
            static void apply(Fn&& fn, array<byte*, N>&& args)
            {
                using fn_traits = function_traits<remove_cvref_t<Fn>>;
                constexpr auto argument_count = core::type_list_size_v<typename fn_traits::argument_types>;

                using arg_indices = make_index_sequence<argument_count>;

                apply(tempest::forward<Fn>(fn), tempest::move(args), arg_indices{});
            }
        };
    } // namespace detail
    // namespace detail

    template <typename Fn>
    inline void basic_archetype_registry::each(Fn&& func)
    {
        using fn_traits = function_traits<remove_cvref_t<Fn>>;
        static const auto hash_mask = detail::hash_mask_type_list_traits<typename fn_traits::argument_types>::create();
        static constexpr auto argument_count = core::type_list_size_v<typename fn_traits::argument_types>;

        for (size_t i = 0; i < _archetypes.size(); ++i)
        {
            // Test against the hash mask
            const auto& hash = _hashes[i];
            bool matches = true;
            for (size_t j = 0; j < 256u / 8u; ++j)
            {
                if ((hash_mask.hash[j] & hash.hash[j]) != hash_mask.hash[j])
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
            {
                // Compute the index of the requested components in the archetype
                // This is not the same as the index of the component in the hash
                array<size_t, argument_count> argument_indices;

                size_t argument_indices_idx = 0;
                size_t argument_index = 0;

                for (size_t j = 0; j < 256u; ++j)
                {
                    // If the mask and hash bits are both set, this is a component we're interested in
                    // If the hash bit is set, increment the argument index

                    const bool mask_bit =
                        (hash_mask.hash[j / 8] & static_cast<byte>(1 << (j % 8))) != static_cast<byte>(0);
                    const bool hash_bit = (hash.hash[j / 8] & static_cast<byte>(1 << (j % 8))) != static_cast<byte>(0);

                    if (mask_bit && hash_bit)
                    {
                        argument_indices[argument_indices_idx++] = _index_of_component_in_archetype(i, j);
                    }

                    if (mask_bit)
                    {
                        ++argument_index;
                    }
                }

                auto& arch = _archetypes[i];

                for (size_t j = 0; j < arch.size(); ++j)
                {
                    array<byte*, argument_count> arguments;

                    for (size_t k = 0; k < argument_count; ++k)
                    {
                        size_t storage_index = argument_indices[k];
                        byte* storage_data = arch.element_at(j, storage_index);

                        arguments[k] = storage_data;
                    }

                    detail::for_each_fn_applier<argument_count, typename fn_traits::argument_types>::apply(
                        tempest::forward<Fn>(func), tempest::move(arguments));
                }
            }
        }
    }
} // namespace tempest::ecs

#endif // tempest_ecs_archetype_hpp