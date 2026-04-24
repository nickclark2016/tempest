#include <array>
#include <fstream>
#include <iomanip>
#include <string>

#include <tempest/vk/aftermath/gpu_crash_tracker.hpp>

namespace tempest::rhi::vk::aftermath
{
    shader_database::shader_database()
    {
    }

    shader_database::~shader_database()
    {
    }

    bool shader_database::_read_file(const char* file_name, std::vector<uint8_t>& data)
    {
        std::ifstream file_stream(file_name, std::ios::in | std::ios::binary);
        if (!file_stream)
        {
            return false;
        }

        file_stream.seekg(0, std::ios::end);
        data.resize(file_stream.tellg());
        file_stream.seekg(0, std::ios::beg);
        file_stream.read(reinterpret_cast<char*>(data.data()), data.size());
        file_stream.close();

        return true;
    }

    void shader_database::_add_shader_binary(const char* shader_file_hash)
    {
        std::vector<uint8_t> data;
        if (!_read_file(shader_file_hash, data))
        {
            return;
        }

        const GFSDK_Aftermath_SpirvCode shader{
            .pData = data.data(),
            .size = uint32_t(data.size()),
        };

        GFSDK_Aftermath_ShaderBinaryHash shader_hash;
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetShaderHashSpirv(GFSDK_Aftermath_Version_API, &shader, &shader_hash));

