#ifndef tempest_core_functional_hpp
#define tempest_core_functional_hpp

#include <tempest/invoke.hpp>
#include <tempest/memory.hpp>
#include <tempest/meta.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

#include <new> // needed for placement new

namespace tempest
{
    template <typename T>
    class reference_wrapper
    {
        static constexpr T* test(T& r) noexcept
        {
            return &r;
        }
        static constexpr T* test(T&&) = delete;

      public:
        using type = T;

        template <typename U>
            requires(!is_same_v<reference_wrapper<T>, remove_cvref_t<U>> && is_convertible_v<U, T&> &&
                     !is_convertible_v<U, T &&>)
        constexpr reference_wrapper(U&& u) noexcept(is_nothrow_convertible_v<U&, T&>);

        constexpr reference_wrapper(const reference_wrapper& other) noexcept;

        constexpr reference_wrapper& operator=(const reference_wrapper& other) noexcept;

        operator T&() const noexcept;
        T& get() const noexcept;

        template <typename... Ts>
            requires is_invocable_v<T&, Ts...>
        constexpr invoke_result_t<T&, Ts...> operator()(Ts&&... ts) const noexcept(is_nothrow_invocable_v<T&, Ts...>);

      private:
        T* _ptr;
    };

    template <typename T>
    reference_wrapper(T&) -> reference_wrapper<T>;

    template <typename T>
    template <typename U>
        requires(!is_same_v<reference_wrapper<T>, remove_cvref_t<U>> && is_convertible_v<U, T&> &&
                 !is_convertible_v<U, T &&>)
    inline constexpr reference_wrapper<T>::reference_wrapper(U&& u) noexcept(is_nothrow_convertible_v<U&, T&>)
        : _ptr{&u}
    {
    }

    template <typename T>
    template <typename... Ts>
        requires is_invocable_v<T&, Ts...>
    inline constexpr invoke_result_t<T&, Ts...> reference_wrapper<T>::operator()(Ts&&... ts) const
        noexcept(is_nothrow_invocable_v<T&, Ts...>)
    {
        if constexpr (is_void_v<invoke_result_t<T&, Ts...>>)
        {
            invoke(get(), tempest::forward<Ts>(ts)...);
        }
        else
        {
            return invoke(get(), tempest::forward<Ts>(ts)...);
        }
    }

    template <typename T>
    inline constexpr reference_wrapper<T>::reference_wrapper(const reference_wrapper& other) noexcept : _ptr{other._ptr}
    {
    }

    template <typename T>
    inline constexpr reference_wrapper<T>& reference_wrapper<T>::operator=(const reference_wrapper& other) noexcept
    {
        _ptr = other._ptr;
        return *this;
    }

    template <typename T>
    inline reference_wrapper<T>::operator T&() const noexcept
    {
        return *_ptr;
    }

    template <typename T>
    inline T& reference_wrapper<T>::get() const noexcept
    {
        return *_ptr;
    }

    template <typename T>
    inline constexpr reference_wrapper<T> ref(T& t) noexcept
    {
        return reference_wrapper<T>(t);
    }

    template <typename T>
    inline constexpr reference_wrapper<T> ref(reference_wrapper<T> t) noexcept
    {
        return t;
    }

    template <typename T>
    inline constexpr reference_wrapper<T> ref(const T&&) = delete;

    template <typename T>
    inline constexpr reference_wrapper<const T> cref(const T& t) noexcept
    {
        return reference_wrapper<const T>(t);
    }

    template <typename T>
    inline constexpr reference_wrapper<const T> cref(reference_wrapper<T> t) noexcept
    {
        return reference_wrapper<const T>(t.get());
    }

    template <typename T>
    inline constexpr reference_wrapper<const T> cref(const T&&) = delete;

    template <typename T>
    struct unwrap_reference
    {
        using type = T;
    };

    template <typename T>
    struct unwrap_reference<reference_wrapper<T>>
    {
        using type = T;
    };

    template <typename T>
    using unwrap_reference_t = typename unwrap_reference<T>::type;

    template <typename T>
    struct unwrap_reference_decay : unwrap_reference<decay_t<T>>
    {
    };

    template <typename T>
    using unwrap_reference_decay_t = typename unwrap_reference_decay<T>::type;

    template <typename T>
    class function;

    namespace detail
    {
        template <typename T>
        struct is_location_invariant : tempest::is_trivially_copyable<T>
        {
        };

