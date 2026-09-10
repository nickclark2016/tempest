#include <gtest/gtest.h>

#include <tempest/int.hpp>
#include <tempest/vector.hpp>

#include <tinyexr/tinyexr.h>

#include <cstdlib>

//=============================================================================
// Section: TinyEXR Miniz Roundtrip Tests
//=============================================================================

/// @brief Verifies that tinyexr compresses and decompresses floating-point image buffers
///        in memory using the pure-C miniz backend without zlib.
TEST(exr_test, roundtrip_memory_miniz)
{
    // 1. Setup - create a 4x4 RGBA floating point image
    constexpr auto width = 4;
    constexpr auto height = 4;
    constexpr auto components = 4;

    auto input_pixels = tempest::vector<float>{};
    input_pixels.reserve(width * height * components);

    for (auto y = 0; y < height; ++y)
    {
        for (auto x = 0; x < width; ++x)
        {
            input_pixels.push_back(static_cast<float>(x) / static_cast<float>(width));  // R
            input_pixels.push_back(static_cast<float>(y) / static_cast<float>(height)); // G
            input_pixels.push_back(0.75F);                                              // B
            input_pixels.push_back(1.0F);                                               // A
        }
    }

    // 2. Act - Save to compressed memory buffer (using miniz ZIP compression)
    auto* memory_buffer = static_cast<unsigned char*>(nullptr);
    const auto* err = static_cast<const char*>(nullptr);

    auto save_result = SaveEXRToMemory(input_pixels.data(), width, height, components,
                                       /*save_as_fp16=*/0, &memory_buffer, &err);

    ASSERT_GT(save_result, 0) << (((err != nullptr) ? err : "Failed to save EXR to memory") != nullptr);
    ASSERT_NE(memory_buffer, nullptr);

    // Decompress the EXR buffer back into floating-point pixels
    auto* loaded_pixels = static_cast<float*>(nullptr);
    auto loaded_width = 0;
    auto loaded_height = 0;

    auto load_result = LoadEXRFromMemory(&loaded_pixels, &loaded_width, &loaded_height, memory_buffer,
                                         static_cast<size_t>(save_result), &err);

    // 3. Assert - Verify dimensions and pixel components
    ASSERT_EQ(load_result, 0) << (((err != nullptr) ? err : "Failed to load EXR from memory") != nullptr);
    ASSERT_NE(loaded_pixels, nullptr);
    EXPECT_EQ(loaded_width, width);
    EXPECT_EQ(loaded_height, height);

    for (size_t i = 0; i < input_pixels.size(); ++i)
    {
        EXPECT_NEAR(loaded_pixels[i], input_pixels[i], 1e-5F);
    }

    // Cleanup
    ::free(memory_buffer);
    ::free(loaded_pixels);
}

/// @brief Verifies TinyEXR header initialization and cleanup without memory leaks.
TEST(exr_test, header_lifecycle)
{
    // 1. Setup
    auto header = EXRHeader{};
    InitEXRHeader(&header);

    // 2. Act
    header.num_channels = 3;
    header.channels = static_cast<EXRChannelInfo*>(::malloc(sizeof(EXRChannelInfo) * 3));
    header.pixel_types = static_cast<int*>(::malloc(sizeof(int) * 3));
    header.requested_pixel_types = static_cast<int*>(::malloc(sizeof(int) * 3));

    // 3. Assert
    EXPECT_EQ(header.compression_type, TINYEXR_COMPRESSIONTYPE_NONE);
    EXPECT_EQ(header.num_channels, 3);
    FreeEXRHeader(&header);
}
