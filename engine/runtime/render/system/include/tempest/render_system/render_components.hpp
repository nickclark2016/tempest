#ifndef tempest_render_system_render_components_hpp
#define tempest_render_system_render_components_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/mat4.hpp>
#include <tempest/material.hpp>
#include <tempest/render_target_asset.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>
#include <tempest/vertex.hpp>

namespace tempest::render_system
{
    struct TEMPEST_API mesh_layout
    {
        uint64_t vertex_buffer_address{0};
        uint32_t mesh_start_offset{0};
        uint32_t positions_offset{0};
        uint32_t interleave_offset{0};
        uint32_t interleave_stride{0};
        uint32_t uvs_offset{0};
        uint32_t normals_offset{0};
        uint32_t tangents_offset{numeric_limits<uint32_t>::max()};
        uint32_t color_offset{numeric_limits<uint32_t>::max()};
        uint32_t index_offset{0};
        uint32_t index_count{0};
    };

    enum class alpha_behavior : uint32_t
    {
        opaque = 0,
        mask = 1,
        transparent = 2,
        transmissive = 3,
    };

    enum class material_type : uint32_t
    {
        opaque = 0,
        mask = 1,
        blend = 2,
        transmissive = 3,
    };

    struct TEMPEST_API material_payload
    {
        static constexpr int32_t invalid_texture_id = -1;

        math::vec4<float> base_color_factor{1.0F, 1.0F, 1.0F, 1.0F};
        math::vec4<float> emissive_factor{0.0F, 0.0F, 0.0F, 0.0F};
        math::vec4<float> attenuation_color{1.0F, 1.0F, 1.0F, 1.0F};

        float normal_scale{1.0F};
        float metallic_factor{0.0F};
        float roughness_factor{1.0F};
        float alpha_cutoff{0.5F};
        float reflectance{0.5F};
        float transmission_factor{0.0F};
        float thickness_factor{0.0F};
        float attenuation_distance{0.0F};

        int32_t base_color_texture_id{invalid_texture_id};
        int32_t normal_texture_id{invalid_texture_id};
        int32_t metallic_roughness_texture_id{invalid_texture_id};
        int32_t emissive_texture_id{invalid_texture_id};
        int32_t occlusion_texture_id{invalid_texture_id};
        int32_t transmission_texture_id{invalid_texture_id};
        int32_t thickness_texture_id{invalid_texture_id};
        material_type type{material_type::opaque};
    };

    struct TEMPEST_API object_payload
    {
        math::mat4<float> model{1.0F};
        math::mat4<float> inv_transpose_model{1.0F};
        uint64_t mesh_gpu_address{0};     // 64-bit BDA pointer to mesh_layout
        uint64_t material_gpu_address{0}; // 64-bit BDA pointer to material_payload
        uint64_t parent_gpu_address{0};   // 64-bit BDA pointer to parent object_payload (0 if root)
        uint32_t self_id{numeric_limits<uint32_t>::max()};
        uint32_t padding{0};
    };

    struct TEMPEST_API indirect_command
    {
        uint32_t vertex_count{0};
        uint32_t instance_count{0};
        uint32_t first_vertex{0};
        uint32_t first_instance{0};
    };

    struct TEMPEST_API indexed_indirect_command
    {
        uint32_t index_count{0};
        uint32_t instance_count{0};
        uint32_t first_index{0};
        int32_t vertex_offset{0};
        uint32_t first_instance{0};
    };

    enum class camera_target_type : uint32_t
    {
        viewport = 0,
        render_texture = 1,
    };

    struct TEMPEST_API camera_component
    {
        static constexpr float default_aspect_ratio = 16.0F / 9.0F;
        static constexpr float default_vertical_fov = 1.04719755F; // ~60 degrees
        static constexpr float default_near_plane = 0.1F;
        static constexpr float default_far_plane = 1000.0F;
        static constexpr camera_target_type default_target = camera_target_type::viewport;
        static constexpr guid default_render_texture_id = {};
        static constexpr bool default_is_active = true;

        float aspect_ratio = default_aspect_ratio;
        float vertical_fov = default_vertical_fov;
        float near_plane = default_near_plane;
        float far_plane = default_far_plane;
        camera_target_type target = default_target;
        guid render_texture_id = default_render_texture_id;
        bool is_active = default_is_active;
    };

