#ifndef tempest_input_keyboard_hpp
#define tempest_input_keyboard_hpp

#include <tempest/array.hpp>
#include <tempest/concepts.hpp>
#include <tempest/enum.hpp>
#include <tempest/int.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest::core
{
    enum class key : uint32_t
    {
        unknown,
        space,
        apostrophe,
        comma,
        minus,
        period,
        slash,
        tw_0,
        tw_1,
        tw_2,
        tw_3,
        tw_4,
        tw_5,
        tw_6,
        tw_7,
        tw_8,
        tw_9,
        semicolon,
        equal,
        a,
        b,
        c,
        d,
        e,
        f,
        g,
        h,
        i,
        j,
        k,
        l,
        m,
        n,
        o,
        p,
        q,
        r,
        s,
        t,
        u,
        v,
        w,
        x,
        y,
        z,
        left_bracket,
        backslash,
        right_bracket,
        grave_accent,
        world_1,
        world_2,
        escape,
        enter,
        tab,
        backspace,
        insert,
        deletion,
        dpad_right,
        dpad_left,
        dpad_down,
        dpad_up,
        page_up,
        page_down,
        home,
        end,
        caps_lock,
        scroll_lock,
        num_lock,
        print_screen,
        pause,
        fn_1,
        fn_2,
        fn_3,
        fn_4,
        fn_5,
        fn_6,
        fn_7,
        fn_8,
        fn_9,
        fn_10,
        fn_11,
        fn_12,
        fn_13,
        fn_14,
        fn_15,
        fn_16,
        fn_17,
        fn_18,
        fn_19,
        fn_20,
        fn_21,
        fn_22,
        fn_23,
        fn_24,
        fn_25,
        kp_0,
        kp_1,
        kp_2,
        kp_3,
        kp_4,
        kp_5,
        kp_6,
        kp_7,
        kp_8,
        kp_9,
        kp_decimal,
        kp_divide,
        kp_multiply,
        kp_subtract,
        kp_add,
        kp_enter,
        kp_equal,
        left_shift,
        left_control,
        left_alt,
        left_super,
        right_shift,
        right_control,
        right_alt,
        right_super,
        menu,
        last_key
    };

    enum class key_action
    {
        press,
        release,
        repeat,
    };

    enum class key_modifier
    {
        none = 0x00,
        shift = 0x01,
        control = 0x02,
        alt = 0x04,
        super = 0x08,
        caps_lock = 0x10,
        num_lock = 0x20
    };

    struct key_state
    {
        key k = key::unknown;
        key_action action = key_action::release;
        enum_mask<key_modifier> modifiers = make_enum_mask(key_modifier::none);
    };

    inline constexpr bool test_modifier(key_state s, same_as<key_modifier> auto... modifiers) noexcept
    {
        auto set_mod = static_cast<underlying_type_t<key_modifier>>(s.modifiers);
        return (... && ((static_cast<underlying_type_t<key_modifier>>(modifiers) & set_mod) ==
                        static_cast<underlying_type_t<key_modifier>>(modifiers)));
    }

    class keyboard
    {
      public:
        void set(key_state state)
        {
            _key_states[static_cast<underlying_type_t<key>>(state.k)] = state;
        }

        key_state get(key k) const
        {
            return _key_states[to_underlying(k)];
        }

        bool is_key_down(key k) const noexcept
        {
            auto action = get(k).action;
            return action == key_action::press || action == key_action::repeat;
        }

      private:
        array<key_state, static_cast<underlying_type_t<key>>(key::last_key)> _key_states;
    };
} // namespace tempest::core

#endif // tempest_input_keyboard_hpp