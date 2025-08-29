#ifndef tempest_editor_viewport_hpp
#define tempest_editor_viewport_hpp

#include <tempest/pane.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vec2.hpp>

namespace tempest::editor
{
    class viewport final : public ui::pane
    {
      public:
        viewport(graphics::render_pipeline* pipeline) : _pipeline{pipeline}
        {
        }

        viewport(const viewport&) = delete;
        viewport(viewport&&) noexcept = delete;
        ~viewport() override = default;

        viewport& operator=(const viewport&) = delete;
        viewport& operator=(viewport&&) noexcept = delete;

        void render() override;
        bool should_render() const noexcept override;
        bool should_close() const noexcept override;
        string_view name() const noexcept override;

        math::vec2<uint32_t> window_size() const noexcept
        {
            return _win_size;
        }

        bool visible() const noexcept
        {
            return _visible;
        }

      private:
        math::vec2<uint32_t> _win_size{};
        bool _visible{};
        graphics::render_pipeline* _pipeline{};
    };
} // namespace tempest::editor

#endif // tempest_editor_viewport_hpp
