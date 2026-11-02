#ifndef tempest_editor_core_menus_menu_item_hpp
#define tempest_editor_core_menus_menu_item_hpp

#include <tempest/cstring_view.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor
{
    class TEMPEST_EDITOR_API menu_item
    {
      public:
        menu_item(const menu_item&) = delete;
        menu_item(menu_item&&) noexcept = delete;
        virtual ~menu_item() = default;

        auto operator=(const menu_item&) -> menu_item& = delete;
        auto operator=(menu_item&&) noexcept -> menu_item& = delete;

        /// @brief Validates the menu item.
        /// @return True if the menu item is valid, false otherwise.
        [[nodiscard]] virtual auto validate() const noexcept -> bool;

        /// @brief Called when the menu item is pressed.
        virtual auto on_press() noexcept -> void = 0;

        /// @brief Returns whether the menu item is checked.
        /// @return True if the menu item is checked, false otherwise.
        [[nodiscard]] virtual auto is_checked() const noexcept -> bool
        {
            return false;
        }

        /// @brief Returns the shortcut of the menu item.
        /// @return The shortcut as a string view.
        [[nodiscard]] virtual auto get_shortcut() const noexcept -> string_view
        {
            return {};
        }

        /// @brief Returns the menu path of the menu item.
        /// @return The menu path as a span of strings.
        [[nodiscard]] auto get_menu_path() const noexcept -> span<const string>
        {
            return span{_menu_path.cbegin(), _menu_path.cend() - 1};
        }

        /// @brief Returns the name of the menu item.
        /// @return The name as a C string view.
        [[nodiscard]] auto get_menu_item_name() const noexcept -> cstring_view
        {
            const auto& name = _menu_path.back();
            return {name.c_str(), name.length()};
        }

      protected:
        /// @brief Constructs a menu item with the given menu path.
        /// @tparam Ts The types of the menu path elements. Each type must be convertible to a string.
        /// @param menu_path The menu path elements. Must have at least two elements: the root menu node and the menu
        /// item name.
        template <convertible_to<string>... Ts>
            requires(sizeof...(Ts) >= 2) // Element 0 - Root menu node, Last Element - Menu item
        menu_item(Ts... menu_path) : _menu_path(init_list, tempest::forward<Ts>(menu_path)...)
        {
        }

      private:
        vector<string> _menu_path;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_menus_menu_item_hpp
