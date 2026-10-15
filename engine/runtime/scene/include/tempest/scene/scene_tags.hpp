#ifndef tempest_scene_scene_tags_hpp
#define tempest_scene_scene_tags_hpp

#include <tempest/api.hpp>
#include <tempest/traits.hpp>

namespace tempest::scene
{
    /// @brief Tag component marking an entity as inactive during time-sliced asynchronous loading and staging.
    struct TEMPEST_API scene_inactive_tag
    {
    };

    static_assert(ecs::component<scene_inactive_tag>);
} // namespace tempest::scene

namespace tempest::ecs
{
    template <>
    struct is_serializable<tempest::scene::scene_inactive_tag> : false_type
    {
    };

    template <>
    struct is_duplicatable<tempest::scene::scene_inactive_tag> : false_type
    {
    };

    template <>
    struct is_authorable<tempest::scene::scene_inactive_tag> : false_type
    {
    };
} // namespace tempest::ecs

#endif // tempest_scene_scene_tags_hpp
