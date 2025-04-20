#ifndef tempest_rhi_rhi_types_hpp
#define tempest_rhi_rhi_types_hpp

#include <tempest/enum.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/string.hpp>

namespace tempest::rhi
{
    class device;
    class instance;
    class work_queue;
    class window_surface;
    class rhi_resource_tracker;

    enum class rhi_handle_type
    {
        buffer,
        image,
        sampler,
        graphics_pipeline,
        compute_pipeline,
        command_list,
        fence,
        semaphore,
        render_surface,
        descriptor_set,
        descriptor_set_layout,
    };

    template <rhi_handle_type T>
    struct typed_rhi_handle
    {
        static constexpr rhi_handle_type type = T;

        uint32_t id;
        uint32_t generation;

        static const typed_rhi_handle<T> null_handle;

        constexpr bool operator==(const typed_rhi_handle& other) const noexcept;
        constexpr bool operator!=(const typed_rhi_handle& other) const noexcept;

        constexpr operator bool() const noexcept;
        constexpr bool is_valid() const noexcept;
    };

    template <rhi_handle_type T>
    inline const typed_rhi_handle<T> typed_rhi_handle<T>::null_handle = {
        .id = numeric_limits<uint32_t>::max(),
        .generation = numeric_limits<uint32_t>::max(),
    };

    template <rhi_handle_type T>
    inline constexpr bool typed_rhi_handle<T>::operator==(const typed_rhi_handle& other) const noexcept
    {
        return id == other.id && generation == other.generation;
    }

    template <rhi_handle_type T>
    inline constexpr bool typed_rhi_handle<T>::operator!=(const typed_rhi_handle& other) const noexcept
    {
        return !(*this == other);
    }

    template <rhi_handle_type T>
    inline constexpr typed_rhi_handle<T>::operator bool() const noexcept
    {
        return is_valid();
    }

    template <rhi_handle_type T>
    inline constexpr bool typed_rhi_handle<T>::is_valid() const noexcept
    {
        return id != numeric_limits<uint32_t>::max() && generation != numeric_limits<uint32_t>::max();
    }

    struct rhi_device_description
    {
        uint32_t device_index;
        string device_name;
    };

    enum class memory_location
    {
        DEVICE,
        HOST,
        AUTO,
    };

    enum class image_format
    {
        // Single channel, color format
        R8_UNORM,
        R8_SNORM,
        R16_UNORM,
        R16_SNORM,
        R16_FLOAT,
        R32_FLOAT,
        // Two channels, color format
        RG8_UNORM,
        RG8_SNORM,
        RG16_UNORM,
        RG16_SNORM,
        RG16_FLOAT,
        RG32_FLOAT,
        // Four channels, color format
        RGBA8_UNORM,
        RGBA8_SNORM,
        RGBA8_SRGB,
        BGRA8_SRGB,
        RGBA16_UNORM,
        RGBA16_SNORM,
        RGBA16_FLOAT,
        RGBA32_FLOAT,
        // Depth-Stencil formats
        S8_UINT,
        D16_UNORM,
        D24_UNORM,
        D32_FLOAT,
        D16_UNORM_S8_UINT,
        D24_UNORM_S8_UINT,
        D32_FLOAT_S8_UINT,
        // HDR Formats
        A2BGR10_UNORM_PACK32,
    };

    enum class image_layout
    {
        UNDEFINED,
        GENERAL,
        COLOR_ATTACHMENT,
        DEPTH_STENCIL_READ_WRITE,
        DEPTH_STENCIL_READ_ONLY,
        SHADER_READ_ONLY,
        TRANSFER_SRC,
        TRANSFER_DST,
        DEPTH,
        DEPTH_READ_ONLY,
        STENCIL,
        STENCIL_READ_ONLY,
        PRESENT,
    };

    enum class buffer_usage
    {
        VERTEX = 0x00000001,
        INDEX = 0x00000002,
        INDIRECT = 0x00000004,
        CONSTANT = 0x00000008,
        STORAGE = 0x00000010,
        TRANSFER_SRC = 0x00000020,
        TRANSFER_DST = 0x00000040,
    };

    enum class image_usage
    {
        COLOR_ATTACHMENT = 0x00000001,
        DEPTH_ATTACHMENT = 0x00000002,
        STENCIL_ATTACHMENT = 0x00000004,
        STORAGE = 0x00000008,
        SAMPLED = 0x00000010,
        TRANSFER_SRC = 0x00000020,
        TRANSFER_DST = 0x00000040,
    };

    enum class image_type
    {
        IMAGE_1D,
        IMAGE_2D,
        IMAGE_3D,
        IMAGE_CUBE,
        IMAGE_1D_ARRAY,
        IMAGE_2D_ARRAY,
        IMAGE_CUBE_ARRAY,
    };

    enum class image_tiling_type
    {
        OPTIMAL,
        LINEAR,
    };

