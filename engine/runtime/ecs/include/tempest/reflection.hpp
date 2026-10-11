#ifndef tempest_ecs_reflection_hpp
#define tempest_ecs_reflection_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/invoke.hpp>
#include <tempest/optional.hpp>
#include <tempest/quat.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>
#include <tempest/traits.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>
#include <tempest/variant.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>
#include <tempest/vector.hpp>

namespace tempest::ecs
{
    /// @brief Enumerates the primitive and composite value types recognized by the reflection engine.
    enum class field_kind : uint8_t
    {
        boolean,
        int32,
        uint32,
        int64,
        uint64,
        float32,
        float64,
        vec2,
        vec3,
        vec4,
        quat,
        color3,
        color4,
        guid,
        entity_ref,
        asset_ref,
        enumeration,
        fixed_string,
    };

    /// @brief Semantic attribute flags modifying field persistence, inspector rendering, and mutability.
    enum class field_flags : uint8_t
    {
        none = 0,
        transient = 1 << 0,
        read_only = 1 << 1,
        hidden = 1 << 2,
    };

    constexpr auto operator|(field_flags lhs, field_flags rhs) noexcept -> field_flags
    {
        return static_cast<field_flags>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
    }

    constexpr auto operator&(field_flags lhs, field_flags rhs) noexcept -> field_flags
    {
        return static_cast<field_flags>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
    }

    constexpr auto operator~(field_flags flags) noexcept -> field_flags
    {
        return static_cast<field_flags>(~static_cast<uint8_t>(flags));
    }

    constexpr auto operator|=(field_flags& lhs, field_flags rhs) noexcept -> field_flags&
    {
        lhs = lhs | rhs;
        return lhs;
    }

    constexpr auto operator&=(field_flags& lhs, field_flags rhs) noexcept -> field_flags&
    {
        lhs = lhs & rhs;
        return lhs;
    }

    /// @brief Individual entry in an enumerated type descriptor with display name and integer value.
    struct TEMPEST_API enum_entry
    {
        static constexpr int64_t default_value = 0;

        string_view name;
        int64_t value = default_value;
    };

    /// @brief Type-safe variant supporting numeric, vector, and composite boundary limits for UI controls.
    using ui_bound_value = variant<int64_t, uint64_t, double, math::vec2<float>, math::vec3<float>, math::vec4<float>>;

    /// @brief Rendering presentation style for the UI inspector.
    enum class ui_widget_type : uint8_t
    {
        default_widget = 0,
        drag,
        slider,
        color_picker,
        dropdown,
        flags,
        text_box,
        checkbox,
    };

    /// @brief Editor and inspector presentation hints associated with a reflected field.
    struct TEMPEST_API field_ui_hints
    {
        static constexpr float default_speed = 0.1F;

        optional<ui_bound_value> min_value;
        optional<ui_bound_value> max_value;
        float drag_speed = default_speed;
        ui_widget_type widget = ui_widget_type::default_widget;
        string_view units;
        string_view tooltip;
        string_view display_name;
    };

    /// @brief Complete metadata and runtime accessor bindings for a single component field or property.
    struct TEMPEST_API field_descriptor
    {
        static constexpr size_t no_offset = ~size_t{0};
        static constexpr size_t default_size = 0;
        static constexpr uint64_t default_asset_type_hash = 0;

        string_view name;
        field_kind kind = field_kind::boolean;
        field_flags flags = field_flags::none;
        size_t offset = no_offset;
        size_t size = default_size;
        uint64_t asset_type_hash = default_asset_type_hash;
        span<const enum_entry> enum_entries;
        field_ui_hints hints;
        void (*get)(const void* component_ptr, void* out_value) = nullptr;
        void (*set)(void* component_ptr, const void* in_value) = nullptr;
    };

    /// @brief Complete type descriptor containing metadata, field table, and lifecycle hooks.
    struct TEMPEST_API type_descriptor
    {
        static constexpr size_t default_size = 0;
        static constexpr size_t default_alignment = 0;
        static constexpr uint32_t default_version = 1;

        string_view name;
        size_t size = default_size;
        size_t alignment = default_alignment;
        uint32_t version = default_version;
        vector<field_descriptor> fields;
        void (*post_load)(void* component_ptr) = nullptr;

