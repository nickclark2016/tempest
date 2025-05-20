#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/input.hpp>
#include <tempest/material.hpp>
#include <tempest/pipelines/pbr_pipeline.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/rhi.hpp>
#include <tempest/texture.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/tuple.hpp>
#include <tempest/vertex.hpp>

namespace rhi = tempest::rhi;

int main()
{
    auto renderer = tempest::graphics::renderer();

    auto win = renderer.create_window({
        .width = 1920,
        .height = 1080,
        .name = "Window 1",
        .fullscreen = false,
    });

    auto entity_registry = tempest::ecs::archetype_registry();
    auto mesh_registry = tempest::core::mesh_registry();
    auto texture_registry = tempest::core::texture_registry();
    auto material_registry = tempest::core::material_registry();
    auto asset_database = tempest::assets::asset_database(&mesh_registry, &texture_registry, &material_registry);

    auto sponza_prefab =
        asset_database.import("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", entity_registry);
    auto sponza_instance = entity_registry.duplicate(sponza_prefab);

    auto pbr_pipeline =
        renderer.register_window<tempest::graphics::pbr_pipeline>(win.get(), 1920, 1080, entity_registry);

    pbr_pipeline->upload_objects_sync(renderer.get_device(), {&sponza_instance, 1}, mesh_registry, texture_registry,
                                      material_registry);

    auto camera = entity_registry.create();
    tempest::graphics::camera_component camera_data = {
        .aspect_ratio = 16.0f / 9.0f,
        .vertical_fov = 90.0f,
        .near_plane = 0.01f,
        .far_shadow_plane = 64.0f,
    };

    entity_registry.assign(camera, camera_data);
    entity_registry.assign(camera, tempest::ecs::transform_component::identity());

    while (true)
    {
        tempest::core::input::poll();

        bool windows_open = renderer.render();
        if (!windows_open)
        {
            break;
        }
    }

    return 0;
}