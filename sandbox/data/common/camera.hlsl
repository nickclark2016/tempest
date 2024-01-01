#ifndef tempest_common_camera_hlsl
#define tempest_common_camera_hlsl

struct CameraData {
    float4x4 proj_matrix;
    float4x4 view_matrix;
    float4x4 view_proj_matrix;
    float3 position;
    float _pad0;
};

#endif // tempest_common_camera_hlsl