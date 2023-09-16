#include "binned_histogram_pass.hpp"

#include <tempest/logger.hpp>

#include <cassert>
#include <fstream>
#include <sstream>

namespace tempest::graphics
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::binned_histogram"}});

        inline std::vector<uint32_t> read_spirv(const std::string& path)
        {
            std::ostringstream buf;
            std::ifstream input(path.c_str(), std::ios::ate | std::ios::binary);
            assert(input);
            size_t file_size = (size_t)input.tellg();
            std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
            input.seekg(0);
            input.read(reinterpret_cast<char*>(buffer.data()), file_size);
            return buffer;
        }
    } // namespace

    bool tempest::graphics::binned_histogram_pass::initialize(gfx_device& device)
    {
        auto cx_spv = read_spirv("data/histogram/histogram.cx.spv");

        std::array<shader_stage, 5> stages = {{
            {
                .byte_code{reinterpret_cast<std::byte*>(cx_spv.data()), cx_spv.size() * sizeof(std::uint32_t)},
                .shader_type{VK_SHADER_STAGE_COMPUTE_BIT},
            },
        }};

        descriptor_set_layout_create_info::binding input_binding = {
            .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
            .start_binding{0},
            .binding_count{0},
            .name{"histogram_input"},
        };

        descriptor_set_layout_create_info::binding output_binding = {
            .type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
            .start_binding{1},
            .binding_count{0},
            .name{"histogram_output"},
        };

        descriptor_set_layout_create_info set0_layout_ci = {
            .bindings{input_binding, output_binding},
            .binding_count{2},
            .set_index{0},
            .name = {"histogram_set"},
        };

        compute_ios_layout = device.create_descriptor_set_layout(set0_layout_ci);

        push_constant_range bin_size{
            .offset{0},
            .range{4},
        };

        compute_shader = device.create_pipeline({
            .shaders{
                .stages{stages},
                .stage_count{1},
                .name{"histogram_compute"},
            },
            .desc_layouts{
                compute_ios_layout,
            },
            .active_desc_layouts{1},
            .push_constants{
                bin_size,
            },
            .active_push_constant_ranges{1},
        });

        input = device.create_buffer({
            .type{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
            .usage{resource_usage::DYNAMIC},
            .size{8192},
            .name{"histogram_input"},
        });

        output = device.create_buffer({
            .type{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
            .usage{resource_usage::DYNAMIC},
            .size{1024},
            .name{"histogram_output"},
        });

        compute_ios = device.create_descriptor_set(descriptor_set_builder("histogram_io")
                                                       .add_buffer(input, 0)
                                                       .add_buffer(output, 1)
                                                       .set_layout(compute_ios_layout));

        buffer_mapping mapping_desc = {
            .offset{0},
            .range{8192},
            .buffer{input},
        };

        std::uint8_t* inputs = (uint8_t*)device.map_buffer(mapping_desc);
        for (std::size_t i = 0; i < 8192; ++i)
        {
            inputs[i] = i % 8;
        }

        for (std::size_t i = 0; i < 1024; ++i)
        {
            inputs[i] += (i % 2);
        }

        std::vector<std::size_t> histogram(256, 0);
        for (std::size_t i = 0; i < 8192; ++i)
        {
            auto value = inputs[i];
            auto bin = value / 256;
            histogram[bin]++;
        }

        device.unmap_buffer(mapping_desc);

        mapping_desc = {
            .offset{0},
            .range{1024},
            .buffer{output},
        };

        std::memset(device.map_buffer(mapping_desc), 0, 1024);
        device.unmap_buffer(mapping_desc);
        return true;
    }

    void binned_histogram_pass::record(command_buffer& buf)
    {
        uint32_t bin_size = 256;
        descriptor_set_handle sets[] = {compute_ios};
        uint32_t offsets[] = {0, 0};
        buf.begin();
        buf.bind_pipeline(compute_shader)
            .bind_descriptor_set({sets}, offsets)
            .push_constants({0, 4}, &bin_size)
            .dispatch(2, 1, 1);
        buf.end();
    }

    void binned_histogram_pass::release(gfx_device& device)
    {
        buffer_mapping mapping_desc = {
            .offset{0},
            .range{1024},
            .buffer{output},
        };
        std::uint32_t* outputs = (uint32_t*)device.map_buffer(mapping_desc);

        auto view = std::span(outputs, 256);
        device.unmap_buffer(mapping_desc);

        device.release_descriptor_set_layout(compute_ios_layout);
        device.release_pipeline(compute_shader);
        device.release_buffer(input);
        device.release_buffer(output);
    }
} // namespace tempest::graphics