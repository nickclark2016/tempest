#include <tempest/assets.hpp>
#include <tempest/assets/model_asset.hpp>
#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_system.hpp>
#include <tempest/transformations.hpp>
#include <tempest/window.hpp>

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <streambuf>

namespace
{
    inline constexpr std::size_t global_memory_allocator_size = 1024 * 1024 * 64;
}

void fixed_renderer()
{
    auto logger = tempest::logger::logger_factory::create({
        .prefix{"Sandbox"},
    });
    logger->info("Starting Sandbox Application.");

    auto global_allocator = tempest::core::heap_allocator(global_memory_allocator_size);

    auto window = tempest::graphics::window_factory::create({
        .title{"Tempest Sandbox"},
        .width{1280},
        .height{720},
    });

    auto renderer = tempest::graphics::render_system({.major{0}, .minor{0}, .patch{1}}, *window, global_allocator);

    auto last_time = std::chrono::high_resolution_clock::now();
    std::uint32_t fps_counter = 0;

    auto proj = tempest::math::perspective(16.0f / 9.0f, 100.0f, 0.01f, 1000.0f);
    auto modl = tempest::math::transform<float>({0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});

    auto asset_manager = tempest::assets::asset_manager();
    auto model = asset_manager.load<tempest::assets::model_asset>("assets/box.gltf");

    auto box_mesh_view = tempest::core::mesh_view{
        .vertices{model->vertices, model->vertex_count},
        .indices{model->indices, model->index_count},
        .has_tangents{false},
        .has_bitangents{false},
        .has_colors{false},
    };

    renderer.upload_mesh(box_mesh_view);

    while (!window->should_close())
    {
        tempest::input::poll();
        renderer.render();
        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> frame_time = current_time - last_time;

        ++fps_counter;

        if (frame_time.count() >= 1.0)
        {
            std::cout << fps_counter << " FPS" << std::endl;
            fps_counter = 0;
            last_time = current_time;
        }
    }

    logger->info("Exiting Sandbox Application.");
}