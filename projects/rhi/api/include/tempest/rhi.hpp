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
    };

    template <rhi_handle_type T>
    inline const typed_rhi_handle<T> typed_rhi_handle<T>::null_handle = {
        .id = numeric_limits<uint32_t>::max(),
        .generation = numeric_limits<uint32_t>::max(),
    };

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

    struct buffer_desc
    {
        size_t size;
        memory_location location;
        enum_mask<buffer_usage> usage;
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
        int initial_value;
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
    };

    struct swapchain_image_acquire_info_result
    {
        typed_rhi_handle<rhi_handle_type::image> image;
        uint32_t image_index;
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
            typed_rhi_handle<rhi_handle_type::semaphore> signal_sem,
            typed_rhi_handle<rhi_handle_type::fence> signal_fence) noexcept = 0;

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

        virtual typed_rhi_handle<rhi_handle_type::command_list> get_command_list() noexcept = 0;

        virtual bool submit(span<const submit_info> infos,
                            typed_rhi_handle<rhi_handle_type::fence> fence =
                                typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept = 0;
        virtual bool present(const present_info& info) noexcept = 0;

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