    struct TEMPEST_API directional_light_component
    {
        static constexpr math::vec3<float> default_color = {1.0F, 1.0F, 1.0F};
        static constexpr float default_intensity = 1.0F;

        math::vec3<float> color = default_color;
        float intensity = default_intensity;
    };

    struct TEMPEST_API point_light_component
    {
        static constexpr math::vec3<float> default_color = {1.0F, 1.0F, 1.0F};
        static constexpr float default_intensity = 1.0F;
        static constexpr float default_range = 10.0F;

        math::vec3<float> color = default_color;
        float intensity = default_intensity;
        float range = default_range;
    };

    struct TEMPEST_API light_payload
    {
        math::vec4<float> color_intensity{1.0F, 1.0F, 1.0F, 1.0F};   // rgb = color, w = intensity
        math::vec4<float> position_falloff{0.0F, 0.0F, 0.0F, 10.0F}; // xyz = world position, w = range/radius
        math::vec4<float> direction_angle{0.0F, -1.0F, 0.0F, 0.0F};  // xyz = forward direction, w = spot angle
        uint32_t type{1};                                            // 0 = Directional, 1 = Point, 2 = Spot
        uint32_t enabled{1};
        uint32_t padding[2]{0, 0};
    };

    enum class shadow_debug_mode : uint32_t
    {
        none = 0,
        cascades = 1,
        shadow_factor = 2,
        cascade_and_shadow = 3,
        scene_cascade_tint = 4,
    };

    struct TEMPEST_API shadow_caster_component
    {
        static constexpr uint32_t default_resolution = 2048;
        static constexpr uint32_t default_num_cascades = 4;
        static constexpr float default_split_lambda = 0.5F;
        static constexpr float default_max_shadow_distance = 200.0F;
        static constexpr float default_normal_bias = 0.02F;
        static constexpr float default_depth_bias = 0.005F;
        static constexpr uint32_t default_priority = 0; // Lower value = higher priority (0 = directional sun)
        static constexpr shadow_debug_mode default_debug_mode = shadow_debug_mode::none;

        uint32_t resolution = default_resolution;
        uint32_t num_cascades = default_num_cascades;
        float split_lambda = default_split_lambda;
        float max_shadow_distance = default_max_shadow_distance;
        float normal_bias = default_normal_bias;
        float depth_bias = default_depth_bias;
        uint32_t priority = default_priority;
        shadow_debug_mode debug_mode = default_debug_mode;
    };
} // namespace tempest::render_system

#include <tempest/reflection.hpp>

namespace tempest::ecs
{
    inline constexpr enum_entry camera_target_entries[] = {
        {.name = "Viewport", .value = static_cast<int64_t>(render_system::camera_target_type::viewport)},
        {.name = "Render Texture", .value = static_cast<int64_t>(render_system::camera_target_type::render_texture)},
    };

    template <>
    struct reflect<render_system::camera_component>
    {
        static void describe(type_builder<render_system::camera_component>& builder)
        {
            builder.version(1)
                .field<&render_system::camera_component::aspect_ratio>("aspect_ratio", field_flags::none,
                    field_ui_hints{.min_value = 0.01, .max_value = 100.0, .drag_speed = 0.05F})
                .field<&render_system::camera_component::vertical_fov>("vertical_fov", field_flags::none,
                    field_ui_hints{.min_value = 0.01, .max_value = 3.14, .drag_speed = 0.01F})
                .field<&render_system::camera_component::near_plane>("near_plane", field_flags::none,
                    field_ui_hints{.min_value = 0.001, .max_value = 1000.0, .drag_speed = 0.05F})
                .field<&render_system::camera_component::far_plane>("far_plane", field_flags::none,
                    field_ui_hints{.min_value = 0.1, .max_value = 100000.0, .drag_speed = 1.0F})
                .field<&render_system::camera_component::target>("target", camera_target_entries)
                .asset_ref<assets::render_target_asset, &render_system::camera_component::render_texture_id>("render_texture_id")
                .visible_when([](const void* component_ptr) -> bool {
                    const auto& camera_component = *static_cast<const render_system::camera_component*>(component_ptr);
                    return camera_component.target == render_system::camera_target_type::render_texture;
                })
                .field<&render_system::camera_component::is_active>("is_active");
        }
    };

