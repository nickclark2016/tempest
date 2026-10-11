#ifndef tempest_ecs_component_type_registry_hpp
#define tempest_ecs_component_type_registry_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/assert.hpp>
#include <tempest/bit.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/int.hpp>
#include <tempest/meta.hpp>
#include <tempest/optional.hpp>
#include <tempest/reflection.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/traits.hpp>
#include <tempest/type_manifest.hpp>

namespace tempest::ecs
{
    template <size_t N>
    struct basic_archetype_types_hash
    {
        static constexpr size_t count = bit_ceil(N) / 8;
        array<byte, count> hash{};
    };

    template <size_t N>
    constexpr auto operator==(const basic_archetype_types_hash<N>& lhs,
                              const basic_archetype_types_hash<N>& rhs) noexcept -> bool
    {
        return lhs.hash == rhs.hash;
    }

    template <size_t N>
    constexpr auto operator!=(const basic_archetype_types_hash<N>& lhs,
                              const basic_archetype_types_hash<N>& rhs) noexcept -> bool
    {
        return !(lhs == rhs);
    }

    struct TEMPEST_API component_type_info
    {
        static constexpr uint16_t default_size = 0;
        static constexpr uint16_t default_alignment = 0;
        static constexpr uint32_t default_index = 0;
        static constexpr bool default_should_duplicate = true;
        static constexpr bool default_is_serializable = true;

        string_view name;
        uint16_t size = default_size;
        uint16_t alignment = default_alignment;
        uint32_t index = default_index;
        bool should_duplicate = default_should_duplicate;
        optional<type_descriptor> descriptor;
        bool is_serializable = default_is_serializable;
    };

    class TEMPEST_API component_type_registry
    {
      public:
        static constexpr size_t max_component_types = 256;

        component_type_registry() = default;
        component_type_registry(const component_type_registry&) = delete;
        component_type_registry(component_type_registry&&) noexcept = default;
        ~component_type_registry() = default;

        auto operator=(const component_type_registry&) -> component_type_registry& = delete;
        auto operator=(component_type_registry&&) noexcept -> component_type_registry& = default;

        template <typename T>
        auto register_type() -> uint32_t
        {
            using component_type = remove_cvref_t<T>;
            const auto name = []() {
                if constexpr (core::is_normalized_type_name_valid_v<component_type>)
                {
                    return core::normalized_type_name<component_type>();
                }
                else
                {
                    return core::get_type_name<component_type>();
                }
            }();

            const auto resolved = resolve_alias(name);
            const auto existing = type_index(resolved);
            if (existing.has_value())
            {
                return *existing;
            }

            TEMPEST_ASSERT(_count < max_component_types);

            const auto idx = static_cast<uint32_t>(_count);
            auto should_dup = true;
            if constexpr (requires { is_duplicatable_v<component_type>; })
            {
                should_dup = is_duplicatable_v<component_type>;
            }

            auto serializable = true;
            if constexpr (requires { is_serializable_v<component_type>; })
            {
                serializable = is_serializable_v<component_type>;
            }

            auto desc = optional<type_descriptor>{nullopt};
            if constexpr (reflected_component<component_type>)
            {
                auto builder = type_builder<component_type>{resolved};
                reflect<component_type>::describe(builder);
                desc = builder.build();
            }

            auto info = component_type_info{
                .name = resolved,
                .size = static_cast<uint16_t>(sizeof(component_type)),
                .alignment = static_cast<uint16_t>(alignof(component_type)),
                .index = idx,
                .should_duplicate = should_dup,
                .descriptor = tempest::move(desc),
                .is_serializable = serializable,
            };

            _register_type_info(tempest::move(info));
            return idx;
        }

        template <reflected_component T>
        auto register_reflected() -> uint32_t
        {
            return register_type<T>();
        }

        template <typename T>
        auto ensure() -> uint32_t
        {
            return register_type<T>();
        }

        template <typename T>
        [[nodiscard]] auto type_index() const -> optional<uint32_t>
        {
            using component_type = remove_cvref_t<T>;
            const auto name = []() {
                if constexpr (core::is_normalized_type_name_valid_v<component_type>)
                {
                    return core::normalized_type_name<component_type>();
                }
                else
                {
                    return core::get_type_name<component_type>();
                }
            }();
            return type_index(name);
        }

        [[nodiscard]] auto type_index(string_view name) const -> optional<uint32_t>;
        [[nodiscard]] auto type_info(uint32_t index) const -> const component_type_info&;
        [[nodiscard]] auto type_info(string_view name) const -> const component_type_info*;
        [[nodiscard]] auto descriptor(uint32_t index) const -> const type_descriptor*;
        [[nodiscard]] auto descriptor(string_view name) const -> const type_descriptor*;

        void add_alias(string_view legacy_name, string_view current_name);
        [[nodiscard]] auto resolve_alias(string_view name) const -> string_view;

        [[nodiscard]] auto size() const noexcept -> size_t;
        [[nodiscard]] auto empty() const noexcept -> bool;

        [[nodiscard]] auto schema_hash() const noexcept -> uint64_t;
        [[nodiscard]] auto export_manifest() const -> type_manifest;

        template <typename... Ts>
        [[nodiscard]] auto hash_mask() -> basic_archetype_types_hash<max_component_types>
        {
            auto result = basic_archetype_types_hash<max_component_types>{};
            auto set_type_bit = [this, &result](uint32_t idx) {
                result.hash[idx / char_bit] |= static_cast<byte>(1 << (idx % char_bit));
            };
            (set_type_bit(ensure<Ts>()), ...);
            return result;
        }

        template <typename... Ts>
        [[nodiscard]] auto hash_mask() const -> basic_archetype_types_hash<max_component_types>
        {
            auto result = basic_archetype_types_hash<max_component_types>{};
            auto set_type_bit = [this, &result](optional<uint32_t> opt_idx) {
                if (opt_idx.has_value())
                {
                    const auto idx = *opt_idx;
                    result.hash[idx / char_bit] |= static_cast<byte>(1 << (idx % char_bit));
                }
                else
                {
                    result.hash[(max_component_types - 1) / char_bit] |= static_cast<byte>(1 << ((max_component_types - 1) % char_bit));
                }
            };
            (set_type_bit(type_index<Ts>()), ...);
            return result;
        }

      private:
        void _register_type_info(component_type_info info);

        array<component_type_info, max_component_types> _types{};
        flat_unordered_map<string, uint32_t> _name_to_index;
        flat_unordered_map<string, string> _aliases;
        size_t _count{0};
    };
} // namespace tempest::ecs

#endif // tempest_ecs_component_type_registry_hpp
