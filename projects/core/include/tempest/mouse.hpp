#ifndef tempest_core_mouse_hpp
#define tempest_core_mouse_hpp

#include <array>
#include <cstdint>
#include <type_traits>

namespace tempest::core
{
    enum class mouse_button : std::uint32_t
    {
        MB_0,
        MB_1,
        MB_2,
        MB_3,
        MB_4,
        MB_5,
        MB_6,
        MB_7,
        MB_8,
        LAST,
        LEFT = MB_1,
        RIGHT = MB_2,
        MIDDLE = MB_3,
    };

    enum class mouse_action : std::uint32_t
    {
        PRESS,
        RELEASE,
    };

    struct mouse_button_state
    {
        mouse_button button;
        mouse_action action;
    };

    class mouse
    {
      public:
        void set(mouse_button_state state)
        {
            _button_states[static_cast<std::underlying_type_t<mouse_button>>(state.button)] = state;
        }

        void set_position(float x, float y)
        {
            _dx = x - _x;
            _dy = y - _y;

            _x = x;
            _y = y;
        }

        void set_scroll(float x, float y)
        {
            _scroll_dx = x - _scroll_x;
            _scroll_dy = y - _scroll_y;

            _scroll_x = x;
            _scroll_y = y;
        }

        mouse_button_state get(mouse_button button) const
        {
            return _button_states[static_cast<std::underlying_type_t<mouse_button>>(button)];
        }

        float x() const noexcept
        {
            return _x;
        }

        float y() const noexcept
        {
            return _y;
        }

        float dx() const noexcept
        {
            return _dx;
        }

        float dy() const noexcept
        {
            return _dy;
        }

        float scroll_x() const noexcept
        {
            return _scroll_x;
        }

        float scroll_y() const noexcept
        {
            return _scroll_y;
        }

        float scroll_dx() const noexcept
        {
            return _scroll_dx;
        }

        float scroll_dy() const noexcept
        {
            return _scroll_dy;
        }

        bool is_pressed(mouse_button button) const
        {
            return _button_states[static_cast<std::underlying_type_t<mouse_button>>(button)].action ==
                   mouse_action::PRESS;
        }

        bool is_disabled() const noexcept
        {
            return _disabled;
        }

        void set_disabled(bool captured = true) noexcept
        {
            _disabled = captured;
        }

        void reset_mouse_deltas()
        {
            _dx = 0.0f;
            _dy = 0.0f;
            _scroll_dx = 0.0f;
            _scroll_dy = 0.0f;
        }

      private:
        std::array<mouse_button_state, static_cast<std::underlying_type_t<mouse_button>>(mouse_button::LAST)>
            _button_states;

        float _x = 0.0f;
        float _y = 0.0f;
        float _dx = 0.0f;
        float _dy = 0.0f;
        float _scroll_x = 0.0f;
        float _scroll_y = 0.0f;
        float _scroll_dx = 0.0f;
        float _scroll_dy = 0.0f;
        bool _disabled = false;
    };
} // namespace tempest::core

#endif // tempest_core_mouse_hpp