    enum class image_sample_count
    {
        SAMPLE_COUNT_1 = 0x00000001,
        SAMPLE_COUNT_2 = 0x00000002,
        SAMPLE_COUNT_4 = 0x00000004,
        SAMPLE_COUNT_8 = 0x00000008,
        SAMPLE_COUNT_16 = 0x00000010,
        SAMPLE_COUNT_32 = 0x00000020,
        SAMPLE_COUNT_64 = 0x00000040,
    };

    enum class host_access_type
    {
        NONE,
        COHERENT,
        INCOHERENT,
    };

    enum class host_access_pattern
    {
        NONE,
        RANDOM,
        SEQUENTIAL,
    };

    struct buffer_desc
    {
        size_t size;
        memory_location location;
        enum_mask<buffer_usage> usage;
        host_access_type access_type;
        host_access_pattern access_pattern;
        string name;
    };

    struct image_desc
    {
        image_format format;
        image_type type;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t array_layers;
        uint32_t mip_levels;
        image_sample_count sample_count;
        image_tiling_type tiling;
        memory_location location;
        enum_mask<image_usage> usage;
        string name;
    };

    struct fence_info
    {
        bool signaled;
    };

    enum class semaphore_type
    {
        BINARY,
        TIMELINE,
    };

    struct semaphore_info
    {
        semaphore_type type;
        uint32_t initial_value;
    };

    enum class operation_type
    {
        COMPUTE,
        GRAPHICS,
        TRANSFER,
    };

    enum class color_space
    {
        ADOBE_RGB_LINEAR,
        ADOBE_RGB_NONLINEAR,
        BT709_LINEAR,
        BT709_NONLINEAR,
        BT2020_LINEAR,
        DCI_P3_NONLINEAR,
        DISPLAY_NATIVE_AMD,
        DISPLAY_P3_LINEAR,
        DISPLAY_P3_NONLINEAR,
        EXTENDED_SRGB_LINEAR,
        EXTENDED_SRGB_NONLINEAR,
        HDR10_HLG,
        HDR10_ST2084,
        PASS_THROUGH,
        SRGB_NONLINEAR,
    };

    enum class present_mode
    {
        IMMEDIATE,
        MAILBOX,
        FIFO,
        FIFO_RELAXED,
    };

    struct render_surface_format
    {
        color_space space;
        image_format format;
    };

    struct render_surface_info
    {
        vector<present_mode> present_modes;
        vector<render_surface_format> formats;
        uint32_t min_image_count;
        uint32_t max_image_count;
        uint32_t min_image_width;
        uint32_t min_image_height;
        uint32_t max_image_width;
        uint32_t max_image_height;
        uint32_t max_image_layers;
        enum_mask<image_usage> supported_usages;
    };

    struct render_surface_desc
    {
        window_surface* window;
        uint32_t min_image_count;
        render_surface_format format;
        present_mode present_mode;
        uint32_t width;
        uint32_t height;
        uint32_t layers;
    };

    struct window_surface_desc
    {
        uint32_t width;
        uint32_t height;
        string name;
        bool fullscreen;
    };

    enum class swapchain_error_code
    {
        OUT_OF_DATE,
        SUBOPTIMAL,
        FAILURE,
        INVALID_SWAPCHAIN_ARGUMENT,
    };

    struct swapchain_image_acquire_info_result
    {
        typed_rhi_handle<rhi_handle_type::fence> frame_complete_fence;
        typed_rhi_handle<rhi_handle_type::semaphore> acquire_sem;
        typed_rhi_handle<rhi_handle_type::semaphore> render_complete_sem;
        typed_rhi_handle<rhi_handle_type::image> image;
        uint32_t image_index;
    };

    enum class pipeline_stage
    {
        NONE = 0x00000,
        TOP = 0x00001,
        BOTTOM = 0x00002,
        INDIRECT_COMMAND = 0x00004,
        // Graphics commmands
        VERTEX_ATTRIBUTE_INPUT = 0x00008,
        INDEX_INPUT = 0x00010,
        VERTEX_SHADER = 0x00020,
        TESSELLATION_CONTROL_SHADER = 0x00040,
        TESSELLATION_EVALUATION_SHADER = 0x00080,
        GEOMETRY_SHADER = 0x00100,
        FRAGMENT_SHADER = 0x00200,
        EARLY_FRAGMENT_TESTS = 0x00400,
        LATE_FRAGMENT_TESTS = 0x00800,
        COLOR_ATTACHMENT_OUTPUT = 0x01000,
        // Compute commands
        COMPUTE_SHADER = 0x02000,
        // Transfer commands
        COPY = 0x04000,
        RESOLVE = 0x08000,
        BLIT = 0x10000,
        CLEAR = 0x20000,
        ALL_TRANSFER = 0x40000,
    };