        _shader_binaries[shader_hash].swap(data);
    }

    bool shader_database::find_shader_binary(const GFSDK_Aftermath_ShaderBinaryHash& shader_hash,
                                             std::vector<uint8_t>& shader) const
    {
        auto iter_shader = _shader_binaries.find(shader_hash);
        if (iter_shader == _shader_binaries.end())
        {
            return false;
        }

        shader = iter_shader->second;
        return true;
    }

    void shader_database::add_shader_binary(std::span<std::uint8_t> data)
    {
        const GFSDK_Aftermath_SpirvCode shader{
            .pData = data.data(),
            .size = static_cast<uint32_t>(data.size()),
        };

        GFSDK_Aftermath_ShaderBinaryHash shader_hash;
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetShaderHashSpirv(GFSDK_Aftermath_Version_API, &shader, &shader_hash));

        _shader_binaries[shader_hash] = std::vector<uint8_t>{data.begin(), data.end()};
    }

    gpu_crash_tracker::gpu_crash_tracker(const marker_map& marker_map)
        : _initialized(false), _mutex(), _shader_debug_info(), _shader_database(), _marker_map(marker_map)
    {
    }

    gpu_crash_tracker::~gpu_crash_tracker()
    {
        if (_initialized)
        {
            GFSDK_Aftermath_DisableGpuCrashDumps();
        }
    }

    void gpu_crash_tracker::initialize()
    {
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_EnableGpuCrashDumps(
            GFSDK_Aftermath_Version_API, GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
            GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks, // Let the Nsight Aftermath library cache
                                                                              // shader debug information.
            _gpu_crash_dump_callback,                                         // Register callback for GPU crash dumps.
            _shader_debug_info_callback, // Register callback for shader debug information.
            _crash_dump_desc_callback,   // Register callback for GPU crash dump description.
            _resolve_marker_callback,    // Register callback for resolving application-managed markers.
            this));                      // Set the gpu_crash_tracker object as user data for the above callbacks.

        _initialized = true;
    }

    // Handler for GPU crash dump callbacks from Nsight Aftermath
    void gpu_crash_tracker::_on_crash_dump(const void* gpu_crash_dump, const uint32_t gpu_crash_dump_size)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _write_gpu_crash_dump_to_file(gpu_crash_dump, gpu_crash_dump_size);
    }

    void gpu_crash_tracker::_on_shader_debug_info(const void* shader_debug_info, const uint32_t shader_debug_info_size)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier = {};
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetShaderDebugInfoIdentifier(
            GFSDK_Aftermath_Version_API, shader_debug_info, shader_debug_info_size, &identifier));

        std::vector<uint8_t> data((uint8_t*)shader_debug_info, (uint8_t*)shader_debug_info + shader_debug_info_size);
        _shader_debug_info[identifier].swap(data);

        _write_shader_debug_info_to_file(identifier, shader_debug_info, shader_debug_info_size);
    }

    void gpu_crash_tracker::_on_description(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description)
    {
        add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "Tempest Engine");
        add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "1.0");
    }

    // Handler for app-managed marker resolve callback
    void gpu_crash_tracker::_on_resolve_marker(const void* marker_data,
                                               [[maybe_unused]] const uint32_t marker_data_size,
                                               void** resolved_marker_data, uint32_t* resolved_marker_data_size)
    {
        for (auto& map : _marker_map)
        {
            const auto& found_marker = map.find((uint64_t)marker_data);
            if (found_marker != map.end())
            {
                const auto& found_marker_data = found_marker->second;

                *resolved_marker_data = (void*)found_marker_data.data();
                *resolved_marker_data_size = (uint32_t)found_marker_data.length();
                return;
            }
        }
    }

    // Helper for writing a GPU crash dump to a file
    void gpu_crash_tracker::_write_gpu_crash_dump_to_file(const void* gpu_crash_dump,
                                                          const uint32_t gpu_crash_dump_size)
    {
        GFSDK_Aftermath_GpuCrashDump_Decoder decoder = {};
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_CreateDecoder(GFSDK_Aftermath_Version_API, gpu_crash_dump,
                                                                         gpu_crash_dump_size, &decoder));

        GFSDK_Aftermath_GpuCrashDump_BaseInfo base_info = {};
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GetBaseInfo(decoder, &base_info));

        uint32_t application_name_length = 0;
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GetDescriptionSize(
            decoder, GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, &application_name_length));

        std::vector<char> application_name(application_name_length, '\0');

        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GetDescription(
            decoder, GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, uint32_t(application_name.size()),
            application_name.data()));

        static int count = 0;
        const std::string base_file_name =
            std::string(application_name.data()) + "-" + std::to_string(base_info.pid) + "-" + std::to_string(++count);

        const std::string crash_dump_file_name = base_file_name + ".nv-gpudmp";
        std::ofstream dump_file(crash_dump_file_name, std::ios::out | std::ios::binary);
        if (dump_file)
        {
            dump_file.write((const char*)gpu_crash_dump, gpu_crash_dump_size);
            dump_file.close();
        }

        uint32_t json_size = 0;
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GenerateJSON(
            decoder, GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO, GFSDK_Aftermath_GpuCrashDumpFormatterFlags_NONE,
            _shader_debug_info_lookup_callback, _shader_lookup_callback, _shader_source_debug_info_lookup_callback,
            this, &json_size));

        std::vector<char> json(json_size);
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_GetJSON(decoder, uint32_t(json.size()), json.data()));

        const std::string json_file_name = crash_dump_file_name + ".json";
        std::ofstream json_file(json_file_name, std::ios::out | std::ios::binary);
        if (json_file)
        {
            json_file.write(json.data(), json.size() - 1);
            json_file.close();
        }

        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder));
    }

    void gpu_crash_tracker::_write_shader_debug_info_to_file(GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier,
                                                             const void* shader_debug_info,
                                                             const uint32_t shader_debug_info_size)
    {
        const std::string file_path = "shader-" + std::to_string(identifier) + ".nvdbg";

        std::ofstream file(file_path, std::ios::out | std::ios::binary);
        if (file)
        {
            file.write((const char*)shader_debug_info, shader_debug_info_size);
        }
    }

    void gpu_crash_tracker::_on_shader_debug_info_lookup(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier,
                                                         PFN_GFSDK_Aftermath_SetData set_shader_debug_info) const
    {
        auto iter_debug_info = _shader_debug_info.find(identifier);
        if (iter_debug_info == _shader_debug_info.end())
        {
            return;
        }

        set_shader_debug_info(iter_debug_info->second.data(), uint32_t(iter_debug_info->second.size()));
    }

    void gpu_crash_tracker::_on_shader_lookup(const GFSDK_Aftermath_ShaderBinaryHash& shader_hash,
                                              PFN_GFSDK_Aftermath_SetData set_shader_binary) const
    {
        std::vector<uint8_t> shader_binary;
        if (!_shader_database.find_shader_binary(shader_hash, shader_binary))
        {
            return;
        }

        set_shader_binary(shader_binary.data(), uint32_t(shader_binary.size()));
    }

    void gpu_crash_tracker::_on_shader_source_debug_info_lookup(
        [[maybe_unused]] const GFSDK_Aftermath_ShaderDebugName& shader_debug_name,
        [[maybe_unused]] PFN_GFSDK_Aftermath_SetData set_shader_binary) const
    {
        // Handle split lookup
        return;
    }

    void gpu_crash_tracker::_gpu_crash_dump_callback(const void* gpu_crash_dump, const uint32_t crash_dump_size,
                                                     void* user_data)
    {
        gpu_crash_tracker* gpu_crash_tracker_ptr = reinterpret_cast<gpu_crash_tracker*>(user_data);
        gpu_crash_tracker_ptr->_on_crash_dump(gpu_crash_dump, crash_dump_size);
    }

    void gpu_crash_tracker::_shader_debug_info_callback(const void* shader_debug_info,
                                                        const uint32_t shader_debug_info_size, void* user_data)
    {
        gpu_crash_tracker* gpu_crash_tracker_ptr = reinterpret_cast<gpu_crash_tracker*>(user_data);
        gpu_crash_tracker_ptr->_on_shader_debug_info(shader_debug_info, shader_debug_info_size);
    }

    void gpu_crash_tracker::_crash_dump_desc_callback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_desc,
                                                      void* user_data)
    {
        gpu_crash_tracker* gpu_crash_tracker_ptr = reinterpret_cast<gpu_crash_tracker*>(user_data);
        gpu_crash_tracker_ptr->_on_description(add_desc);
    }

    void gpu_crash_tracker::_resolve_marker_callback(const void* marker_data, const uint32_t marker_data_size,
                                                     void* user_data, void** resolved_marker_data,
                                                     uint32_t* resolved_marker_data_size)
    {
        gpu_crash_tracker* gpu_crash_tracker_ptr = reinterpret_cast<gpu_crash_tracker*>(user_data);
        gpu_crash_tracker_ptr->_on_resolve_marker(marker_data, marker_data_size, resolved_marker_data,
                                                  resolved_marker_data_size);
    }

    void gpu_crash_tracker::_shader_debug_info_lookup_callback(
        const GFSDK_Aftermath_ShaderDebugInfoIdentifier* identifier, PFN_GFSDK_Aftermath_SetData set_shader_debug_info,
        void* user_data)
    {
        gpu_crash_tracker* gpu_crash_tracker_ptr = reinterpret_cast<gpu_crash_tracker*>(user_data);
        gpu_crash_tracker_ptr->_on_shader_debug_info_lookup(*identifier, set_shader_debug_info);
    }

    void gpu_crash_tracker::_shader_lookup_callback(const GFSDK_Aftermath_ShaderBinaryHash* shader_hash,
                                                    PFN_GFSDK_Aftermath_SetData set_shader_binary, void* user_data)
    {
        gpu_crash_tracker* gpu_crash_tracker_ptr = reinterpret_cast<gpu_crash_tracker*>(user_data);
        gpu_crash_tracker_ptr->_on_shader_lookup(*shader_hash, set_shader_binary);
    }

    void gpu_crash_tracker::_shader_source_debug_info_lookup_callback(
        const GFSDK_Aftermath_ShaderDebugName* shader_debug_name, PFN_GFSDK_Aftermath_SetData set_shader_binary,
        void* user_data)
    {
        gpu_crash_tracker* gpu_crash_tracker_ptr = reinterpret_cast<gpu_crash_tracker*>(user_data);
        gpu_crash_tracker_ptr->_on_shader_source_debug_info_lookup(*shader_debug_name, set_shader_binary);
    }
} // namespace tempest::rhi::vk::aftermath