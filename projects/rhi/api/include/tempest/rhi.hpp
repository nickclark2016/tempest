#ifndef tempest_rhi_rhi_hpp
#define tempest_rhi_rhi_hpp

#include <tempest/enum.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::rhi
{
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

    class device;
    class instance;
    class work_queue;
    class window_surface;

    struct rhi_device_description
    {
        uint32_t device_index;
        string device_name;
    };

    class instance
    {
      public:
        instance(const instance&) = delete;
        instance(instance&&) noexcept = delete;
        virtual ~instance() = default;

        instance& operator=(const instance&) = delete;
        instance& operator=(instance&&) noexcept = delete;

        virtual vector<rhi_device_description> get_devices() const noexcept = 0;
        virtual device& acquire_device(uint32_t device_index) noexcept = 0;

      protected:
        instance() = default;
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

        typed_rhi_handle<rhi_handle_type::render_surface> render_surface =
            typed_rhi_handle<rhi_handle_type::render_surface>::null_handle;
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

    class device
    {
      public:
        device(const device&) = delete;
        device(device&&) noexcept = delete;
        virtual ~device() = default;

        device& operator=(const device&) = delete;
        device& operator=(device&&) noexcept = delete;

        virtual typed_rhi_handle<rhi_handle_type::buffer> create_buffer(const buffer_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::image> create_image(const image_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::fence> create_fence(const fence_info& info) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::semaphore> create_semaphore(const semaphore_info& info) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::render_surface> create_render_surface(
            const render_surface_desc& desc) noexcept = 0;

        virtual void destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept = 0;
        virtual void destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept = 0;
        virtual void destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept = 0;
        virtual void destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept = 0;
        virtual void destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept = 0;

        virtual work_queue& get_primary_work_queue() noexcept = 0;
        virtual work_queue& get_dedicated_transfer_queue() noexcept = 0;
        virtual work_queue& get_dedicated_compute_queue() noexcept = 0;

        virtual render_surface_info query_render_surface_info(const window_surface& window) noexcept = 0;
        virtual span<const typed_rhi_handle<rhi_handle_type::image>> get_render_surfaces(
            typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept = 0;
        virtual expected<swapchain_image_acquire_info_result, swapchain_error_code> acquire_next_image(
            typed_rhi_handle<rhi_handle_type::render_surface> swapchain,
            typed_rhi_handle<rhi_handle_type::semaphore> signal_sem =
                typed_rhi_handle<rhi_handle_type::semaphore>::null_handle,
            typed_rhi_handle<rhi_handle_type::fence> signal_fence =
                typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept = 0;

        virtual bool is_signaled(typed_rhi_handle<rhi_handle_type::fence> fence) const noexcept = 0;
        virtual bool reset(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept = 0;
        virtual bool wait(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept = 0;

        virtual void start_frame() = 0;
        virtual void end_frame() = 0;

        virtual uint32_t frames_in_flight() const noexcept = 0;

      protected:
        device() = default;
    };

    class work_queue
    {
      public:
        struct semaphore_submit_info
        {
            typed_rhi_handle<rhi_handle_type::semaphore> semaphore;
            uint64_t value;
            enum_mask<pipeline_stage> stages;
        };

        struct submit_info
        {
            vector<typed_rhi_handle<rhi_handle_type::command_list>> command_lists;
            vector<semaphore_submit_info> wait_semaphores;
            vector<semaphore_submit_info> signal_semaphores;
        };

        struct swapchain_image_present_info
        {
            typed_rhi_handle<rhi_handle_type::render_surface> render_surface;
            uint32_t image_index;
        };

        struct present_info
        {
            vector<swapchain_image_present_info> swapchain_images;
            vector<typed_rhi_handle<rhi_handle_type::semaphore>> wait_semaphores;
        };

        enum class present_result
        {
            SUCCESS,
            SUBOPTIMAL,
            OUT_OF_DATE,
            ERROR,
        };

        work_queue(const work_queue&) = delete;
        work_queue(work_queue&&) noexcept = delete;
        virtual ~work_queue() = default;

        work_queue& operator=(const work_queue&) = delete;
        work_queue& operator=(work_queue&&) noexcept = delete;

        virtual typed_rhi_handle<rhi_handle_type::command_list> get_next_command_list(
            uint32_t frame_in_flight) noexcept = 0;

        virtual bool submit(span<const submit_info> infos,
                            typed_rhi_handle<rhi_handle_type::fence> fence =
                                typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept = 0;
        virtual bool present(const present_info& info) noexcept = 0;

        // Commands
        struct image_barrier
        {
            typed_rhi_handle<rhi_handle_type::image> image;
            image_layout old_layout;
            image_layout new_layout;
            enum_mask<pipeline_stage> src_stages;
            enum_mask<memory_access> src_access;
            enum_mask<pipeline_stage> dst_stages;
            enum_mask<memory_access> dst_access;
            work_queue* src_queue = nullptr;
            work_queue* dst_queue = nullptr;
        };

        virtual void begin_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                        bool one_time_submit) noexcept = 0;
        virtual void end_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept = 0;
        virtual void transition_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                      span<const image_barrier> image_barriers) noexcept = 0;
        virtual void clear_color_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                       typed_rhi_handle<rhi_handle_type::image> image, image_layout layout, float r, float g, float b,
                                       float a) noexcept = 0;

      protected:
        work_queue() = default;
    };

    class window_surface
    {
      public:
        window_surface(const window_surface&) = delete;
        window_surface(window_surface&&) noexcept = delete;
        virtual ~window_surface() = default;

        window_surface& operator=(const window_surface&) = delete;
        window_surface& operator=(window_surface&&) noexcept = delete;

        virtual uint32_t width() const noexcept = 0;
        virtual uint32_t height() const noexcept = 0;
        virtual string name() const noexcept = 0;
        virtual bool should_close() const noexcept = 0;

      protected:
        window_surface() = default;
    };
} // namespace tempest::rhi

namespace tempest::rhi::vk
{
    extern unique_ptr<rhi::instance> create_instance() noexcept;
    extern unique_ptr<rhi::window_surface> create_window_surface(const rhi::window_surface_desc& desc) noexcept;
} // namespace tempest::rhi::vk

#endif // tempest_rhi_rhi_hpp
