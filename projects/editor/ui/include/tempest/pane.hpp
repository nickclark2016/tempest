#ifndef tempest_editor_ui_pane_hpp
#define tempest_editor_ui_pane_hpp

#include <tempest/string_view.hpp>

namespace tempest::editor::ui
{
    /// <summary>
    /// Base class for editor panes. Editor panes are windows or panels that are rendered as part of the editor UI.
    /// The editor context will manage the lifecycle of these panes, including rendering and closing them. Panes specify
    /// whether they should be rendered and whether they should close.
    /// </summary>
    class pane
    {
      public:
        pane() = default;
        pane(const pane&) = delete;
        pane(pane&&) noexcept = delete;
        virtual ~pane() = 0;

        pane& operator=(const pane&) = delete;
        pane& operator=(pane&&) noexcept = delete;

        /// <summary>
        /// Pure virtual function that renders the object.
        /// </summary>
        virtual void render() = 0;

        /// <summary>
        /// Determines whether rendering should occur.
        /// </summary>
        /// <returns>True if rendering should proceed; otherwise, false.</returns>
        virtual bool should_render() const noexcept = 0;

        /// <summary>
        /// Determines whether the object should be closed.
        /// </summary>
        /// <returns>true if the object should be closed; otherwise, false.</returns>
        virtual bool should_close() const noexcept = 0;

        /// <summary>
        /// Name of the pane. This is used for identification and docking purposes.
        /// </summary>
        /// <returns>Name of the pane.</returns>
        virtual string_view name() const noexcept = 0;
    };

    inline pane::~pane() = default;
} // namespace tempest::editor::ui

#endif // tempest_editor_ui_pane_hpp