        class undefined_class_type;

        union non_copyable_types {
            void* object_ptr;
            const void* const_object_ptr;
            void (*function_ptr)();
            void (undefined_class_type::*member_ptr)();
        };

        union any_func_data {
            void* access() noexcept;
            const void* access() const noexcept;

            template <typename T>
            T& access() noexcept;

            template <typename T>
            const T& access() const noexcept;

            non_copyable_types unused;
            unsigned char pod_data[sizeof(non_copyable_types)];
        };

        inline void* any_func_data::access() noexcept
        {
            return &pod_data[0];
        }

        inline const void* any_func_data::access() const noexcept
        {
            return &pod_data[0];
        }

        template <typename T>
        T& any_func_data::access() noexcept
        {
            return *static_cast<T*>(access());
        }

        template <typename T>
        const T& any_func_data::access() const noexcept
        {
            return *static_cast<const T*>(access());
        }

        enum class function_manager_operation
        {
            GET_FUNCTOR_POINTER,
            CLONE_FUNCTOR,
            DESTROY_FUNCTOR,
        };

        class function_base
        {
          public:
            static constexpr size_t max_size = sizeof(non_copyable_types);
            static constexpr size_t max_alignment = alignof(non_copyable_types);

            template <typename Fn>
            class manager
            {
              protected:
                static constexpr bool is_stored_locally = is_location_invariant<Fn>::value && sizeof(Fn) <= max_size &&
                                                          alignof(Fn) <= max_alignment &&
                                                          max_alignment % alignof(Fn) == 0;

                static Fn* get_pointer(const any_func_data& data) noexcept;

              private:
                template <typename F, bool Local>
                static void _create(any_func_data& dst, F&& src);

                template <bool Local>
                static void _destroy(any_func_data& tgt);

              public:
                static void exec(any_func_data& tgt, const any_func_data& src, function_manager_operation op);

                template <typename F>
                static void init_functor(any_func_data& tgt, F&& src) noexcept(manager<Fn>::is_stored_locally &&
                                                                               is_nothrow_constructible_v<Fn, F>);

                template <typename Sig>
                static bool non_empty_function(const function<Sig>& f) noexcept;

                template <typename T>
                static bool non_empty_function(T* fp) noexcept;

                template <typename C, typename T>
                static bool non_empty_function(T C::*mp) noexcept;

                template <typename F>
                static bool non_empty_function(const F& f) noexcept;
            };

            function_base() = default;
            ~function_base();

            bool empty() const;

            using manager_fn_t = void (*)(any_func_data&, const any_func_data&, function_manager_operation);

            any_func_data _data;
            manager_fn_t _manager_fn;
        };

        template <typename Sig, typename Fn>
        class function_handler;

        template <typename R, typename Fn, typename... Args>
        class function_handler<R(Args...), Fn> : public function_base::manager<Fn>
        {
            using base = function_base::manager<Fn>;

          public:
            static constexpr bool is_nothrow_init = base::is_stored_locally && is_nothrow_constructible_v<Fn>;

            static void exec(any_func_data& tgt, const any_func_data& src, function_manager_operation op);
            static R invoke(const any_func_data& data, Args&&... args);
        };

        template <>
        class function_handler<void, void>
        {
          public:
            static bool exec([[maybe_unused]] any_func_data& tgt, [[maybe_unused]] const any_func_data& src,
                             [[maybe_unused]] function_manager_operation op);
        };

        template <typename Sig, typename Fn, bool valid = is_object_v<Fn>>
        struct function_target_handler : function_handler<Sig, remove_cv_t<Fn>>
        {
        };

        template <typename Sig, typename Fn>
        struct function_target_handler<Sig, Fn, false> : function_handler<void, void>
        {
        };

        template <typename Fn>
        inline Fn* function_base::manager<Fn>::get_pointer(const any_func_data& data) noexcept
        {
            if constexpr (is_stored_locally)
            {
                const Fn& f = data.access<Fn>();
                return const_cast<Fn*>(&f); // This is safe because the object is not const
            }
            else
            {
                return data.access<Fn*>();
            }
        }

        template <typename Fn>
        template <typename F, bool Local>
        inline void function_base::manager<Fn>::_create(any_func_data& dst, F&& src)
        {
            if constexpr (Local)
            {
                ::new (dst.access()) Fn(tempest::forward<F>(src));
            }
            else
            {
                dst.access<Fn*>() = new Fn(tempest::forward<F>(src));
            }
        }

