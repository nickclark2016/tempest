#ifndef tempest_editor_core_editor_window_hpp
#define tempest_editor_core_editor_window_hpp

#include <tempest/int.hpp>
#include <tempest/string_view.hpp>

namespace tempest::editor
{
    class TEMPEST_EDITOR_API editor_window
    {
      public:
        enum class dock_location : uint8_t
        {
            none,
            center,
            left,
            right,
            bottom,
        };

        editor_window(const editor_window&) = delete;
        editor_window(editor_window&&) noexcept = delete;
        virtual ~editor_window() = default;

        editor_window& operator=(const editor_window&) = delete;
        editor_window& operator=(editor_window&&) noexcept = delete;

        [[nodiscard]] virtual auto desired_initial_dock() const -> dock_location = 0;
        [[nodiscard]] virtual auto window_name() const -> string_view = 0;
        virtual auto draw() -> void = 0;

      protected:
        editor_window() = default;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_editor_window_hpp