        [[nodiscard]] auto find_field(string_view field_name) const noexcept -> const field_descriptor*;
    };

    /// @brief Type trait deducing the corresponding field_kind for supported C++ types.
    template <typename T>
    struct field_type_traits;

    template <>
    struct field_type_traits<bool>
    {
        static constexpr field_kind kind = field_kind::boolean;
    };

    template <>
    struct field_type_traits<int32_t>
    {
        static constexpr field_kind kind = field_kind::int32;
    };

    template <>
    struct field_type_traits<uint32_t>
    {
        static constexpr field_kind kind = field_kind::uint32;
    };

    template <>
    struct field_type_traits<int64_t>
    {
        static constexpr field_kind kind = field_kind::int64;
    };

    template <>
    struct field_type_traits<uint64_t>
    {
        static constexpr field_kind kind = field_kind::uint64;
    };

    template <>
    struct field_type_traits<float>
    {
        static constexpr field_kind kind = field_kind::float32;
    };

    template <>
    struct field_type_traits<double>
    {
        static constexpr field_kind kind = field_kind::float64;
    };

    template <>
    struct field_type_traits<math::vec2<float>>
    {
        static constexpr field_kind kind = field_kind::vec2;
    };

    template <>
    struct field_type_traits<math::vec3<float>>
    {
        static constexpr field_kind kind = field_kind::vec3;
    };

    template <>
    struct field_type_traits<math::vec4<float>>
    {
        static constexpr field_kind kind = field_kind::vec4;
    };

    template <>
    struct field_type_traits<math::quat<float>>
    {
        static constexpr field_kind kind = field_kind::quat;
    };

    template <>
    struct field_type_traits<entity>
    {
        static constexpr field_kind kind = field_kind::entity_ref;
    };

    template <>
    struct field_type_traits<guid>
    {
        static constexpr field_kind kind = field_kind::guid;
    };

    template <typename T>
        requires(is_enum_v<T> && !is_same_v<T, entity>)
    struct field_type_traits<T>
    {
        static constexpr field_kind kind = field_kind::enumeration;
    };

    template <typename T>
    inline constexpr field_kind field_type_traits_v = field_type_traits<remove_cvref_t<T>>::kind;

    namespace detail
    {
        template <typename T>
        struct member_pointer_traits;

        template <typename ClassType, typename MemberType>
        struct member_pointer_traits<MemberType ClassType::*>
        {
            using class_type = ClassType;
            using member_type = MemberType;
        };
    } // namespace detail

    /// @brief Fluent builder used to declare component layout, member offsets, and property bindings.
    template <typename Class>
    class type_builder
    {
      public:
        static constexpr uint32_t default_version = 1;

        explicit type_builder(string_view name) : _name{name}
        {
        }

        auto version(uint32_t ver) -> type_builder&
        {
            _version = ver;
            return *this;
        }

        template <auto MemberPtr>
        auto field(string_view name, field_flags flags = field_flags::none, field_ui_hints hints = {}) -> type_builder&
        {
            using member_traits = detail::member_pointer_traits<decltype(MemberPtr)>;
            using member_type = typename member_traits::member_type;
            using class_type = typename member_traits::class_type;
            static_assert(is_same_v<class_type, Class>, "Member pointer must belong to the reflected class");

            alignas(alignof(Class)) auto storage = array<byte, sizeof(Class)>{};
            const auto* dummy = reinterpret_cast<const Class*>(storage.data());
            const auto offset = static_cast<size_t>(reinterpret_cast<const byte*>(&(dummy->*MemberPtr)) -
                                                    reinterpret_cast<const byte*>(dummy));

            auto getter = [](const void* component_ptr, void* out_value) -> void {
                const auto* object_ptr = static_cast<const Class*>(component_ptr);
                *static_cast<member_type*>(out_value) = object_ptr->*MemberPtr;
            };

            auto setter = [](void* component_ptr, const void* in_value) -> void {
                auto* object_ptr = static_cast<Class*>(component_ptr);
                object_ptr->*MemberPtr = *static_cast<const member_type*>(in_value);
            };

            _fields.push_back(field_descriptor{
                .name = name,
                .kind = field_type_traits_v<member_type>,
                .flags = flags,
                .offset = offset,
                .size = sizeof(member_type),
                .asset_type_hash = 0,
                .enum_entries = {},
                .hints = tempest::move(hints),
                .get = getter,
                .set = setter,
            });

            return *this;
        }

