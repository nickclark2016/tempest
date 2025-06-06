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

    //auto sponza_prefab =
    //    asset_database.import("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", entity_registry);
    //auto sponza_instance = entity_registry.duplicate(sponza_prefab);
    //entity_registry.get<tempest::ecs::transform_component>(sponza_instance).scale({0.125f});

    auto skybox_texture_prefab =
        asset_database.import("assets/polyhaven/hdri/autumn_field_puresky.exr", entity_registry);
    auto skybox_texture = entity_registry.get<tempest::core::texture_component>(skybox_texture_prefab).texture_id;

    auto chess_prefab = asset_database.import(
        "assets/glTF-Sample-Assets/Models/ABeautifulGame/glTF/ABeautifulGame.gltf", entity_registry);

    tempest::ecs::archetype_entity_hierarchy_view chess_view(entity_registry, chess_prefab);
    for (auto ent : chess_view)
    {
        auto material_comp = entity_registry.try_get<tempest::core::material_component>(ent);
        if (material_comp)
        {
            material_registry.update_material(
                material_comp->material_id, [](tempest::core::material& comp) {
                    if (auto mode = comp.get_string(tempest::core::material::alpha_mode_name);
                        mode && *mode == "TRANSMISSIVE")
                    {
                        comp.set_scalar(tempest::core::material::transmissive_factor_name, 0.75f);
                    }
                });
        }
    }

    auto chess_board = entity_registry.duplicate(chess_prefab);
    auto chess_board_tx = tempest::ecs::transform_component::identity();
    chess_board_tx.position({0.0f, 0.0f, 0.0f});
    entity_registry.assign_or_replace(chess_board, chess_board_tx);

    auto pbr_pipeline =
        renderer.register_window<tempest::graphics::pbr_pipeline>(win.get(), 1920, 1080, entity_registry);

    const auto renderables = tempest::array{
        chess_board,
    };

    pbr_pipeline->upload_objects_sync(renderer.get_device(), renderables, mesh_registry, texture_registry,
                                      material_registry);
    pbr_pipeline->set_skybox_texture(renderer.get_device(), skybox_texture, texture_registry);

    auto camera = entity_registry.create();
    tempest::graphics::camera_component camera_data = {
        .aspect_ratio = 16.0f / 9.0f,
        .vertical_fov = 90.0f,
        .near_plane = 0.01f,
        .far_shadow_plane = 64.0f,
    };

    entity_registry.assign(camera, camera_data);
    auto camera_tx = tempest::ecs::transform_component::identity();
    camera_tx.position({0.0f, 0.2f, -1.0f});
    camera_tx.rotation({0.0f, 0.0f, 0.0f});
    entity_registry.assign(camera, camera_tx);

    auto sun = entity_registry.create();
    tempest::graphics::directional_light_component sun_data = {
        .color = {1.0f, 1.0f, 1.0f},
        .intensity = 1.0f,
    };

    tempest::graphics::shadow_map_component sun_shadows = {
        .size = {2048, 2048},
        .cascade_count = 3,
    };

    entity_registry.assign_or_replace(sun, sun_shadows);
    entity_registry.assign_or_replace(sun, sun_data);
    entity_registry.name(sun, "Sun");

    tempest::ecs::transform_component sun_tx = tempest::ecs::transform_component::identity();
    sun_tx.rotation({tempest::math::as_radians(90.0f), 0.0f, 0.0f});

    entity_registry.assign_or_replace(sun, sun_tx);

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