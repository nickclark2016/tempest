#ifndef tempest_scene_scene_load_handle_hpp
#define tempest_scene_scene_load_handle_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/scene_types.hpp>

namespace tempest::scene
{
    class scene_manager;
    enum class scene_error : uint8_t;

    /// @brief Lifecycle state of an asynchronous scene loading operation.
    enum class scene_load_state : uint8_t
    {
        invalid = 0,
        decoding,
        resolving_assets,
        committing,
        ready,
        failed,
        cancelled,
    };

    /// @brief Generational handle referencing an active or completed asynchronous scene load operation.
    class TEMPEST_API scene_load_handle
    {
      public:
        static constexpr uint32_t invalid_index = ~uint32_t{0};
        static constexpr uint32_t default_generation = 0;
        static constexpr scene_manager* default_manager = nullptr;

        uint32_t index = invalid_index;
        uint32_t generation = default_generation;
        scene_manager* manager = default_manager;

        [[nodiscard]] auto is_valid() const -> bool;
        [[nodiscard]] auto state() const -> scene_load_state;
        [[nodiscard]] auto is_ready() const -> bool;
        [[nodiscard]] auto is_failed() const -> bool;
        [[nodiscard]] auto is_cancelled() const -> bool;
        [[nodiscard]] auto is_done() const -> bool;
        [[nodiscard]] auto progress() const -> float;
        [[nodiscard]] auto scene() const -> scene_handle;
        [[nodiscard]] auto error() const -> optional<scene_error>;
        auto cancel() -> void;

        constexpr auto operator==(const scene_load_handle& rhs) const noexcept -> bool = default;
        constexpr auto operator!=(const scene_load_handle& rhs) const noexcept -> bool = default;
    };
} // namespace tempest::scene

#endif // tempest_scene_scene_load_handle_hpp
