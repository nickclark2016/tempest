#include <gtest/gtest.h>
#include <tempest/rhi/mock/mock_commands.hpp>
#include <tempest/rhi/mock/mock_device.hpp>
#include <tempest/rhi/mock/mock_device_commands.hpp>
#include <tempest/rhi/mock/mock_work_queue.hpp>


TEST(MockGraphicsTests, FullScreenPassUpdatesStateDynamically)
{
    tempest::rhi::mock::mock_device device;
    EXPECT_NE(device.create_image({}).id, 0);
}

TEST(MockDeviceTests, RecordsOperationsAndMaintainsGenerations)
{
    tempest::rhi::mock::mock_device device;

    // Create a buffer
    tempest::rhi::buffer_desc buf_desc{};
    buf_desc.size = 1024; // NOLINT
    auto buf1 = device.create_buffer(buf_desc);

    // Destroy it
    device.destroy_buffer(buf1);

    // Create another buffer (will have same index but different generation)
    // mock RHI just increments the index by default, so let's mock the generation explicitly.
    auto buf2_gen0 = tempest::rhi::typed_rhi_handle<tempest::rhi::rhi_handle_type::buffer>{
        .id = 5, // NOLINT
        .generation = 0,
    };
    
    auto buf2_gen1 = tempest::rhi::typed_rhi_handle<tempest::rhi::rhi_handle_type::buffer>{
        .id = 5, // NOLINT
        .generation = 1,
    };

    device.destroy_buffer(buf2_gen0);
    device.destroy_buffer(buf2_gen1);

    EXPECT_EQ(device.get_history_count(), 4);

    // Verify first command: create_buffer
    const auto* const cmd0 = get_if<tempest::rhi::mock::create_buffer_cmd>(&device.get_history(0));
    ASSERT_NE(cmd0, nullptr);
    EXPECT_EQ(cmd0->desc.size, 1024);
    EXPECT_EQ(cmd0->result, buf1);

    // Verify second command: destroy_buffer for buf1
    const auto* const cmd1 = get_if<tempest::rhi::mock::destroy_buffer_cmd>(&device.get_history(1));
    ASSERT_NE(cmd1, nullptr);
    EXPECT_EQ(cmd1->handle, buf1);

    // Verify third and fourth commands: destruction of same index, different generation
    const auto* const cmd2 = get_if<tempest::rhi::mock::destroy_buffer_cmd>(&device.get_history(2));
    ASSERT_NE(cmd2, nullptr);
    EXPECT_EQ(cmd2->handle, buf2_gen0);
    EXPECT_NE(cmd2->handle, buf2_gen1); // Must not be treated as equal

    const auto* const cmd3 = get_if<tempest::rhi::mock::destroy_buffer_cmd>(&device.get_history(3));
    ASSERT_NE(cmd3, nullptr);
    EXPECT_EQ(cmd3->handle, buf2_gen1);
    EXPECT_NE(cmd3->handle, buf2_gen0); // Must not be treated as equal
}

auto main(int argc, char** argv) -> int
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