        template <auto MemberPtr>
        auto field(string_view name, decltype(MemberPtr) /*unused*/, field_flags flags = field_flags::none,
                   field_ui_hints hints = {}) -> type_builder&
        {
            return field<MemberPtr>(name, flags, hints);
        }

        template <auto MemberPtr>
        auto field(string_view name, span<const enum_entry> entries, field_flags flags = field_flags::none,
                   field_ui_hints hints = {}) -> type_builder&
        {
            using member_traits = detail::member_pointer_traits<decltype(MemberPtr)>;
            using member_type = typename member_traits::member_type;
            using class_type = typename member_traits::class_type;
            static_assert(is_same_v<class_type, Class>, "Member pointer must belong to the reflected class");

            alignas(alignof(Class)) auto storage = array<byte, sizeof(Class)>{};
            const auto* dummy = reinterpret_cast<const Class*>(storage.data());
            const auto offset = static_cast<size_t>(reinterpret_cast<const byte*>(&(dummy->*MemberPtr)) -
                                                    reinterpret_cast<const byte*>(dummy));

            auto getter = [](const void* component_ptr, void* out_value) -> void {
                const auto* object_ptr = static_cast<const Class*>(component_ptr);
                *static_cast<member_type*>(out_value) = object_ptr->*MemberPtr;
            };

            auto setter = [](void* component_ptr, const void* in_value) -> void {
                auto* object_ptr = static_cast<Class*>(component_ptr);
                object_ptr->*MemberPtr = *static_cast<const member_type*>(in_value);
            };

            _fields.push_back(field_descriptor{
                .name = name,
                .kind = field_kind::enumeration,
                .flags = flags,
                .offset = offset,
                .size = sizeof(member_type),
                .asset_type_hash = 0,
                .enum_entries = entries,
                .hints = tempest::move(hints),
                .get = getter,
                .set = setter,
            });

            return *this;
        }

        template <auto MemberPtr>
        auto field(string_view name, decltype(MemberPtr) /*unused*/, span<const enum_entry> entries,
                   field_flags flags = field_flags::none, field_ui_hints hints = {}) -> type_builder&
        {
            return field<MemberPtr>(name, entries, flags, hints);
        }

        template <typename Member>
        auto field(string_view name, Member Class::* member_ptr, field_flags flags = field_flags::none,
                   field_ui_hints hints = {}) -> type_builder&
        {
            alignas(alignof(Class)) auto storage = array<byte, sizeof(Class)>{};
            const auto* dummy = reinterpret_cast<const Class*>(storage.data());
            const auto offset = static_cast<size_t>(reinterpret_cast<const byte*>(&(dummy->*member_ptr)) -
                                                    reinterpret_cast<const byte*>(dummy));

            _fields.push_back(field_descriptor{
                .name = name,
                .kind = field_type_traits_v<Member>,
                .flags = flags,
                .offset = offset,
                .size = sizeof(Member),
                .asset_type_hash = 0,
                .enum_entries = {},
                .hints = tempest::move(hints),
                .get = nullptr,
                .set = nullptr,
            });

            return *this;
        }

        template <typename Member>
        auto field(string_view name, Member Class::* member_ptr, span<const enum_entry> entries,
                   field_flags flags = field_flags::none, field_ui_hints hints = {}) -> type_builder&
        {
            alignas(alignof(Class)) auto storage = array<byte, sizeof(Class)>{};
            const auto* dummy = reinterpret_cast<const Class*>(storage.data());
            const auto offset = static_cast<size_t>(reinterpret_cast<const byte*>(&(dummy->*member_ptr)) -
                                                    reinterpret_cast<const byte*>(dummy));

            _fields.push_back(field_descriptor{
                .name = name,
                .kind = field_kind::enumeration,
                .flags = flags,
                .offset = offset,
                .size = sizeof(Member),
                .asset_type_hash = 0,
                .enum_entries = entries,
                .hints = tempest::move(hints),
                .get = nullptr,
                .set = nullptr,
            });

            return *this;
        }

