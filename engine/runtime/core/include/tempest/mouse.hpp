#ifndef tempest_core_mouse_hpp
#define tempest_core_mouse_hpp

#include <tempest/array.hpp>
#include <tempest/int.hpp>
#include <tempest/type_traits.hpp>

namespace tempest::core
{
    enum class mouse_button : uint32_t
    {
        mb_1,
        mb_2,
        mb_3,
        mb_4,
        mb_5,
        mb_6,
        mb_7,
        mb_8,
        last,
        left = mb_1,
        right = mb_2,
        middle = mb_3,
    };

    enum class mouse_action : uint32_t
    {
        press,
        release,
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
            _button_states[to_underlying(state.button)] = state;
        }

        void set_position(float x, float y)
        {
            if (_x != -1.0f && _y != -1.0f) [[likely]]
            {
                _dx = x - _x;
                _dy = y - _y;
            }

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
            return _button_states[static_cast<underlying_type_t<mouse_button>>(button)];
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
            return _button_states[to_underlying(button)].action == mouse_action::press;
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
        array<mouse_button_state, to_underlying(mouse_button::last)> _button_states{};

        float _x = -1.0f;
        float _y = -1.0f;
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