    template <>
    struct reflect<render_system::directional_light_component>
    {
        static void describe(type_builder<render_system::directional_light_component>& builder)
        {
            builder.version(1)
                .field<&render_system::directional_light_component::color>("color", field_flags::none,
                    field_ui_hints{.widget = ui_widget_type::color_picker})
                .field<&render_system::directional_light_component::intensity>("intensity", field_flags::none,
                    field_ui_hints{.min_value = 0.0, .max_value = 100000.0, .drag_speed = 0.5F});
        }
    };

    template <>
    struct reflect<render_system::point_light_component>
    {
        static void describe(type_builder<render_system::point_light_component>& builder)
        {
            builder.version(1)
                .field<&render_system::point_light_component::color>("color", field_flags::none,
                    field_ui_hints{.widget = ui_widget_type::color_picker})
                .field<&render_system::point_light_component::intensity>("intensity", field_flags::none,
                    field_ui_hints{.min_value = 0.0, .max_value = 100000.0, .drag_speed = 0.5F})
                .field<&render_system::point_light_component::range>("range", field_flags::none,
                    field_ui_hints{.min_value = 0.1, .max_value = 10000.0, .drag_speed = 0.5F});
        }
    };

    inline constexpr enum_entry shadow_debug_entries[] = {
        {.name = "None", .value = static_cast<int64_t>(render_system::shadow_debug_mode::none)},
        {.name = "Cascades", .value = static_cast<int64_t>(render_system::shadow_debug_mode::cascades)},
        {.name = "Shadow Factor", .value = static_cast<int64_t>(render_system::shadow_debug_mode::shadow_factor)},
        {.name = "Cascade & Shadow", .value = static_cast<int64_t>(render_system::shadow_debug_mode::cascade_and_shadow)},
        {.name = "Scene Cascade Tint", .value = static_cast<int64_t>(render_system::shadow_debug_mode::scene_cascade_tint)},
    };

    template <>
    struct reflect<render_system::shadow_caster_component>
    {
        static void describe(type_builder<render_system::shadow_caster_component>& builder)
        {
            builder.version(1)
                .field<&render_system::shadow_caster_component::resolution>("resolution")
                .field<&render_system::shadow_caster_component::num_cascades>("num_cascades", field_flags::none,
                    field_ui_hints{.min_value = static_cast<uint64_t>(1), .max_value = static_cast<uint64_t>(4), .drag_speed = 1.0F})
                .field<&render_system::shadow_caster_component::split_lambda>("split_lambda", field_flags::none,
                    field_ui_hints{.min_value = 0.0, .max_value = 1.0, .drag_speed = 0.01F})
                .field<&render_system::shadow_caster_component::max_shadow_distance>("max_shadow_distance", field_flags::none,
                    field_ui_hints{.min_value = 1.0, .max_value = 5000.0, .drag_speed = 1.0F})
                .field<&render_system::shadow_caster_component::normal_bias>("normal_bias", field_flags::none,
                    field_ui_hints{.min_value = 0.0, .max_value = 1.0, .drag_speed = 0.001F})
                .field<&render_system::shadow_caster_component::depth_bias>("depth_bias", field_flags::none,
                    field_ui_hints{.min_value = 0.0, .max_value = 1.0, .drag_speed = 0.001F})
                .field<&render_system::shadow_caster_component::priority>("priority")
                .field<&render_system::shadow_caster_component::debug_mode>("debug_mode", shadow_debug_entries);
        }
    };

    template <>
    struct reflect<core::mesh_component>
    {
        static void describe(type_builder<core::mesh_component>& builder)
        {
            builder.version(1)
                .asset_ref<core::mesh, &core::mesh_component::mesh_id>("mesh_id");
        }
    };

    template <>
    struct reflect<core::material_component>
    {
        static void describe(type_builder<core::material_component>& builder)
        {
            builder.version(1)
                .asset_ref<core::material, &core::material_component::material_id>("material_id");
        }
    };
} // namespace tempest::ecs

#endif // tempest_render_system_render_components_hpp