        template <typename Fn>
        template <bool Local>
        inline void function_base::manager<Fn>::_destroy(any_func_data& tgt)
        {
            if constexpr (Local)
            {
                tgt.access<Fn>().~Fn();
            }
            else
            {
                delete tgt.access<Fn*>();
            }
        }

        template <typename Fn>
        inline void function_base::manager<Fn>::exec(any_func_data& dst, const any_func_data& src,
                                                     function_manager_operation op)
        {
            switch (op)
            {
            case function_manager_operation::GET_FUNCTOR_POINTER:
                dst.access<Fn*>() = get_pointer(src);
                break;
            case function_manager_operation::CLONE_FUNCTOR:
                init_functor(dst, forward_like<Fn>(*get_pointer(src)));
                break;
            case function_manager_operation::DESTROY_FUNCTOR:
                _destroy<is_stored_locally>(dst);
                break;
            }
        }

        template <typename Fn>
        template <typename F>
        inline void function_base::manager<Fn>::init_functor(any_func_data& tgt,
                                                             F&& src) noexcept(manager<Fn>::is_stored_locally &&
                                                                               is_nothrow_constructible_v<Fn, F>)
        {
            _create<F&&, is_stored_locally>(tgt, tempest::forward<F>(src));
        }

        template <typename Fn>
        template <typename Sig>
        inline bool function_base::manager<Fn>::non_empty_function(const function<Sig>& f) noexcept
        {
            return static_cast<bool>(f);
        }

        template <typename Fn>
        template <typename T>
        inline bool function_base::manager<Fn>::non_empty_function(T* fp) noexcept
        {
            return fp != nullptr;
        }

        template <typename Fn>
        template <typename C, typename T>
        inline bool function_base::manager<Fn>::non_empty_function(T C::*mp) noexcept
        {
            return mp != nullptr;
        }

        template <typename Fn>
        template <typename F>
        inline bool function_base::manager<Fn>::non_empty_function([[maybe_unused]] const F& f) noexcept
        {
            return true;
        }

        inline function_base::~function_base()
        {
            if (_manager_fn)
            {
                _manager_fn(_data, _data, function_manager_operation::DESTROY_FUNCTOR);
            }
        }

        inline bool function_base::empty() const
        {
            return !_manager_fn;
        }

        template <typename R, typename Fn, typename... Args>
        inline void function_handler<R(Args...), Fn>::exec(any_func_data& tgt, const any_func_data& src,
                                                           function_manager_operation op)
        {
            switch (op)
            {
            case function_manager_operation::GET_FUNCTOR_POINTER:
                tgt.access<Fn*>() = base::get_pointer(src);
                break;
            default:
                base::exec(tgt, src, op);
            }
        }

        template <typename R, typename Fn, typename... Args>
        inline R function_handler<R(Args...), Fn>::invoke(const any_func_data& data, Args&&... args)
        {
            return invoke_r<R>(*base::get_pointer(data), tempest::forward<Args>(args)...);
        }

        inline bool function_handler<void, void>::exec([[maybe_unused]] any_func_data& tgt,
                                                       [[maybe_unused]] const any_func_data& src,
                                                       [[maybe_unused]] function_manager_operation op)
        {
            return false;
        }
    } // namespace detail

    template <typename M, typename T>
    constexpr auto mem_fn(M T::*pm) noexcept
    {
        return [pm](T& obj, auto&&... args) -> decltype(auto) {
            return tempest::invoke(pm, obj, tempest::forward<decltype(args)>(args)...);
        };
    }

    template <typename R, typename... Args>
    class function<R(Args...)> : private detail::function_base
    {
        template <typename Fn>
        using handler = detail::function_target_handler<R(Args...), decay_t<Fn>>;

      public:
        using result_type = R;

        function() noexcept;
        function(nullptr_t) noexcept;
        function(const function& fn);
        function(function&& fn) noexcept;

        template <typename Fn>
            requires is_invocable_v<Fn, Args...> && (!is_same_v<decay_t<Fn>, function<R(Args...)>>)
        inline function(Fn&& fn) noexcept(handler<Fn>::is_nothrow_init) : detail::function_base()
        {
            using fn_handler = handler<Fn>;
            if (fn_handler::non_empty_function(fn))
            {
                fn_handler::init_functor(_data, tempest::forward<Fn>(fn));
                _invoker = &fn_handler::invoke;
                _manager_fn = &fn_handler::exec;
            }
        }

