#ifndef tempest_graphics_pass_hpp
#define tempest_graphics_pass_hpp

#include <tempest/graphics_components.hpp>
#include <tempest/types.hpp>

namespace tempest::graphics::passes
{
    inline constexpr descriptor_binding_info scene_constant_buffer = {
        .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
        .binding_index = 0,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info vertex_pull_buffer_desc = {
        .type = descriptor_binding_type::STRUCTURED_BUFFER,
        .binding_index = 1,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info mesh_layout_buffer_desc = {
        .type = descriptor_binding_type::STRUCTURED_BUFFER,
        .binding_index = 2,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info object_buffer_desc = {
        .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
        .binding_index = 3,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info instance_buffer_desc = {
        .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
        .binding_index = 4,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info materials_buffer_desc = {
        .type = descriptor_binding_type::STRUCTURED_BUFFER,
        .binding_index = 5,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info oit_moment_image_desc = {
        .type = descriptor_binding_type::STORAGE_IMAGE,
        .binding_index = 6,
        .binding_count = 1, // 8 moments, 4 channels, 2 images
    };

    inline constexpr descriptor_binding_info oit_zero_moment_image_desc = {
        .type = descriptor_binding_type::STORAGE_IMAGE,
        .binding_index = 7,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info oit_accum_image_desc = {
        .type = descriptor_binding_type::SAMPLED_IMAGE,
        .binding_index = 8,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info oit_spinlock_buffer_desc = {
        .type = descriptor_binding_type::STRUCTURED_BUFFER,
        .binding_index = 8,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info skybox_image_desc = {
        .type = descriptor_binding_type::SAMPLED_IMAGE,
        .binding_index = 9,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info linear_sampler_desc = {
        .type = descriptor_binding_type::SAMPLER,
        .binding_index = 15,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info texture_array_desc = {
        .type = descriptor_binding_type::SAMPLED_IMAGE,
        .binding_index = 16,
        .binding_count = 512,
    };

    inline constexpr descriptor_binding_info light_parameter_desc = {
        .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
        .binding_index = 0,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info shadow_map_parameter_desc = {
        .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
        .binding_index = 1,
        .binding_count = 1,
    };

    inline constexpr descriptor_binding_info shadow_map_mt_desc = {
        .type = descriptor_binding_type::SAMPLED_IMAGE,
        .binding_index = 2,
        .binding_count = 1,
    };

    struct draw_command_state
    {
        buffer_resource_handle indirect_command_buffer;
        size_t first_indirect_command;
        size_t indirect_command_count;
        bool double_sided;
    };
} // namespace tempest::graphics::passes

#endif // tempest_graphics_pass_hpp
