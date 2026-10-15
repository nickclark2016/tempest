#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/engine_types.hpp>
#include <tempest/event.hpp>
#include <tempest/format.hpp>
#include <tempest/guid.hpp>
#include <tempest/property_grid.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/render_target_asset.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor::tests
{
    namespace
    {
        template <typename T>
        auto to_byte_vector(const T& value) -> vector<byte>
        {
            auto result = vector<byte>{};
            result.resize(sizeof(T));
            tempest::memcpy(result.data(), &value, sizeof(T));
            return result;
        }
    } // namespace

    //==============================================================================
    // Camera Component Conditional Visibility Tests
    //==============================================================================

    /// @brief Verifies that camera_component::render_texture_id visibility predicate hides the field
    /// when the camera targets the viewport and shows it when targeting a render_texture.
    TEST(property_grid_asset_test, camera_component_render_texture_id_conditional_visibility)
    {
        // Setup: Build the reflection type descriptor for camera_component
        auto builder = ecs::type_builder<render_system::camera_component>{"tempest::render_system::camera_component"};
        ecs::reflect<render_system::camera_component>::describe(builder);
        const auto descriptor = builder.build();

        // Act: Look up the render_texture_id field descriptor
        const auto* field_descriptor = descriptor.find_field("render_texture_id");

        // Assert: Field descriptor exists, is of kind asset_ref, and has an active visibility predicate
        ASSERT_NE(field_descriptor, nullptr);
        EXPECT_EQ(field_descriptor->kind, ecs::field_kind::asset_ref);
        EXPECT_EQ(field_descriptor->asset_type_hash, normalized_type_hash_v<assets::render_target_asset>);
        ASSERT_NE(field_descriptor->is_visible, nullptr);

        // Setup: Construct camera targeting viewport
        auto camera_instance = render_system::camera_component{};
        camera_instance.target = render_system::camera_target_type::viewport;

        // Act & Assert: Field is hidden when target is viewport
        EXPECT_FALSE(field_descriptor->is_visible(&camera_instance));

        // Act: Switch camera target to render_texture
        camera_instance.target = render_system::camera_target_type::render_texture;

        // Assert: Field becomes visible
        EXPECT_TRUE(field_descriptor->is_visible(&camera_instance));
    }

    //==============================================================================
    // Asset Database Queries & Transient Filtering Tests
    //==============================================================================

    /// @brief Verifies that get_assets_by_type excludes transient assets by default and includes
    /// render_target_asset when querying for core::texture.
    TEST(property_grid_asset_test, asset_database_get_assets_by_type_transient_filtering)
    {
        // Setup: Create an asset database and register persistent and transient assets
        auto database = assets::asset_database{};

        const auto persistent_texture_guid = guid::generate_random_guid();
        const auto persistent_target_guid = guid::generate_random_guid();
        const auto transient_texture_guid = guid::generate_random_guid();

        // Register persistent assets under @builtin (marked non-transient)
        database.register_asset_with_guid(persistent_texture_guid, assets::asset_type_id::of<core::texture>(),
                                          "@builtin/textures/checkerboard");
        database.register_asset_with_guid(persistent_target_guid, assets::asset_type_id::of<assets::render_target_asset>(),
                                          "@builtin/textures/main_target");

        // Register transient asset without sidecar or builtin mount
        database.register_asset_with_guid(transient_texture_guid, assets::asset_type_id::of<core::texture>(),
                                          "assets/temp/runtime_preview.png");

        // Act: Query textures excluding transient assets (default behavior)
        const auto persistent_texture_items = database.get_assets_by_type(assets::asset_type_id::of<core::texture>(), false);

        // Assert: Persistent texture and render target are included, transient is excluded
        EXPECT_EQ(persistent_texture_items.size(), 2);

        auto found_checkerboard = false;
        auto found_target = false;
        auto found_transient = false;

        for (const auto& item : persistent_texture_items)
        {
            if (item.id == persistent_texture_guid)
            {
                found_checkerboard = true;
                EXPECT_EQ(item.display_name, "@builtin/textures/checkerboard");
            }
            if (item.id == persistent_target_guid)
            {
                found_target = true;
                EXPECT_EQ(item.display_name, "@builtin/textures/main_target");
            }
            if (item.id == transient_texture_guid)
            {
                found_transient = true;
            }
        }

        EXPECT_TRUE(found_checkerboard);
        EXPECT_TRUE(found_target);
        EXPECT_FALSE(found_transient);

        // Act: Query textures including transient assets
        const auto all_texture_items = database.get_assets_by_type(assets::asset_type_id::of<core::texture>(), true);

        // Assert: All three assets are returned
        EXPECT_EQ(all_texture_items.size(), 3);
    }

    /// @brief Verifies that get_asset_display_name handles empty GUIDs, registered assets, and unknown GUIDs correctly.
    TEST(property_grid_asset_test, asset_database_get_asset_display_name_resolution)
    {
        // Setup: Create asset database and register a known mesh asset
        auto database = assets::asset_database{};
        const auto box_mesh_guid = guid::generate_random_guid();
        database.register_asset_with_guid(box_mesh_guid, assets::asset_type_id::of<core::mesh>(), "@builtin/meshes/box");

        // Act & Assert: Empty GUID resolves to <None>
        EXPECT_EQ(database.get_asset_display_name(guid{}), "<None>");

        // Act & Assert: Registered asset resolves to its source path
        EXPECT_EQ(database.get_asset_display_name(box_mesh_guid), "@builtin/meshes/box");

        // Act & Assert: Unknown GUID formats as Missing (<guid>)
        const auto unknown_guid = guid::generate_random_guid();
        const auto expected_missing_string = format("Missing ({})", to_string(unknown_guid));
        EXPECT_EQ(database.get_asset_display_name(unknown_guid), expected_missing_string);
    }

    //==============================================================================
    // Property Grid & Command History Undo/Redo Tests
    //==============================================================================

    /// @brief Verifies that assigning an asset reference via set_field_command updates the component,
    /// records undo history in command_history, and restores the previous asset GUID upon undo.
    TEST(property_grid_asset_test, asset_assignment_undo_redo)
    {
        // Setup: Archetype registry, scene manager, command history, and entity with camera_component
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto history = command_history{};

        const auto initial_guid = guid::generate_random_guid();
        const auto new_guid = guid::generate_random_guid();

        const auto ent = reg.create();
        reg.assign(ent, render_system::camera_component{
                            .target = render_system::camera_target_type::render_texture,
                            .render_texture_id = initial_guid,
                        });

        const auto addr = compute_entity_address(reg, ent);
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
        };

        // Act: Dispatch set_field_command to update render_texture_id
        auto set_cmd = make_unique<set_field_command>(addr, "tempest::render_system::camera_component",
                                                      "render_texture_id", to_byte_vector(new_guid));
        history.execute(tempest::move(set_cmd), cmd_ctx);

        // Assert: New asset GUID is applied
        const auto* cam_after = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam_after, nullptr);
        EXPECT_EQ(cam_after->render_texture_id, new_guid);

        // Act: Undo the command
        ASSERT_TRUE(history.can_undo());
        history.undo(cmd_ctx);

        // Assert: Previous asset GUID is restored
        const auto* cam_undone = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam_undone, nullptr);
        EXPECT_EQ(cam_undone->render_texture_id, initial_guid);

        // Act: Redo the command
        ASSERT_TRUE(history.can_redo());
        history.redo(cmd_ctx);

        // Assert: New asset GUID is reapplied
        const auto* cam_redone = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam_redone, nullptr);
        EXPECT_EQ(cam_redone->render_texture_id, new_guid);
    }
} // namespace tempest::editor::tests
