#ifndef tempest_input_keyboard_hpp
#define tempest_input_keyboard_hpp

#include <array>
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace tempest::core
{
    enum class key : std::uint32_t
    {
        UNKNOWN,
        SPACE,
        APOSTROPHE,
        COMMA,
        MINUS,
        PERIOD,
        SLASH,
        TW_0,
        TW_1,
        TW_2,
        TW_3,
        TW_4,
        TW_5,
        TW_6,
        TW_7,
        TW_8,
        TW_9,
        SEMICOLON,
        EQUAL,
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        LEFT_BRACKET,
        BACKSLASH,
        RIGHT_BRACKET,
        GRAVE_ACCENT,
        WORLD_1,
        WORLD_2,
        ESCAPE,
        ENTER,
        TAB,
        BACKSPACE,
        INSERT,
        DELETE,
        DPAD_RIGHT,
        DPAD_LEFT,
        DPAD_DOWN,
        DPAD_UP,
        PAGE_UP,
        PAGE_DOWN,
        HOME,
        END,
        CAPS_LOCK,
        NUM_LOCK,
        PRINT_SCREEN,
        PAUSE,
        FUNCTION_1,
        FUNCTION_2,
        FUNCTION_3,
        FUNCTION_4,
        FUNCTION_5,
        FUNCTION_6,
        FUNCTION_7,
        FUNCTION_8,
        FUNCTION_9,
        FUNCTION_10,
        FUNCTION_11,
        FUNCTION_12,
        FUNCTION_13,
        FUNCTION_14,
        FUNCTION_15,
        FUNCTION_16,
        FUNCTION_17,
        FUNCTION_18,
        FUNCTION_19,
        FUNCTION_20,
        FUNCTION_21,
        FUNCTION_22,
        FUNCTION_23,
        FUNCTION_24,
        FUNCTION_25,
        KP_0,
        KP_1,
        KP_2,
        KP_3,
        KP_4,
        KP_5,
        KP_6,
        KP_7,
        KP_8,
        KP_9,
        KP_DECIMAL,
        KP_DIVIDE,
        KP_MULTIPLY,
        KP_SUBTRACT,
        KP_ADD,
        KP_ENTER,
        KP_EQUAL,
        LEFT_SHIFT,
        LEFT_CONTROL,
        LEFT_ALT,
        LEFT_SUPER,
        RIGHT_SHIFT,
        RIGHT_CONTROL,
        RIGHT_ALT,
        RIGHT_SUPER,
        MENU,
        LAST_KEY
    };

    enum class key_action
    {
        PRESS,
        RELEASE,
        REPEAT,
    };

    enum class key_modifier
    {
        NONE = 0x00,
        SHIFT = 0x01,
        CONTROL = 0x02,
        ALT = 0x04,
        SUPER = 0x08,
        CAPS_LOCK = 0x10,
        NUM_LOCK = 0x20
    };

    struct key_state
    {
        key k = key::UNKNOWN;
        key_action action = key_action::RELEASE;
        key_modifier modifiers = key_modifier::NONE;
    };

    inline constexpr bool test_modifier(key_state s, std::same_as<key_modifier> auto... modifiers) noexcept
    {
        auto set_mod = static_cast<std::underlying_type_t<key_modifier>>(s.modifiers);
        return (... && ((static_cast<std::underlying_type_t<key_modifier>>(modifiers) & set_mod) ==
                        static_cast<std::underlying_type_t<key_modifier>>(modifiers)));
    }

    class keyboard
    {
      public:
        void set(key_state state)
        {
            _key_states[static_cast<std::underlying_type_t<key>>(state.k)] = state;
        }

        key_state get(key k) const
        {
            return _key_states[static_cast<std::underlying_type_t<key>>(k)];
        }

        bool is_key_down(key k) const noexcept
        {
            auto action = get(k).action;
            return action == key_action::PRESS || action == key_action::REPEAT;
        }

      private:
        std::array<key_state, static_cast<std::underlying_type_t<key>>(key::LAST_KEY)> _key_states;
    };
} // namespace tempest::core

#endif // tempest_input_keyboard_hpp