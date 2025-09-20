#ifndef tempest_editor_asset_explorer_hpp
#define tempest_editor_asset_explorer_hpp

#include <tempest/filesystem.hpp>
#include <tempest/optional.hpp>
#include <tempest/pane.hpp>

namespace tempest::editor
{
    /// <summary>
    /// Pane used to explore and manage assets in the editor.
    /// </summary>
    class asset_explorer final : public ui::pane
    {
      public:
        asset_explorer();
        asset_explorer(const asset_explorer&) = delete;
        asset_explorer(asset_explorer&&) noexcept = delete;
        ~asset_explorer() override = default;
        asset_explorer& operator=(const asset_explorer&) = delete;
        asset_explorer& operator=(asset_explorer&&) noexcept = delete;

        void render() override;
        bool should_render() const noexcept override;
        bool should_close() const noexcept override;
        string_view name() const noexcept override;

      private:
        filesystem::path _root_path;    // Root path of the asset directory
        filesystem::path _current_path; // Current path being explored
        optional<filesystem::path> _selected_path; // Currently selected file or directory (if any)
    };
} // namespace tempest::editor

#endif // tempest_editor_asset_explorer_hpp
