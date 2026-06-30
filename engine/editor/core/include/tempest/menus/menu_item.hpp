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

        menu_item& operator=(const menu_item&) = delete;
        menu_item& operator=(menu_item&&) noexcept = delete;

        virtual auto validate() const noexcept -> bool;
        virtual auto on_press() noexcept -> void = 0;

        auto get_menu_path() const noexcept -> span<const string>
        {
            return span{_menu_path.cbegin(), _menu_path.cend() - 1};
        }

        auto get_menu_item_name() const noexcept -> cstring_view
        {
            const auto& name = _menu_path.back();
            return {name.c_str(), name.length()};
        }

      protected:
        template <convertible_to<string>... Ts>
            requires(sizeof...(Ts) >= 2) // Element 0 - Root menu node, Last Element - Menu item
        menu_item(Ts... menu_path) : _menu_path(init_list, tempest::forward<Ts>(menu_path)...)
        {
        }

        vector<string> _menu_path;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_menus_menu_item_hpp
