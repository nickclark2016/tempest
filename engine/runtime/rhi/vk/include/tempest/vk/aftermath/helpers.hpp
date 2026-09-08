#ifndef tempest_rhi_vk_aftermath_helpers_hpp
#define tempest_rhi_vk_aftermath_helpers_hpp

#include <tempest/format.hpp>
#include <tempest/string.hpp>

#include <GFSDK_Aftermath.h>
#include <GFSDK_Aftermath_GpuCrashDump.h>
#include <GFSDK_Aftermath_GpuCrashDumpDecoding.h>
#include <vulkan/vulkan.hpp>

//*********************************************************
// Some to_string overloads for some Nsight Aftermath
// API types.
//

namespace tempest::rhi::vk::aftermath
{
    template <typename T>
    inline auto to_hex_string(T n) -> tempest::string
    {
        if constexpr (sizeof(T) == 8)
        {
            return tempest::format("{:016x}", static_cast<uint64_t>(n));
        }
        else if constexpr (sizeof(T) == 4)
        {
            return tempest::format("{:08x}", static_cast<uint32_t>(n));
        }
        else if constexpr (sizeof(T) == 2)
        {
            return tempest::format("{:04x}", static_cast<uint16_t>(n));
        }
        else
        {
            return tempest::format("{:02x}", static_cast<uint8_t>(n));
        }
    }

    inline auto to_string(GFSDK_Aftermath_Result result) -> tempest::string
    {
        return tempest::format("{:#x}", static_cast<uint32_t>(result));
    }

    inline auto to_string(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier) -> tempest::string
    {
        return tempest::format("{}-{}", to_hex_string(identifier.id[0]), to_hex_string(identifier.id[1]));
    }

    inline auto to_string(const GFSDK_Aftermath_ShaderBinaryHash& hash) -> tempest::string
    {
        return to_hex_string(hash.hash);
    }
} // namespace tempest::rhi::vk::aftermath

//*********************************************************
// Helper for comparing shader hashes and debug info identifier.
//

// Helper for comparing GFSDK_Aftermath_ShaderDebugInfoIdentifier.
inline bool operator<(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& lhs,
                      const GFSDK_Aftermath_ShaderDebugInfoIdentifier& rhs)
{
    if (lhs.id[0] == rhs.id[0])
    {
        return lhs.id[1] < rhs.id[1];
    }
    return lhs.id[0] < rhs.id[0];
}

// Helper for comparing GFSDK_Aftermath_ShaderBinaryHash.
inline bool operator<(const GFSDK_Aftermath_ShaderBinaryHash& lhs, const GFSDK_Aftermath_ShaderBinaryHash& rhs)
{
    return lhs.hash < rhs.hash;
}

// Helper for comparing GFSDK_Aftermath_ShaderDebugName.
inline bool operator<(const GFSDK_Aftermath_ShaderDebugName& lhs, const GFSDK_Aftermath_ShaderDebugName& rhs)
{
    return strncmp(lhs.name, rhs.name, sizeof(lhs.name)) < 0;
}

//*********************************************************
// Helper for checking Nsight Aftermath failures.
//

inline auto AftermathErrorMessage(GFSDK_Aftermath_Result result) -> tempest::string
{
    switch (result)
    {
    case GFSDK_Aftermath_Result_FAIL_DriverVersionNotSupported:
        return "Unsupported driver version - requires an NVIDIA R495 display driver or newer.";
    default:
        return tempest::format("Aftermath Error {:#x}", static_cast<uint32_t>(result));
    }
}

// Helper macro for checking Nsight Aftermath results and throwing exception
// in case of a failure.
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#define AFTERMATH_CHECK_ERROR(FC)                                                                                      \
    [&]() {                                                                                                            \
        GFSDK_Aftermath_Result _result = FC;                                                                           \
        if (!GFSDK_Aftermath_SUCCEED(_result))                                                                         \
        {                                                                                                              \
            MessageBoxA(0, AftermathErrorMessage(_result).c_str(), "Aftermath Error", MB_OK);                          \
            exit(1);                                                                                                   \
        }                                                                                                              \
    }()
#else
#define AFTERMATH_CHECK_ERROR(FC)                                                                                      \
    [&]() {                                                                                                            \
        GFSDK_Aftermath_Result _result = FC;                                                                           \
        if (!GFSDK_Aftermath_SUCCEED(_result))                                                                         \
        {                                                                                                              \
            printf("%s\n", AftermathErrorMessage(_result).c_str());                                                    \
            fflush(stdout);                                                                                            \
            exit(1);                                                                                                   \
        }                                                                                                              \
    }()
#endif

#endif // !tempest_rhi_vk_aftermath_helpers_hpp