        ~function() = default;

        function& operator=(const function& fn);
        function& operator=(function&& fn) noexcept;
        function& operator=(nullptr_t) noexcept;

        template <typename Fn>
            requires is_invocable_v<Fn, Args...> && (!is_same_v<decay_t<Fn>, function<R(Args...)>>)
        inline function& operator=(Fn&& fn) noexcept(handler<Fn>::is_nothrow_init)
        {
            function(tempest::forward<Fn>(fn)).swap(*this);
            return *this;
        }

        template <typename Fn>
        function& operator=(reference_wrapper<Fn> fn) noexcept;

        void swap(function& fn) noexcept;

        explicit operator bool() const noexcept;

        R operator()(Args... args) const;

      private:
        using invoker_type_t = R (*)(const detail::any_func_data&, Args&&...);
        invoker_type_t _invoker = nullptr;
    };

    namespace detail
    {
        template <typename>
        struct function_deduction_guide_helper
        {
        };

        template <typename R, typename T, typename... Args>
        struct function_deduction_guide_helper<R (T::*)(Args...)>
        {
            using type = R(Args...);
        };

        template <typename R, typename T, typename... Args>
        struct function_deduction_guide_helper<R (T::*)(Args...)&>
        {
            using type = R(Args...);
        };

        template <typename R, typename T, typename... Args>
        struct function_deduction_guide_helper<R (T::*)(Args...) const>
        {
            using type = R(Args...);
        };

        template <typename R, typename T, typename... Args>
        struct function_deduction_guide_helper<R (T::*)(Args...) const&>
        {
            using type = R(Args...);
        };

#if __cpp_explicit_this_parameter >= 202110L

        template <typename R, typename T, typename... Args>
        struct function_deduction_guide_helper<R (*)(T*, Args...)>
        {
            using type = R(Args...);
        };

#endif

#if __cpp_static_call_operator >= 202207L

        template <typename StaticCallOperator>
        struct function_deduction_guide_static_helper
        {
        };

        template <typename R, typename... Args>
        struct function_deduction_guide_static_helper<R (*)(Args...)>
        {
            using type = R(Args...);
        };

        template <typename R, typename... Args>
        struct function_deduction_guide_static_helper<R (*)(Args...) noexcept>
        {
            using type = R(Args...);
        };

        template <typename F, typename O>
        using function_deduction_guide = typename conditional_t<requires(F& f) {
            (void)f.operator();
        }, function_deduction_guide_static_helper<O>, function_deduction_guide_helper<O>>::type;

#else

        template <typename F, typename O>
        using function_deduction_guide = typename function_deduction_guide_helper<O>::type;

#endif
    } // namespace detail

    template <typename R, typename... Args>
    function(R (*)(Args...)) -> function<R(Args...)>;

    template <typename Fn>
    function(Fn) -> function<detail::function_deduction_guide<Fn, decltype(&Fn::operator())>>;

    template <typename R, typename... Args>
    inline function<R(Args...)>::function() noexcept : detail::function_base()
    {
    }

    template <typename R, typename... Args>
    inline function<R(Args...)>::function(nullptr_t) noexcept : detail::function_base()
    {
    }

    template <typename R, typename... Args>
    inline function<R(Args...)>::function(const function& fn) : detail::function_base()
    {
        if (static_cast<bool>(fn))
        {
            fn._manager_fn(_data, fn._data, detail::function_manager_operation::CLONE_FUNCTOR);
            _invoker = fn._invoker;
            _manager_fn = fn._manager_fn;
        }
    }

    template <typename R, typename... Args>
    inline function<R(Args...)>::function(function&& fn) noexcept : detail::function_base()
    {
        if (static_cast<bool>(fn))
        {
            _data = fn._data;

            _invoker = tempest::exchange(fn._invoker, nullptr);
            _manager_fn = tempest::exchange(fn._manager_fn, nullptr);
        }
    }

    template <typename R, typename... Args>
    inline function<R(Args...)>& function<R(Args...)>::operator=(const function& fn)
    {
        function(fn).swap(*this);
        return *this;
    }

    template <typename R, typename... Args>
    inline function<R(Args...)>& function<R(Args...)>::operator=(function&& fn) noexcept
    {
        function(tempest::move(fn)).swap(*this);
        return *this;
    }

