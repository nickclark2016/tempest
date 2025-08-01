#ifndef tempest_editor_asset_explorer_hpp
#define tempest_editor_asset_explorer_hpp

#include <tempest/pane.hpp>

namespace tempest::editor
{
    /// <summary>
    /// Pane used to explore and manage assets in the editor.
    /// </summary>
    class asset_explorer final : public ui::pane
    {
      public:
        asset_explorer() = default;
        asset_explorer(const asset_explorer&) = delete;
        asset_explorer(asset_explorer&&) noexcept = delete;
        ~asset_explorer() override = default;
        asset_explorer& operator=(const asset_explorer&) = delete;
        asset_explorer& operator=(asset_explorer&&) noexcept = delete;

        void render() override;
        bool should_render() const noexcept override;
        bool should_close() const noexcept override;
        string_view name() const noexcept override;
    };
} // namespace tempest::editor

#endif // tempest_editor_asset_explorer_hpp