    enum class memory_access
    {
        NONE = 0x00000,
        INDIRECT_COMMAND_READ = 0x00001,
        INDEX_READ = 0x00002,
        VERTEX_ATTRIBUTE_READ = 0x00004,
        CONSTANT_BUFFER_READ = 0x00008,
        SHADER_READ = 0x00010,
        SHADER_WRITE = 0x00020,
        COLOR_ATTACHMENT_READ = 0x00040,
        COLOR_ATTACHMENT_WRITE = 0x00080,
        DEPTH_STENCIL_ATTACHMENT_READ = 0x00100,
        DEPTH_STENCIL_ATTACHMENT_WRITE = 0x00200,
        TRANSFER_READ = 0x00400,
        TRANSFER_WRITE = 0x00800,
        HOST_READ = 0x01000,
        HOST_WRITE = 0x02000,
        MEMORY_READ = 0x04000,
        MEMORY_WRITE = 0x08000,
        SHADER_SAMPLED_READ = 0x10000,
        SHADER_STORAGE_READ = 0x20000,
        SHADER_STORAGE_WRITE = 0x40000,
    };

    enum class shader_stage
    {
        NONE = 0x00000,
        VERTEX = 0x00001,
        TESSELLATION_CONTROL = 0x00002,
        TESSELLATION_EVALUATION = 0x00004,
        GEOMETRY = 0x00008,
        FRAGMENT = 0x00010,
        COMPUTE = 0x00020,
    };

    enum class descriptor_type
    {
        SAMPLER,
        COMBINED_IMAGE_SAMPLER,
        SAMPLED_IMAGE,
        STORAGE_IMAGE,
        UNIFORM_BUFFER,
        STORAGE_BUFFER,
        UNIFORM_TEXEL_BUFFER,
        STORAGE_TEXEL_BUFFER,
        INPUT_ATTACHMENT,
    };

    enum class descriptor_binding_flags
    {
        NONE = 0x00,
        PARTIALLY_BOUND = 0x01,
        VARIABLE_LENGTH = 0x02,
    };

    struct descriptor_binding_layout
    {
        uint32_t binding_index;
        descriptor_type type;
        uint32_t count;
        enum_mask<shader_stage> stages;
        enum_mask<descriptor_binding_flags> flags = make_enum_mask(descriptor_binding_flags::NONE);

        bool operator==(const descriptor_binding_layout& other) const noexcept;
        bool operator!=(const descriptor_binding_layout& other) const noexcept;
    };

    inline bool descriptor_binding_layout::operator==(const descriptor_binding_layout& other) const noexcept
    {
        return binding_index == other.binding_index && type == other.type && count == other.count &&
               stages == other.stages && flags == other.flags;
    }

    inline bool descriptor_binding_layout::operator!=(const descriptor_binding_layout& other) const noexcept
    {
        return !(*this == other);
    }

    class descriptor_resource_binding
    {
      public:
        struct image_binding
        {
            typed_rhi_handle<rhi_handle_type::image> image;
            typed_rhi_handle<rhi_handle_type::sampler> sampler;
            image_layout layout = image_layout::UNDEFINED;

            bool operator==(const image_binding& other) const noexcept;
            bool operator!=(const image_binding& other) const noexcept;
        };

        struct buffer_binding
        {
            typed_rhi_handle<rhi_handle_type::buffer> buffer;
            size_t offset = 0;
            size_t size = numeric_limits<size_t>::max();

            bool operator==(const buffer_binding& other) const noexcept;
            bool operator!=(const buffer_binding& other) const noexcept;
        };

        void bind_image(uint32_t set, uint32_t binding, rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image,
                        rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> sampler,
                        rhi::image_layout layout = rhi::image_layout::UNDEFINED) noexcept;

        void bind_buffer(uint32_t set, uint32_t binding, rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer,
                         size_t offset = 0, size_t size = numeric_limits<size_t>::max()) noexcept;

        const flat_unordered_map<uint64_t, image_binding>& get_image_bindings() const noexcept;
        const flat_unordered_map<uint64_t, buffer_binding>& get_buffer_bindings() const noexcept;

        bool operator==(const descriptor_resource_binding& other) const noexcept;
        bool operator!=(const descriptor_resource_binding& other) const noexcept;

        static uint64_t make_key(uint32_t set, uint32_t binding) noexcept;
        static void split_key(uint64_t key, uint32_t& set, uint32_t& binding) noexcept;

      private:
        flat_unordered_map<uint64_t, image_binding> _image_bindings;
        flat_unordered_map<uint64_t, buffer_binding> _buffer_bindings;
    };
} // namespace tempest::rhi

namespace tempest
{
    template <rhi::rhi_handle_type T>
    struct hash<rhi::typed_rhi_handle<T>>
    {
        size_t operator()(const rhi::typed_rhi_handle<T>& handle) const noexcept
        {
            return ::tempest::hash_combine(handle.id, handle.generation, to_underlying(T));
        }
    };
} // namespace tempest

#endif // tempest_rhi_rhi_types_hpp
