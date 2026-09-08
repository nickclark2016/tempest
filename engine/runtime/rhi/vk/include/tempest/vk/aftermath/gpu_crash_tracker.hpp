#ifndef tempest_rhi_vk_aftermath_gpu_crash_tracker_hpp
#define tempest_rhi_vk_aftermath_gpu_crash_tracker_hpp

#include <tempest/array.hpp>
#include <tempest/flat_map.hpp>
#include <tempest/int.hpp>
#include <tempest/mutex.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

#include "helpers.hpp"

namespace tempest::rhi::vk::aftermath
{
    class gpu_crash_tracker;
    class shader_database;

    class shader_database
    {
      public:
        shader_database();
        ~shader_database();

        bool find_shader_binary(const GFSDK_Aftermath_ShaderBinaryHash& shader_hash,
                                tempest::vector<uint8_t>& shader) const;

        void add_shader_binary(tempest::span<uint8_t> data);

      private:
        void _add_shader_binary(const char* shader_file_path);

        static bool _read_file(const char* filename, tempest::vector<uint8_t>& data);

        tempest::flat_map<GFSDK_Aftermath_ShaderBinaryHash, tempest::vector<uint8_t>> _shader_binaries;
    };

    class gpu_crash_tracker
    {
      public:
        static constexpr size_t marker_frame_history = 4;
        using marker_map = tempest::array<tempest::flat_map<uint64_t, tempest::string>, marker_frame_history>;

        gpu_crash_tracker(const marker_map& markers);
        ~gpu_crash_tracker();

        void initialize();

      private:
        void _on_crash_dump(const void* gpu_crash_dump, const uint32_t gpu_crash_dump_size);
        void _on_shader_debug_info(const void* shader_debug_info, const uint32_t shader_debug_info_size);
        void _on_description(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description);
        void _on_resolve_marker(const void* marker_data, const uint32_t marker_data_size, void** resolved_marker_data,
                                uint32_t* resolved_marker_data_size);

        void _write_gpu_crash_dump_to_file(const void* gpu_crash_dump, const uint32_t gpu_crash_dump_size);
        void _write_shader_debug_info_to_file(GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier,
                                              const void* shader_debug_info, const uint32_t shader_debug_info_size);

        void _on_shader_debug_info_lookup(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier,
                                          PFN_GFSDK_Aftermath_SetData set_shader_debug_info) const;
        void _on_shader_lookup(const GFSDK_Aftermath_ShaderBinaryHash& shader_hash,
                               PFN_GFSDK_Aftermath_SetData set_shader_binary) const;
        void _on_shader_source_debug_info_lookup(const GFSDK_Aftermath_ShaderDebugName& shader_debug_name,
                                                 PFN_GFSDK_Aftermath_SetData set_shader_binary) const;

        static void _gpu_crash_dump_callback(const void* gpu_crash_dump, const uint32_t crash_dump_size,
                                             void* user_data);
        static void _shader_debug_info_callback(const void* shader_debug_info, const uint32_t shader_debug_info_size,
                                                void* user_data);
        static void _crash_dump_desc_callback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_desc, void* user_data);
        static void _resolve_marker_callback(const void* marker_data, const uint32_t marker_data_size, void* user_data,
                                             void** resolved_marker_data, uint32_t* resolved_marker_data_size);
        static void _shader_debug_info_lookup_callback(const GFSDK_Aftermath_ShaderDebugInfoIdentifier* identifier,
                                                       PFN_GFSDK_Aftermath_SetData set_shader_debug_info,
                                                       void* user_data);
        static void _shader_lookup_callback(const GFSDK_Aftermath_ShaderBinaryHash* shader_hash,
                                            PFN_GFSDK_Aftermath_SetData set_shader_binary, void* user_data);
        static void _shader_source_debug_info_lookup_callback(const GFSDK_Aftermath_ShaderDebugName* shader_debug_name,
                                                              PFN_GFSDK_Aftermath_SetData set_shader_binary,
                                                              void* user_data);

        bool _initialized;
        mutable tempest::mutex _mutex;
        tempest::flat_map<GFSDK_Aftermath_ShaderDebugInfoIdentifier, tempest::vector<uint8_t>> _shader_debug_info;
        shader_database _shader_database;
        const marker_map& _marker_map;
    };

} // namespace tempest::rhi::vk::aftermath

#endif // !tempest_rhi_vk_aftermath_gpu_crash_tracker_hpp