    template <typename R, typename... Args>
    inline function<R(Args...)>& function<R(Args...)>::operator=(nullptr_t) noexcept
    {
        if (_manager_fn)
        {
            _manager_fn(_data, _data, detail::function_manager_operation::DESTROY_FUNCTOR);
            _manager_fn = nullptr;
            _invoker = nullptr;
        }
        return *this;
    }

    template <typename R, typename... Args>
    template <typename Fn>
    inline function<R(Args...)>& function<R(Args...)>::operator=(reference_wrapper<Fn> fn) noexcept
    {
        function(fn).swap(*this);
        return *this;
    }

    template <typename R, typename... Args>
    inline void function<R(Args...)>::swap(function& fn) noexcept
    {
        tempest::swap(_data, fn._data);
        tempest::swap(_manager_fn, fn._manager_fn);
        tempest::swap(_invoker, fn._invoker);
    }

    template <typename R, typename... Args>
    inline function<R(Args...)>::operator bool() const noexcept
    {
        return !empty();
    }

    template <typename R, typename... Args>
    inline R function<R(Args...)>::operator()(Args... args) const
    {
        return _invoker(_data, tempest::forward<Args>(args)...);
    }

    template <typename R, typename... Args>
    inline void swap(function<R(Args...)>& lhs, function<R(Args...)>& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    template <typename R, typename... Args>
    inline bool operator==(const function<R(Args...)>& lhs, nullptr_t) noexcept
    {
        return !static_cast<bool>(lhs);
    }

    template <typename R, typename... Args>
    inline bool operator==(nullptr_t, const function<R(Args...)>& rhs) noexcept
    {
        return !static_cast<bool>(rhs);
    }

    template <typename R, typename... Args>
    inline bool operator!=(const function<R(Args...)>& lhs, nullptr_t) noexcept
    {
        return static_cast<bool>(lhs);
    }

    template <typename R, typename... Args>
    inline bool operator!=(nullptr_t, const function<R(Args...)>& rhs) noexcept
    {
        return static_cast<bool>(rhs);
    }

    template <typename T = void>
    struct equal_to;

    template <typename T = void>
    struct not_equal_to;

    template <typename T = void>
    struct greater;

    template <typename T = void>
    struct less;

    template <typename T = void>
    struct greater_equal;

    template <typename T = void>
    struct less_equal;

    template <typename T>
    struct equal_to
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct not_equal_to
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct greater
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct less
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct greater_equal
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct less_equal
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    inline constexpr bool equal_to<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs == rhs;
    }