        template <auto Getter, auto Setter>
        auto property(string_view name, field_flags flags = field_flags::none, field_ui_hints hints = {})
            -> type_builder&
        {
            using member_type = remove_cvref_t<decltype(tempest::invoke(Getter, *static_cast<const Class*>(nullptr)))>;

            auto getter_fn = [](const void* component_ptr, void* out_value) -> void {
                const auto* object_ptr = static_cast<const Class*>(component_ptr);
                *static_cast<member_type*>(out_value) = tempest::invoke(Getter, *object_ptr);
            };

            auto setter_fn = [](void* component_ptr, const void* in_value) -> void {
                auto* object_ptr = static_cast<Class*>(component_ptr);
                tempest::invoke(Setter, *object_ptr, *static_cast<const member_type*>(in_value));
            };

            _fields.push_back(field_descriptor{
                .name = name,
                .kind = field_type_traits_v<member_type>,
                .flags = flags,
                .offset = field_descriptor::no_offset,
                .size = sizeof(member_type),
                .asset_type_hash = 0,
                .enum_entries = {},
                .hints = tempest::move(hints),
                .get = getter_fn,
                .set = setter_fn,
            });

            return *this;
        }

        template <auto Getter, auto Setter>
        auto property(string_view name, decltype(Getter) /*unused*/, decltype(Setter) /*unused*/,
                      field_flags flags = field_flags::none, field_ui_hints hints = {}) -> type_builder&
        {
            return property<Getter, Setter>(name, flags, hints);
        }

        template <typename Getter, typename Setter>
            requires(is_empty_v<Getter> && is_empty_v<Setter> && is_default_constructible_v<Getter> &&
                     is_default_constructible_v<Setter>)
        auto property(string_view name, Getter /*unused*/, Setter /*unused*/, field_flags flags = field_flags::none,
                      field_ui_hints hints = {}) -> type_builder&
        {
            using member_type = remove_cvref_t<decltype(Getter{}(*static_cast<const Class*>(nullptr)))>;

            auto getter_fn = [](const void* component_ptr, void* out_value) -> void {
                const auto* object_ptr = static_cast<const Class*>(component_ptr);
                *static_cast<member_type*>(out_value) = Getter{}(*object_ptr);
            };

            auto setter_fn = [](void* component_ptr, const void* in_value) -> void {
                auto* object_ptr = static_cast<Class*>(component_ptr);
                Setter{}(*object_ptr, *static_cast<const member_type*>(in_value));
            };

            _fields.push_back(field_descriptor{
                .name = name,
                .kind = field_type_traits_v<member_type>,
                .flags = flags,
                .offset = field_descriptor::no_offset,
                .size = sizeof(member_type),
                .asset_type_hash = 0,
                .enum_entries = {},
                .hints = tempest::move(hints),
                .get = getter_fn,
                .set = setter_fn,
            });

            return *this;
        }

        template <typename Fn>
            requires(is_empty_v<Fn> && is_default_constructible_v<Fn>)
        auto post_load(Fn /*unused*/) -> type_builder&
        {
            _post_load = [](void* component_ptr) -> void {
                auto* object_ptr = static_cast<Class*>(component_ptr);
                Fn{}(*object_ptr);
            };
            return *this;
        }

        template <auto Fn>
        auto post_load() -> type_builder&
        {
            _post_load = [](void* component_ptr) -> void {
                auto* object_ptr = static_cast<Class*>(component_ptr);
                tempest::invoke(Fn, *object_ptr);
            };
            return *this;
        }

        auto post_load(void (*func)(void*)) -> type_builder&
        {
            _post_load = func;
            return *this;
        }

        auto build() -> type_descriptor
        {
            return type_descriptor{
                .name = _name,
                .size = sizeof(Class),
                .alignment = alignof(Class),
                .version = _version,
                .fields = tempest::move(_fields),
                .post_load = _post_load,
            };
        }

      private:
        string_view _name;
        uint32_t _version = default_version;
        vector<field_descriptor> _fields;
        void (*_post_load)(void* component_ptr) = nullptr;
    };

    /// @brief Primary template for compile-time reflection descriptors. Specialized per component.
    template <typename T>
    struct reflect;

    /// @brief Concept satisfied by any type providing a valid static describe() reflection binding.
    template <typename T>
    concept reflected_component = requires(type_builder<T>& builder) { reflect<T>::describe(builder); };
} // namespace tempest::ecs

#endif // tempest_ecs_reflection_hpp
