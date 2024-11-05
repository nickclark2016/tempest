#ifndef tempest_core_variant_hpp
#define tempest_core_variant_hpp

#include <tempest/functional.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    template <typename... Ts>
    class variant;

    template <typename T>
    struct variant_size;

    template <typename... Ts>
    struct variant_size<variant<Ts...>> : integral_constant<size_t, sizeof...(Ts)>
    {
    };

    template <typename T>
    inline constexpr size_t variant_size_v = variant_size<T>::value;

    namespace detail
    {
        template <typename T>
        inline consteval T consteval_max_val(T t)
        {
            return t;
        }

        template <typename T, typename... Ts>
        inline consteval T consteval_max_val(T t, Ts... ts)
        {
            auto rhs = consteval_max_val(ts...);
            return t > rhs ? t : rhs;
        }

        template <typename... Ts>
        inline consteval size_t consteval_max_size()
        {
            return consteval_max_val(sizeof(Ts)...);
        }

        template <typename T>
        inline consteval T consteval_least_common_multiple(T t)
        {
            return t;
        }

        template <typename T>
        inline consteval T consteval_greatest_common_denominator(T a, T b)
        {
            while (b != 0)
            {
                auto t = b;
                b = a % b;
                a = t;
            }

            return a;
        }

        template <typename T, typename... Ts>
        inline consteval T consteval_greatest_common_denominator(T t, Ts... ts)
        {
            auto rhs = consteval_greatest_common_denominator(ts...);
            return consteval_greatest_common_denominator(t, rhs);
        }

        template <typename T, typename... Ts>
        inline consteval T consteval_least_common_multiple(T t, Ts... ts)
        {
            auto rhs = consteval_least_common_multiple(ts...);
            return (t * rhs) / consteval_greatest_common_denominator(t, rhs);
        }

        template <typename T>
        inline consteval T consteval_minimum_alignment()
        {
            return alignof(T);
        }

        template <typename T1, typename... Ts>
        inline consteval size_t consteval_minimum_alignment()
        {
            return consteval_least_common_multiple(alignof(T1), alignof(Ts)...);
        }

        template <typename... Ts>
        union variant_storage {
            alignas(consteval_minimum_alignment<Ts...>()) byte data[consteval_max_size<Ts...>()];
        };

        template <size_t C>
        using variant_index_type =
            conditional_t<C <= 0xFF, uint8_t,
                          conditional_t<C <= 0xFFFF, uint16_t, conditional_t<C <= 0xFFFFFFFF, uint32_t, uint64_t>>>;

        template <typename...>
        struct type_list
        {
            inline static constexpr size_t size = 0;
        };

        template <typename T1>
        struct type_list<T1>
        {
            inline static constexpr size_t size = 1;

            using head = T1;
            using rest = type_list<>;
        };

        template <typename T1, typename T2, typename... Ts>
        struct type_list<T1, T2, Ts...>
        {
            inline static constexpr size_t size = 2 + sizeof...(Ts);

            using head = T1;
            using rest = type_list<T2, Ts...>;
        };

        template <size_t Idx, size_t Target, typename T1, typename... Ts>
        struct type_at_index_helper
        {
            using type = conditional_t<Idx == Target, T1, typename type_at_index_helper<Idx + 1, Target, Ts...>::type>;
        };

        template <size_t Idx, size_t Target, typename T>
        struct type_at_index_helper<Idx, Target, T>
        {
            using type = conditional_t<Idx == Target, T, void>;
        };

        template <size_t Idx, typename... Ts>
        using type_at_index = typename type_at_index_helper<0, Idx, Ts...>::type;
    } // namespace detail

    template <size_t I, typename T>
    struct variant_alternative;

    template <size_t I, typename... T>
    struct variant_alternative<I, variant<T...>>
    {
        using type = detail::type_at_index<I, T...>;
    };

    template <size_t I, typename T>
    using variant_alternative_t = typename variant_alternative<I, T>::type;

    namespace detail
    {
        template <size_t Index, typename...>
        struct variant_construction_helper;

        template <size_t Index, typename... Ts>
        struct variant_construction_helper<Index, type_list<Ts...>>
        {
            template <typename... Args>
            inline static constexpr void construct(size_t index, void* addr, Args&&... args)
            {
                if (index == Index)
                {
                    if constexpr (tempest::is_constructible_v<typename type_list<Ts...>::head, Args...>)
                    {
                        (void)tempest::construct_at(reinterpret_cast<typename type_list<Ts...>::head*>(addr),
                                                    tempest::forward<Args>(args)...);
                    }
                }
                else if constexpr (sizeof...(Ts) > 1)
                {
                    variant_construction_helper<Index + 1, typename type_list<Ts...>::rest>::construct(
                        index, addr, tempest::forward<Args>(args)...);
                }
                else if constexpr (sizeof...(Ts) == 0)
                {
                    static_assert(false, "Index out of bounds.");
                }
                else
                {
                    tempest::unreachable();
                }
            }
        };

        struct variant_helper
        {
            template <typename... Ts, typename... Args>
            inline static constexpr void construct(size_t index, void* addr, Args&&... args)
            {
                variant_construction_helper<0, type_list<Ts...>>::construct(index, addr,
                                                                            tempest::forward<Args>(args)...);
            }

            template <size_t Idx, typename... Ts, typename... Us>
                requires(sizeof...(Ts) > 0)
            inline static constexpr void copy_construct(size_t index, const void* src, void* dst, type_list<Us...>)
            {
                if (index == Idx)
                {
                    using T = type_at_index<Idx, Us...>;
                    (void)tempest::construct_at(reinterpret_cast<T*>(dst), *reinterpret_cast<const T*>(src));
                }
                else if constexpr (Idx + 1 < sizeof...(Us))
                {
                    variant_helper::copy_construct<Idx + 1, Ts...>(index, src, dst, type_list<Us...>{});
                }
                else
                {
                    tempest::unreachable();
                }
            }

            template <size_t Idx, typename T, typename... Us>
            inline static constexpr void copy_construct(size_t index, const void* src, void* dst, type_list<Us...>)
            {
                if (index == Idx)
                {
                    (void)tempest::construct_at(reinterpret_cast<T*>(dst), *reinterpret_cast<const T*>(src));
                }
                else
                {
                    tempest::unreachable();
                }
            }

            template <size_t Idx, typename... Ts, typename... Us>
                requires(sizeof...(Ts) > 0)
            inline static constexpr void move_construct(size_t index, void* src, void* dst, type_list<Us...>)
            {
                if (index == Idx)
                {
                    static_assert(Idx < sizeof...(Us), "Index out of bounds.");
                    using T = type_at_index<Idx, Us...>;
                    (void)tempest::construct_at(reinterpret_cast<T*>(dst), tempest::move(*reinterpret_cast<T*>(src)));
                }
                else if constexpr (Idx + 1 < sizeof...(Us))
                {
                    variant_helper::move_construct<Idx + 1, Ts...>(index, src, dst, type_list<Us...>{});
                }
                else
                {
                    tempest::unreachable();
                }
            }

            template <size_t Idx, typename T, typename... Us>
            inline static constexpr void move_construct(size_t index, void* src, void* dst, type_list<Us...>)
            {
                if (index == Idx)
                {
                    (void)tempest::construct_at(reinterpret_cast<T*>(dst), tempest::move(*reinterpret_cast<T*>(src)));
                }
                else
                {
                    tempest::unreachable();
                }
            }

            template <size_t Idx, typename T, typename... Ts>
                requires(sizeof...(Ts) > 0)
            inline static constexpr void destroy(size_t index, void* data)
            {
                if (index == Idx)
                {
                    tempest::destroy_at(reinterpret_cast<T*>(data));
                }
                else
                {
                    variant_helper::destroy<Idx + 1, Ts...>(index, data);
                }
            }

            template <size_t Idx, typename T>
            inline static constexpr void destroy(size_t index, void* data)
            {
                if (index == Idx)
                {
                    tempest::destroy_at(reinterpret_cast<T*>(data));
                }
                else
                {
                    tempest::unreachable();
                }
            }

            template <size_t Idx, typename T, typename... Ts>
                requires(sizeof...(Ts) > 0)
            inline static constexpr void copy_assign(size_t index, const void* src, void* dst)
            {
                if (index == Idx)
                {
                    *reinterpret_cast<T*>(dst) = *reinterpret_cast<const T*>(src);
                }
                else
                {
                    variant_helper::copy_assign<Idx + 1, Ts...>(index, src, dst);
                }
            }

            template <size_t Idx, typename T>
            inline static constexpr void copy_assign(size_t index, const void* src, void* dst)
            {
                if (index == Idx)
                {
                    *reinterpret_cast<T*>(dst) = *reinterpret_cast<const T*>(src);
                }
                else
                {
                    tempest::unreachable();
                }
            }

            template <size_t Idx, typename T, typename... Ts>
                requires(sizeof...(Ts) > 0)
            inline static constexpr void move_assign(size_t index, void* src, void* dst)
            {
                if (index == Idx)
                {
                    *reinterpret_cast<T*>(dst) = tempest::move(*reinterpret_cast<T*>(src));
                }
                else
                {
                    variant_helper::move_assign<Idx + 1, Ts...>(index, src, dst);
                }
            }

            template <size_t Idx, typename T>
            inline static constexpr void move_assign(size_t index, void* src, void* dst)
            {
                if (index == Idx)
                {
                    *reinterpret_cast<T*>(dst) = tempest::move(*reinterpret_cast<T*>(src));
                }
                else
                {
                    tempest::unreachable();
                }
            }

            template <size_t Idx, typename... Ts, typename... Us>
                requires(sizeof...(Ts) > 0)
            inline static constexpr void swap(size_t index, void* lhs, void* rhs, type_list<Us...>)
            {
                if (index == Idx)
                {
                    using T = type_at_index<Idx, Us...>;
                    tempest::swap(*reinterpret_cast<T*>(lhs), *reinterpret_cast<T*>(rhs));
                }
                else
                {
                    variant_helper::swap<Idx + 1, Ts...>(index, lhs, rhs, type_list<Us...>{});
                }
            }

            template <size_t Idx, typename T, typename... Us>
            inline static constexpr void swap(size_t index, void* lhs, void* rhs, type_list<Us...>)
            {
                if (index == Idx)
                {
                    tempest::swap(*reinterpret_cast<T*>(lhs), *reinterpret_cast<T*>(rhs));
                }
                else
                {
                    tempest::unreachable();
                }
            }
        };

        template <size_t Idx, typename T, typename U, typename... Ts>
        struct variant_index_of
        {
            static constexpr size_t value = variant_index_of<Idx + 1, T, Ts...>::value;
        };

        template <size_t Idx, typename T, typename... Ts>
        struct variant_index_of<Idx, T, T, Ts...>
        {
            static constexpr size_t value = Idx;
        };

        template <size_t Idx, typename T, typename U>
            requires(!is_same_v<T, U>)
        struct variant_index_of<Idx, T, U>;

        // Index selection criteria:
        // 1. If the type is in the variant, return the index of the type.
        // 2. If the type is not in the variant, but decays to a type in the variant, return the index of the decayed
        // type.
        // 3. If the type is not in the variant, but is convertible to a type in the variant, return the index of the
        // convertible type.
        // 4. If the type is not in the variant, but is constructible from a type in the variant, return the index of
        // the constructible type.

        template <typename T, typename... Ts>
        struct is_type_in_list
        {
            static constexpr bool value = (is_same_v<T, Ts> || ...);
        };

        template <typename T, typename... Ts>
        struct is_decayed_type_in_list
        {
            static constexpr bool value = (is_same_v<decay_t<T>, Ts> || ...);
        };

        template <typename T, typename... Ts>
        struct is_convertible_type_in_list
        {
            static constexpr bool value = (is_convertible_v<T, Ts> || ...);
        };

        template <typename T, typename... Ts>
        struct is_constructible_type_in_list
        {
            static constexpr bool value = (is_constructible_v<T, Ts> || ...);
        };

        template <typename T, typename... Ts>
            requires(is_type_in_list<T, Ts...>::value || is_decayed_type_in_list<T, Ts...>::value ||
                     is_convertible_type_in_list<T, Ts...>::value || is_constructible_type_in_list<T, Ts...>::value)
        struct variant_index_selector
        {
            static constexpr size_t index =
                is_type_in_list<T, Ts...>::value               ? variant_index_of<0, T, Ts...>::value
                : is_decayed_type_in_list<T, Ts...>::value     ? variant_index_of<0, decay_t<T>, Ts...>::value
                : is_convertible_type_in_list<T, Ts...>::value ? variant_index_of<0, T, Ts...>::value
                : is_constructible_v<T, Ts...>                 ? variant_index_of<0, T, Ts...>::value
                                                               : sizeof...(Ts);
        };

        // Test if there are any duplicate types in the list.
        template <typename... Ts>
        struct has_duplicate_types
        {
            static constexpr bool value = false;
        };

        template <typename T, typename... Ts>
        struct has_duplicate_types<T, T, Ts...>
        {
            static constexpr bool value = true;
        };

        template <typename T, typename U, typename... Ts>
        struct has_duplicate_types<T, U, Ts...>
        {
            static constexpr bool value = has_duplicate_types<T, Ts...>::value || has_duplicate_types<U, Ts...>::value;
        };

        template <bool IsValid, typename T>
        struct construction_dispatcher;

        template <typename T>
        struct construction_dispatcher<false, T>
        {
            template <size_t Idx, typename... Args>
            static constexpr void switch_case(size_t index, void* addr, Args&&... args)
            {
                tempest::unreachable();
            }

            template <size_t Base, size_t TargetIdx, typename... Args>
            static constexpr void switch_dispatch(size_t index, void* addr, Args&&... args)
            {
                tempest::unreachable();
            }
        };

        template <typename T>
        struct construction_dispatcher<true, T>
        {
            template <size_t Idx, typename... Args>
            static constexpr void switch_case(void* addr, Args&&... args)
            {
                using actual_type = variant_alternative_t<Idx, T>;

                if constexpr (is_constructible_v<actual_type, Args...>)
                {
                    (void)tempest::construct_at(reinterpret_cast<actual_type*>(addr), tempest::forward<Args>(args)...);
                }
                else
                {
                    unreachable();
                }
            }

            template <size_t Base, size_t TargetIdx, typename... Args>
            static constexpr void switch_dispatch(void* addr, Args&&... args)
            {
                constexpr auto sz = variant_size_v<T>;
                if constexpr (Base == TargetIdx)
                {
                    construction_dispatcher <
                        Base<sz, T>::template switch_case<Base>(addr, tempest::forward<Args>(args)...);
                }
                else
                {
                    construction_dispatcher<Base + 1 < sz, T>::template switch_dispatch<Base + 1, TargetIdx>(
                        addr, tempest::forward<Args>(args)...);
                }
            }
        };
    } // namespace detail

    struct monostate_t
    {
        constexpr explicit monostate_t() = default;
    };

    inline constexpr monostate_t monostate{};

    template <typename... Ts>
    class variant
    {
      public:
        static_assert(sizeof...(Ts) > 0, "Variant must have at least one type.");
        static_assert(!detail::has_duplicate_types<Ts...>::value, "Variant cannot have duplicate types.");

        constexpr variant();
        constexpr variant(const variant& other);
        constexpr variant(variant&& other) noexcept;

        template <typename T>
            requires(!is_same_v<remove_cvref_t<T>, variant<Ts...>>)
        constexpr variant(T&& value) noexcept;

        template <typename T, typename... Args>
            requires is_constructible_v<T, Args...> && detail::is_type_in_list<T, Ts...>::value
        constexpr explicit variant(in_place_type_t<T>, Args&&... args);

        template <size_t I, typename... Args>
            requires is_constructible_v<variant_alternative_t<I, variant<Ts...>>, Args...> && (I < sizeof...(Ts))
        constexpr explicit variant(in_place_index_t<I>, Args&&... args);

        constexpr ~variant();

        constexpr variant& operator=(const variant& other);
        constexpr variant& operator=(variant&& other) noexcept;

        template <typename T>
            requires(!is_same_v<remove_cvref_t<T>, variant<Ts...>>)
        constexpr variant& operator=(T&& value) noexcept;

        [[nodiscard]] constexpr size_t index() const noexcept;

        constexpr void swap(variant& other) noexcept;

        template <typename Callable>
        constexpr decltype(auto) visit(Callable&& callable);

        template <typename Callable>
        constexpr decltype(auto) visit(Callable&& callable) const;

        template <typename R, typename Callable>
        constexpr R visit(Callable&& callable);

        template <typename R, typename Callable>
        constexpr R visit(Callable&& callable) const;

      private:
        detail::variant_storage<Ts...> _storage;
        detail::variant_index_type<sizeof...(Ts)> _index;

        template <size_t I, typename... Ts>
        friend constexpr variant_alternative_t<I, variant<Ts...>>& get(variant<Ts...>& v);

        template <size_t I, typename... Ts>
        friend constexpr const variant_alternative_t<I, variant<Ts...>>& get(const variant<Ts...>& v);

        template <size_t I, typename... Ts>
        friend constexpr variant_alternative_t<I, variant<Ts...>>&& get(variant<Ts...>&& v);

        template <size_t I, typename... Ts>
        friend constexpr const variant_alternative_t<I, variant<Ts...>>&& get(const variant<Ts...>&& v);
    };

    template <typename... Ts>
    inline constexpr variant<Ts...>::variant() : _index{0}
    {
        detail::construction_dispatcher<true, variant<Ts...>>::switch_dispatch<0, 0>(&_storage.data);
    }

    template <typename... Ts>
    inline constexpr variant<Ts...>::variant(const variant& other) : _index{other._index}
    {
        detail::variant_helper::copy_construct<0, Ts...>(other._index, &other._storage.data, &_storage.data,
                                                         detail::type_list<Ts...>{});
    }

    template <typename... Ts>
    inline constexpr variant<Ts...>::variant(variant&& other) noexcept : _index{other._index}
    {
        detail::variant_helper::move_construct<0, Ts...>(other._index, &other._storage.data, &_storage.data,
                                                         detail::type_list<Ts...>{});
    }

    template <typename... Ts>
    template <typename T>
        requires(!is_same_v<remove_cvref_t<T>, variant<Ts...>>)
    inline constexpr variant<Ts...>::variant(T&& value) noexcept
        : _index{detail::variant_index_selector<T, Ts...>::index}
    {
        detail::construction_dispatcher<true, variant<Ts...>>::switch_dispatch<
            0, detail::variant_index_selector<T, Ts...>::index>(&_storage.data, tempest::forward<T>(value));
    }

    template <typename... Ts>
    template <typename T, typename... Args>
        requires is_constructible_v<T, Args...> && detail::is_type_in_list<T, Ts...>::value
    inline constexpr variant<Ts...>::variant(in_place_type_t<T>, Args&&... args)
        : _index{detail::variant_index_of<0, T, Ts...>::value}
    {
        (void)tempest::construct_at(reinterpret_cast<T*>(&_storage.data), tempest::forward<Args>(args)...);
    }

    template <typename... Ts>
    template <size_t I, typename... Args>
        requires is_constructible_v<variant_alternative_t<I, variant<Ts...>>, Args...> && (I < sizeof...(Ts))
    inline constexpr variant<Ts...>::variant(in_place_index_t<I>, Args&&... args) : _index{I}
    {
        using T = detail::type_at_index<I, Ts...>;
        (void)tempest::construct_at(reinterpret_cast<T*>(&_storage.data), tempest::forward<Args>(args)...);
    }

    template <typename... Ts>
    inline constexpr variant<Ts...>::~variant()
    {
        detail::variant_helper::destroy<0, Ts...>(_index, &_storage.data);
    }

    template <typename... Ts>
    inline constexpr variant<Ts...>& variant<Ts...>::operator=(const variant& other)
    {
        if (this != &other)
        {
            if (_index == other._index)
            {
                detail::variant_helper::copy_assign<0, Ts...>(other._index, &other._storage.data, &_storage.data);
            }
            else
            {
                detail::variant_helper::destroy<0, Ts...>(_index, &_storage.data);
                detail::variant_helper::copy_construct<0, Ts...>(other._index, &other._storage.data, &_storage.data,
                                                                 detail::type_list<Ts...>{});
                _index = other._index;
            }
        }

        return *this;
    }

    template <typename... Ts>
    inline constexpr variant<Ts...>& variant<Ts...>::operator=(variant&& other) noexcept
    {
        if (this != &other)
        {
            if (_index == other._index)
            {
                detail::variant_helper::move_assign<0, Ts...>(other._index, &other._storage.data, &_storage.data);
            }
            else
            {
                detail::variant_helper::destroy<0, Ts...>(_index, &_storage.data);
                detail::variant_helper::move_construct<0, Ts...>(other._index, &other._storage.data, &_storage.data,
                                                                 detail::type_list<Ts...>{});
                _index = other._index;
            }
        }

        return *this;
    }

    template <typename... Ts>
    template <typename T>
        requires(!is_same_v<remove_cvref_t<T>, variant<Ts...>>)
    inline constexpr variant<Ts...>& variant<Ts...>::operator=(T&& value) noexcept
    {
        static constexpr auto index = detail::variant_index_selector<T, Ts...>::index;
        static_assert(index < sizeof...(Ts), "Type not in variant.");

        if (_index == index)
        {
            *reinterpret_cast<variant_alternative_t<index, variant<Ts...>>*>(&_storage.data) =
                tempest::forward<T>(value);
        }
        else
        {
            detail::variant_helper::destroy<0, Ts...>(_index, &_storage.data);
            detail::construction_dispatcher<true, variant<Ts...>>::switch_dispatch<0, index>(
                &_storage.data, tempest::forward<T>(value));
            _index = index;
        }

        return *this;
    }

    template <typename... Ts>
    [[nodiscard]] inline constexpr size_t variant<Ts...>::index() const noexcept
    {
        return _index;
    }

    template <typename... Ts>
    inline constexpr void variant<Ts...>::swap(variant& other) noexcept
    {
        if (_index == other._index)
        {
            detail::variant_helper::swap<0, Ts...>(_index, &_storage.data, &other._storage.data,
                                                   detail::type_list<Ts...>{});
        }
        else
        {
            variant<Ts...> tmp(tempest::move(*this));
            *this = tempest::move(other);
            other = tempest::move(tmp);
        }
    }

    template <size_t I, typename... Ts>
    [[nodiscard]] inline constexpr variant_alternative_t<I, variant<Ts...>>& get(variant<Ts...>& v)
    {
        static_assert(I < sizeof...(Ts), "Index out of bounds.");
        return *reinterpret_cast<variant_alternative_t<I, variant<Ts...>>*>(&v._storage.data);
    }

    template <size_t I, typename... Ts>
    [[nodiscard]] inline constexpr const variant_alternative_t<I, variant<Ts...>>& get(const variant<Ts...>& v)
    {
        static_assert(I < sizeof...(Ts), "Index out of bounds.");
        return *reinterpret_cast<const variant_alternative_t<I, variant<Ts...>>*>(&v._storage.data);
    }

    template <size_t I, typename... Ts>
    [[nodiscard]] inline constexpr variant_alternative_t<I, variant<Ts...>>&& get(variant<Ts...>&& v)
    {
        static_assert(I < sizeof...(Ts), "Index out of bounds.");
        return *reinterpret_cast<variant_alternative_t<I, variant<Ts...>>*>(&tempest::move(v)._storage.data);
    }

    template <size_t I, typename... Ts>
    [[nodiscard]] inline constexpr const variant_alternative_t<I, variant<Ts...>>&& get(const variant<Ts...>&& v)
    {
        static_assert(I < sizeof...(Ts), "Index out of bounds.");
        return *reinterpret_cast<const variant_alternative_t<I, variant<Ts...>>*>(&tempest::move(v)._storage.data);
    }

    template <typename T, typename... Ts>
    [[nodiscard]] inline constexpr T& get(variant<Ts...>& v)
    {
        return get<detail::variant_index_of<0, T, Ts...>::value>(v);
    }

    template <typename T, typename... Ts>
    [[nodiscard]] inline constexpr const T& get(const variant<Ts...>& v)
    {
        return get<detail::variant_index_of<0, T, Ts...>::value>(v);
    }

    template <typename T, typename... Ts>
    [[nodiscard]] inline constexpr T&& get(variant<Ts...>&& v)
    {
        return get<detail::variant_index_of<0, T, Ts...>::value>(tempest::move(v));
    }

    template <typename T, typename... Ts>
    [[nodiscard]] inline constexpr const T&& get(const variant<Ts...>&& v)
    {
        return get<detail::variant_index_of<0, T, Ts...>::value>(tempest::move(v));
    }

    template <size_t I, typename... Ts>
    [[nodiscard]] inline constexpr bool holds_alternative(const variant<Ts...>& v) noexcept
    {
        return v.index() == I;
    }

    template <typename T, typename... Ts>
    [[nodiscard]] inline constexpr bool holds_alternative(const variant<Ts...>& v) noexcept
    {
        return holds_alternative<detail::variant_index_of<0, T, Ts...>::value>(v);
    }

    template <size_t I, typename... Ts>
    [[nodiscard]] inline constexpr add_pointer_t<variant_alternative_t<I, variant<Ts...>>> get_if(
        variant<Ts...>* v) noexcept
    {
        return holds_alternative<I>(*v) ? addressof(get<I>(*v)) : nullptr;
    }

    template <size_t I, typename... Ts>
    [[nodiscard]] inline constexpr add_pointer_t<const variant_alternative_t<I, variant<Ts...>>> get_if(
        const variant<Ts...>* v) noexcept
    {
        return holds_alternative<I>(*v) ? addressof(get<I>(*v)) : nullptr;
    }

    template <typename T, typename... Ts>
    [[nodiscard]] inline constexpr add_pointer_t<T> get_if(variant<Ts...>* v) noexcept
    {
        return get_if<detail::variant_index_of<0, T, Ts...>::value>(v);
    }

    template <typename T, typename... Ts>
    [[nodiscard]] inline constexpr add_pointer_t<const T> get_if(const variant<Ts...>* v) noexcept
    {
        return get_if<detail::variant_index_of<0, T, Ts...>::value>(v);
    }

    namespace detail
    {
        template <bool IsValid, typename R>
        struct visitation_dispatcher;

        template <typename R>
        struct visitation_dispatcher<false, R>
        {
            template <size_t Idx, typename Fn, typename V>
            static constexpr R switch_case(Fn&& fn, V&& v)
            {
                tempest::unreachable();
            }

            template <size_t Base, typename Fn, typename V>
            static constexpr R switch_dispatch(size_t index, Fn&& fn, V&& v)
            {
                tempest::unreachable();
            }
        };

        template <typename R>
        struct visitation_dispatcher<true, R>
        {
            template <size_t Idx, typename Fn, typename V>
            static constexpr R switch_case(Fn&& fn, V&& v)
            {
                using actual_result_t = decltype(tempest::invoke(tempest::forward<Fn>(fn), get<Idx>(v)));
                static_assert(is_same_v<R, actual_result_t>, "Return types do not match.");
                return tempest::invoke(tempest::forward<Fn>(fn), get<Idx>(v));
            }

            template <size_t Base, typename Fn, typename V>
            static constexpr R switch_dispatch(size_t index, Fn&& fn, V&& v)
            {
                constexpr auto sz = variant_size_v<remove_cvref_t<V>>;
                switch (v.index())
                {
                case Base + 0:
                    return visitation_dispatcher<Base + 0 < sz, R>::template switch_case<Base + 0>(
                        tempest::forward<Fn>(fn), tempest::forward<V>(v));
                case Base + 1:
                    return visitation_dispatcher<Base + 1 < sz, R>::template switch_case<Base + 1>(
                        tempest::forward<Fn>(fn), tempest::forward<V>(v));
                case Base + 2:
                    return visitation_dispatcher<Base + 2 < sz, R>::template switch_case<Base + 2>(
                        tempest::forward<Fn>(fn), tempest::forward<V>(v));
                case Base + 3:
                    return visitation_dispatcher<Base + 3 < sz, R>::template switch_case<Base + 3>(
                        tempest::forward<Fn>(fn), tempest::forward<V>(v));
                case Base + 4:
                    return visitation_dispatcher<Base + 4 < sz, R>::template switch_case<Base + 4>(
                        tempest::forward<Fn>(fn), tempest::forward<V>(v));
                case Base + 5:
                    return visitation_dispatcher<Base + 5 < sz, R>::template switch_case<Base + 5>(
                        tempest::forward<Fn>(fn), tempest::forward<V>(v));
                case Base + 6:
                    return visitation_dispatcher<Base + 6 < sz, R>::template switch_case<Base + 6>(
                        tempest::forward<Fn>(fn), tempest::forward<V>(v));
                case Base + 7:
                    return visitation_dispatcher<Base + 7 < sz, R>::template switch_case<Base + 7>(
                        tempest::forward<Fn>(fn), tempest::forward<V>(v));
                default:
                    return visitation_dispatcher<Base + 8 < sz, R>::template switch_dispatch<Base + 8>(
                        index, tempest::forward<Fn>(fn), tempest::forward<V>(v));
                }
            }
        };
    } // namespace detail

    template <typename... Ts>
    template <typename Callable>
    inline constexpr decltype(auto) variant<Ts...>::visit(Callable&& callable)
    {
        using R = decltype(tempest::invoke(tempest::forward<Callable>(callable), get<0>(*this)));
        return detail::visitation_dispatcher<true, R>::switch_dispatch<0>(index(), tempest::forward<Callable>(callable),
                                                                          *this);
    }

    template <typename... Ts>
    template <typename Callable>
    inline constexpr decltype(auto) variant<Ts...>::visit(Callable&& callable) const
    {
        using R = decltype(tempest::invoke(tempest::forward<Callable>(callable), get<0>(*this)));
        return detail::visitation_dispatcher<true, R>::switch_dispatch<0>(index(), tempest::forward<Callable>(callable),
                                                                          *this);
    }

    template <typename... Ts>
    template <typename R, typename Callable>
    inline constexpr R variant<Ts...>::visit(Callable&& callable)
    {
        if constexpr (is_void_v<R>)
        {
            detail::visitation_dispatcher<true, R>::switch_dispatch<0>(index(), tempest::forward<Callable>(callable),
                                                                       *this);
        }
        else
        {
            return detail::visitation_dispatcher<true, R>::switch_dispatch<0>(
                index(), tempest::forward<Callable>(callable), *this);
        }
    }

    template <typename... Ts>
    template <typename R, typename Callable>
    inline constexpr R variant<Ts...>::visit(Callable&& callable) const
    {
        if constexpr (is_void_v<R>)
        {
            detail::visitation_dispatcher<true, R>::switch_dispatch<0>(index(), tempest::forward<Callable>(callable),
                                                                       *this);
        }
        else
        {
            return detail::visitation_dispatcher<true, R>::switch_dispatch<0>(
                index(), tempest::forward<Callable>(callable), *this);
        }
    }

    template <typename Callable, typename... Ts>
    inline constexpr decltype(auto) visit(Callable&& callable, variant<Ts...>& v)
    {
        return v.visit(tempest::forward<Callable>(callable));
    }

    template <typename Callable, typename... Ts>
    inline constexpr decltype(auto) visit(Callable&& callable, const variant<Ts...>& v)
    {
        return v.visit(tempest::forward<Callable>(callable));
    }

    template <typename R, typename Callable, typename... Ts>
    inline constexpr R visit(Callable&& callable, variant<Ts...>& v)
    {
        if constexpr (is_void_v<R>)
        {
            v.visit(tempest::forward<Callable>(callable));
        }
        else
        {
            return v.visit<R>(tempest::forward<Callable>(callable));
        }
    }

    template <typename R, typename Callable, typename... Ts>
    inline constexpr R visit(Callable&& callable, const variant<Ts...>& v)
    {
        if constexpr (is_void_v<R>)
        {
            v.visit(tempest::forward<Callable>(callable));
        }
        else
        {
            return v.visit<R>(tempest::forward<Callable>(callable));
        }
    }

    template <typename... Ts>
    inline constexpr void swap(variant<Ts...>& lhs, variant<Ts...>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
} // namespace tempest

#endif // tempest_core_variant_hpp