    template <typename T>
    inline constexpr bool not_equal_to<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs != rhs;
    }

    template <typename T>
    inline constexpr bool greater<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs > rhs;
    }

    template <typename T>
    inline constexpr bool less<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs < rhs;
    }

    template <typename T>
    inline constexpr bool greater_equal<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs >= rhs;
    }

    template <typename T>
    inline constexpr bool less_equal<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs <= rhs;
    }

    template <typename T = void>
    struct plus;

    template <typename T = void>
    struct minus;

    template <typename T = void>
    struct multiplies;

    template <typename T = void>
    struct divides;

    template <typename T = void>
    struct modulus;

    template <typename T = void>
    struct negate;

    template <typename T>
    struct plus
    {
        constexpr T operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct minus
    {
        constexpr T operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct multiplies
    {
        constexpr T operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct divides
    {
        constexpr T operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct modulus
    {
        constexpr T operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct negate
    {
        constexpr T operator()(const T& value) const;
    };

    template <typename T>
    inline constexpr T plus<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs + rhs;
    }

    template <typename T>
    inline constexpr T minus<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs - rhs;
    }

    template <typename T>
    inline constexpr T multiplies<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs * rhs;
    }

    template <typename T>
    inline constexpr T divides<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs / rhs;
    }

    template <typename T>
    inline constexpr T modulus<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs % rhs;
    }

    template <typename T>
    inline constexpr T negate<T>::operator()(const T& value) const
    {
        return -value;
    }

    template <typename T = void>
    struct logical_and;

    template <typename T = void>
    struct logical_or;

    template <typename T = void>
    struct logical_not;

    template <typename T>
    struct logical_and
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct logical_or
    {
        constexpr bool operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct logical_not
    {
        constexpr bool operator()(const T& value) const;
    };

    template <typename T>
    inline constexpr bool logical_and<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs && rhs;
    }

    template <typename T>
    inline constexpr bool logical_or<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs || rhs;
    }

    template <typename T>
    inline constexpr bool logical_not<T>::operator()(const T& value) const
    {
        return !value;
    }

    template <typename T = void>
    struct bit_and;

    template <typename T = void>
    struct bit_or;

    template <typename T = void>
    struct bit_xor;

    template <typename T = void>
    struct bit_not;

    template <typename T>
    struct bit_and
    {
        constexpr T operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct bit_or
    {
        constexpr T operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct bit_xor
    {
        constexpr T operator()(const T& lhs, const T& rhs) const;
    };

    template <typename T>
    struct bit_not
    {
        constexpr T operator()(const T& value) const;
    };

    template <typename T>
    inline constexpr T bit_and<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs & rhs;
    }

    template <typename T>
    inline constexpr T bit_or<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs | rhs;
    }

    template <typename T>
    inline constexpr T bit_xor<T>::operator()(const T& lhs, const T& rhs) const
    {
        return lhs ^ rhs;
    }

    template <typename T>
    inline constexpr T bit_not<T>::operator()(const T& value) const
    {
        return ~value;
    }

    template <typename T = void>
    struct not_fn;

    template <typename T>
    struct not_fn
    {
        constexpr bool operator()(const T& value) const;
    };

    template <typename T>
    inline constexpr bool not_fn<T>::operator()(const T& value) const
    {
        return !value;
    }

    template <typename T = void>
    struct identity;

    template <typename T>
    struct identity
    {
        constexpr T&& operator()(T&& t) const noexcept;
    };

    template <typename T>
    inline constexpr T&& identity<T>::operator()(T&& t) const noexcept
    {
        return tempest::forward<T>(t);
    }

    namespace detail
    {
        template <bool Nx, typename R, typename... Args>
        struct is_invocable_using
        {
            template <typename... Fn>
            static constexpr bool value() noexcept
            {
                if constexpr (Nx)
                {
                    return is_nothrow_invocable_r_v<R, Fn..., Args...>;
                }
                else
                {
                    return is_invocable_r_v<R, Fn..., Args...>;
                }
            }
        };

        struct function_ref_base
        {
            union storage {
                void* p = nullptr;
                void const* cp;
                void (*fp)();

                constexpr storage() noexcept = default;

                template <typename T>
                    requires is_object_v<T>
                constexpr explicit storage(T* t) noexcept : p{t}
                {
                }

                template <typename T>
                    requires is_object_v<T>
                constexpr explicit storage(T const* t) noexcept : cp{t}
                {
                }

                template <typename T>
                    requires is_function_v<T>
                constexpr explicit storage(T* t) noexcept : fp{reinterpret_cast<void (*)()>(t)}
                {
                }
            };

            template <typename T>
            static constexpr auto get(storage s)
            {
                if constexpr (is_const_v<T>)
                {
                    return static_cast<T*>(s.cp);
                }
                else if constexpr (is_object_v<T>)
                {
                    return static_cast<T*>(s.p);
                }
                else
                {
                    return reinterpret_cast<T*>(s.fp);
                }
            }
        };

        template <bool Cx, bool Nx, typename R, typename... Args>
        class function_ref_impl : function_ref_base
        {
          public:
            template <typename T>
            using cv = conditional_t<Cx, const T, T>;

            template <typename T>
            using cvref = cv<T>&;

            using type = R (*)(Args...);

            template <typename F>
                requires(is_invocable_using<Nx, R, Args...>::template value<F>() && is_function_v<F>)
            inline function_ref_impl(F* f) noexcept
                : _obj{f}, _callback{[](storage s, Args... args) noexcept(Nx) -> R {
                      if constexpr (is_void_v<R>)
                      {
                          tempest::invoke(get<F>(s), tempest::forward<Args>(args)...);
                      }
                      else
                      {
                          return tempest::invoke(get<F>(s), tempest::forward<Args>(args)...);
                      }
                  }}
            {
            }

            template <typename Fn>
                requires(is_invocable_using<Nx, R, Args...>::template value<Fn>() && !is_member_pointer_v<Fn> &&
                         !is_same_v<decay_t<Fn>, function_ref_impl>)
            inline function_ref_impl(Fn&& f) noexcept
                : _obj{tempest::addressof(f)}, _callback{[](storage s, Args... args) noexcept(Nx) -> R {
                      cvref<decay_t<Fn>> obj = *get<decay_t<Fn>>(s);
                      if constexpr (is_void_v<R>)
                      {
                          obj(tempest::forward<Args>(args)...);
                      }
                      else
                      {
                          return obj(tempest::forward<Args>(args)...);
                      }
                  }}
            {
            }

            template <auto f>
                requires(is_invocable_using<Nx, R, Args...>::template value<decltype(f)>())
            inline function_ref_impl(nontype_t<f>) noexcept
                : _callback{[]([[maybe_unused]] storage s, Args... args) noexcept(Nx) -> R {
                      return invoke_r<R>(f, tempest::forward<Args>(args)...);
                  }}
            {
            }

            template <auto f, typename T>
                requires(!is_rvalue_reference_v<T &&> &&
                         is_invocable_using<Nx, R, Args...>::template value<decltype(f), cvref<decay_t<T>>>())
            inline function_ref_impl(nontype_t<f>, T&& obj) noexcept
                : _obj{tempest::addressof(obj)}, _callback{[](storage s, Args... args) noexcept(Nx) -> R {
                      cvref<T> obj = *get<decay_t<T>>(s);
                      return invoke_r<R>(f, obj, tempest::forward<Args>(args)...);
                  }}
            {
            }

            template <auto f, typename T>
                requires(is_invocable_using<Nx, R, Args...>::template value<decltype(f),
                                                                            add_const_t<add_volatile_t<T>>>())
            inline function_ref_impl(nontype_t<f>, T* obj) noexcept
                : _obj{obj}, _callback{[](storage s, Args... args) noexcept(Nx) -> R {
                      auto obj = *get<cv<T>>(s);
                      return invoke_r<R>(f, obj, tempest::forward<Args>(args)...);
                  }}
            {
            }

            function_ref_impl(const function_ref_impl&) noexcept = default;

            function_ref_impl& operator=(const function_ref_impl&) noexcept = default;

            template <typename T>
            function_ref_impl& operator=(T) = delete;

            R operator()(Args... args) const noexcept(Nx)
            {
                return _callback(_obj, tempest::forward<Args>(args)...);
            }

          private:
            using fn_t = R (*)(storage, Args...);

            storage _obj;
            fn_t _callback;
        };
    } // namespace detail

    template <typename...>
    class function_ref;

    template <typename R, typename... Args>
    class function_ref<R(Args...)> : public detail::function_ref_impl<false, false, R, Args...>
    {
      public:
        using detail::function_ref_impl<false, false, R, Args...>::function_ref_impl;
    };

    template <typename R, typename... Args>
    class function_ref<R(Args...) noexcept> : public detail::function_ref_impl<false, true, R, Args...>
    {
      public:
        using detail::function_ref_impl<false, true, R, Args...>::function_ref_impl;
    };

    template <typename R, typename... Args>
    class function_ref<R(Args...) const> : public detail::function_ref_impl<true, false, R, Args...>
    {
      public:
        using detail::function_ref_impl<true, false, R, Args...>::function_ref_impl;
    };

    template <typename R, typename... Args>
    class function_ref<R(Args...) const noexcept> : public detail::function_ref_impl<true, true, R, Args...>
    {
      public:
        using detail::function_ref_impl<true, true, R, Args...>::function_ref_impl;
    };

    template <typename F>
    function_ref(F*) -> function_ref<F>;

    template <auto f>
        requires is_function_v<remove_pointer_t<decltype(f)>>
    function_ref(nontype_t<f>) -> function_ref<remove_pointer_t<decltype(f)>>;

    template <typename T>
    struct function_traits;

    // Specialization for function pointers
    template <typename Ret, typename... Args>
    struct function_traits<Ret (*)(Args...)>
    {
        using return_type = Ret;
        using argument_types = core::type_list<Args...>;
    };

    // Specialization for lambdas and other callables (leveraging operator())
    template <typename T>
    struct function_traits : function_traits<decltype(&T::operator())>
    {
    };

    // Specialization for member function pointers
    template <typename Ret, typename ClassType, typename... Args>
    struct function_traits<Ret (ClassType::*)(Args...) const>
    {
        using return_type = Ret;
        using argument_types = core::type_list<Args...>;
    };
} // namespace tempest

#endif // tempest_core_functional_hpp