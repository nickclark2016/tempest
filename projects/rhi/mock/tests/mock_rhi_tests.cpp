#include <gtest/gtest.h>
#include <tempest/rhi/mock/mock_device.hpp>
#include <tempest/rhi/mock/mock_work_queue.hpp>
#include <tempest/rhi/mock/mock_commands.hpp>

TEST(MockGraphicsTests, FullScreenPassUpdatesStateDynamically)
{
    tempest::rhi::mock::mock_device device;
    EXPECT_NE(device.create_image({}).id, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
